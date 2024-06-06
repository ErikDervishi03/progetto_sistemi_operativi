#include "const.h"
#include "dep.h"
#include "headers/msg.h"
#include <umps3/umps/libumps.h>

 cpu_t time_interrupt_start;
#define BIT_CHECKER(n, bit) (((n) >> (bit)) & 1)
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
            int nogood;     //valore di ritorno da inserire in reg_v0
            int ready;      //se uguale a 1 il destinatario è nella ready queue, 0 altrimenti
            int not_exists; //se uguale a 1 il destinatario è nella lista pcbFree_h
            pcb_t *dest = (pcb_t *)(exception_state->reg_a1);
            ready = isProcessInList(dest, &ready_queue);
            not_exists = isProcessInList(dest, &pcbFree_h);

            if(not_exists) {
                exception_state->reg_v0 = DEST_NOT_EXIST;  
            }
            else if(ready || dest == current_process) {
                nogood = send(current_process, dest, exception_state->reg_a2);
                exception_state->reg_v0 = nogood;
            }
            else {
                nogood = send(current_process, dest, exception_state->reg_a2);
                insertProcQ(&ready_queue, dest);   //il processo era bloccato quindi lo inserisco sulla ready queue
                exception_state->reg_v0 = nogood;
            }

            exception_state->pc_epc += WORDLEN; //non bloccante
            LDST(exception_state);
        }
        //SYS2
        else if(exception_state->reg_a0 == RECEIVEMESSAGE) {
            struct list_head *msg_inbox = &(current_process->msg_inbox);                        //inbox del ricevente
            unsigned int from = exception_state->reg_a1;                                        //da chi voglio ricevere
            msg_t *msg = popMessage(msg_inbox, (from == ANYMESSAGE ? NULL : (pcb_t *)(from)));  //rimozione messaggio dalla inbox del ricevente
            
            if(!msg) { 
                //receive bloccante
                current_process->p_s = *exception_state;
                current_process->p_time += getTimeElapsed();
                term_puts("receive bloccante chiamo lo scheduler\n");
                scheduler();       
            }
            else {
                //receive non bloccante
                exception_state->reg_v0 = (memaddr)(msg->m_sender);
                if(msg->m_payload != (unsigned int)NULL) {
                    //accedo all'area di memoria in cui andare a caricare il payload del messaggio
                    unsigned int *a2 = (unsigned int *)exception_state->reg_a2;
                    *a2 = msg->m_payload;
                }
                freeMsg(msg); //il messaggio non serve più, lo libero
                exception_state->pc_epc += WORDLEN; //non bloccante
                LDST(exception_state);
            }
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
    STCK(time_interrupt_start);
    // error code from .ExcCode field of the Cause register
    unsigned status = getSTATUS();
    unsigned cause = getCAUSE();
    unsigned exception_code = cause & 0x7FFFFFFF; // 0311 1111 x 32

    unsigned is_interrupt_enabled = BIT_CHECKER(status, 7);
    unsigned is_interrupt = BIT_CHECKER(cause, 31);

    state_t *exception_state = (state_t *)BIOSDATAPAGE;
    // check if interrupts first
    if (is_interrupt_enabled && is_interrupt) {
        interruptHandler();
        return;
    }

    switch (CAUSE_GET_EXCCODE (getCAUSE ()))
    {
    case 0:
        interruptHandler(); 
        break;
    case 1:
    case 2:
    case 3:
        passupOrDie(PGFAULTEXCEPT); //YL
        break;    
    case 8:
        systemcallHandler();
        break;
    default:
        passupOrDie(GENERALEXCEPT);
        break;
    }
}


