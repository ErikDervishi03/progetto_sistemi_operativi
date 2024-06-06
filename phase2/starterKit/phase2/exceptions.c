#include "const.h"
#include "dep.h"
#include "headers/msg.h"
#include <umps3/umps/libumps.h>

void passupOrDie(int index)
{
    if (current_process == NULL || current_process->p_supportStruct == NULL )
    {
        terminate_process(current_process);
        scheduler ();
    }
    else
    {
        state_t *p_state = (state_t *)BIOSDATAPAGE;
        current_process->p_supportStruct->sup_exceptState[index] = *p_state;

        context_t ctx = current_process->p_supportStruct->sup_exceptContext[index];
        LDCXT (ctx.stackPtr, ctx.status, ctx.pc);
    }

}

static int isProcessInList(pcb_t* p, struct list_head *list) {
    pcb_t *current;
    list_for_each_entry(current, list, p_list){
        if(p == current)
            return 1; 
    }
    return 0;    
}

int send(pcb_t *sender, pcb_t *dest, unsigned int payload) {
    msg_t *msg = allocMsg();
    if(!msg) //non ci sono più messaggi disponibili
        return MSGNOGOOD;

    msg->m_sender = sender;
    msg->m_payload = payload;
    insertMessage(&(dest->msg_inbox), msg); 
    return 0;
}

int isInList(struct list_head *target_process, int pid) {
  pcb_PTR tmp;
  list_for_each_entry(tmp, target_process, p_list) {
    if (tmp->p_pid == pid)
      return TRUE;
  }
  return FALSE;
}

int isFree(int p_pid){
    return isInList(&pcbFree_h, p_pid);
}


void systemcallHandler() {
    //term_puts("dentro syscall handler\n");
    state_t *exception_state = (state_t *)BIOSDATAPAGE;
    if((exception_state->status << 30) >> 31) { //controllo che il processo non sia in kernel-mode
      //scrivo come codice dell'eccezione il valore EXC_RI e invoco il Pass Up or Die
      exception_state->cause = (exception_state->cause & CLEAREXECCODE) | (EXC_RI << CAUSESHIFT);
      passupOrDie(GENERALEXCEPT);
    }
    else {
        //SYS1  
        if(exception_state->reg_a0 == SENDMESSAGE) {
            term_puts("entrato nella send\n");
            if (exception_state->reg_a1 == 0) {
                exception_state->reg_a0 = DEST_NOT_EXIST;
                return;
            }

            pcb_t *dest_process = (pcb_PTR)exception_state->reg_a1;

            if (isFree(dest_process->p_pid)) { // error!
                exception_state->reg_a0 = DEST_NOT_EXIST;
                return;
            }

            // push message
            msg_t *msg = allocMsg();
            if (msg == NULL) {
                exception_state->reg_a0 = MSGNOGOOD;
                return;
            }

            msg->m_payload = (unsigned)exception_state->reg_a2;
            msg->m_sender = current_process;

            if (outProcQ(&msg_queue_list, dest_process) != NULL) {
                // process is blocked waiting for a message, i unblock it
                insertProcQ(&ready_queue, dest_process);
                softBlockCount--;
            }

            insertMessage(&dest_process->msg_inbox, msg);
            exception_state->reg_a0 = 0;

            exception_state->pc_epc += WORDLEN; //non bloccante
            LDST(exception_state);

        }
        //SYS2
        else if(exception_state->reg_a0 == RECEIVEMESSAGE) {
                term_puts("entro in receive\n");
                pcb_t *sender = (pcb_PTR)exception_state->reg_a1;

                // if the sender is NULL, then the process is looking for the first
                msg_t * msg = popMessage(&current_process->msg_inbox, sender);

                // there is no correct message in the inbox, need to be frozen.
                if (msg == NULL) {
                    term_puts("receive bloccante\n");
                    // i can assume the process is in running state
                    insertProcQ(&msg_queue_list, current_process);
                    softBlockCount++;
                    // save the processor state
                    *exception_state = current_process->p_s;
                    // update the accumulated CPU time for the Current Process
                    current_process->p_time += getTimeElapsed();
                    // get the next process
                    scheduler();
                    return;
                }

                /*The saved processor state (located at the start of the BIOS Data
                Page [Section 3]) must be copied into the Current Process’s PCB
                (p_s)*/
                /*This system call provides as returning value (placed in caller’s v0 in
                µMPS3) the identifier of the process which sent the message extracted.
                +payload in stored in a2*/
                exception_state->reg_a0 = (unsigned)msg->m_sender;

                // write the message's payload in the location signaled in the a2
                // register.
                if (exception_state->reg_a2 != 0) {
                    // has a payload
                    *((unsigned *)exception_state->reg_a2) = (unsigned)msg->m_payload;
                }
                exception_state->pc_epc += WORDLEN; //non bloccante
                LDST(exception_state);
        }
        //valore registro a0 non corretto
        else if(exception_state->reg_a0 >= 1) {
            passupOrDie(GENERALEXCEPT);
        }
    }
}



void exceptionHandler ()
{
    //term_puts("dentro l'exception handler\n");

    switch (CAUSE_GET_EXCCODE (getCAUSE ()))
    {
    case 0:
        interruptHandler(); 
        break;
    case 1:
    case 2:
    case 3:
        passupOrDie(PGFAULTEXCEPT);
        break;    
    case 8:
        systemcallHandler();
        break;
    default:
        passupOrDie(GENERALEXCEPT);
        break;
    }
}


