#include "const.h"
#include "dep.h"
#include "headers/msg.h"
#include <umps3/umps/libumps.h>

void passupOrDie(int index)
{
    if (current_process->p_supportStruct == NULL)
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

static int isProcessInList(pcb_t *process, struct list_head *list) {
    struct list_head *pos;
    pcb_t *current;
    list_for_each(pos, list) {
        current = container_of(pos, pcb_t, p_list);
        if (current == process) {
            return 1;
        }
    }
    return 0;
}

void systemcallHandler() {
    
    // Controllo se la chiamata di sistema è stata eseguita in modalità utente
    if ((getSTATUS() & USERPON) != 0) {
        setCAUSE ((getCAUSE () & ~CAUSE_EXCCODE_MASK) | (EXC_RI << CAUSE_EXCCODE_BIT));
        passupOrDie(GENERALEXCEPT);
    }
    
    ((state_t *)BIOSDATAPAGE)->pc_epc += WORD_SIZE; // Incremento del PC per chiamate di sistema non bloccanti

    switch (current_process->p_s.reg_a0) {
        case SENDMESSAGE:
            if(current_process == NULL)break;

            if (isProcessInList(current_process, &pcbFree_h)){
                //If the target process is in the pcbFree_h list, 
                //set the return register (v0 in μMPS3) to DEST_NOT_EXIST
                current_process->p_s.reg_v0 = DEST_NOT_EXIST;
            }
            else{
                pcb_t * destination = (pcb_t *)current_process->p_s.reg_a1;
                if(destination == NULL){
                    current_process->p_s.reg_v0 = MSGNOGOOD;
                    break;
                }

                unsigned int payload = current_process->p_s.reg_a2;
                msg_t *message = allocMsg();
                if(message == NULL){
                    current_process->p_s.reg_v0 = MSGNOGOOD;
                    break;
                }

                mkEmptyMessageQ(&message->m_list);//non sicuro di questo
                message->m_payload = payload;
                message->m_sender = current_process;

                insertMessage(&destination->msg_inbox, message);

                //n success returns/places 0 in the caller’s v0
                current_process->p_s.reg_v0 = 0;
            }
            
        break;
        case RECEIVEMESSAGE:
            {
                pcb_t *p_grantor = (current_process->p_s.reg_a1 == ANYMESSAGE) ? NULL : (pcb_t *)current_process->p_s.reg_a1;

                msg_t* message = popMessage(&current_process->msg_inbox, p_grantor);

                if(message == NULL){
                    scheduler();
                    // not sure, want to start the loop
                    SYSCALL(RECEIVEMESSAGE, current_process->p_s.reg_a1, current_process->p_s.reg_a2, 0);
                }else {
                    /*This system call provides as returning value (placed in caller’s 
                    v0 in μMPS3) the identifier of the process which sent the message extracted*/
                    current_process->p_s.reg_v0 = message->m_sender->p_pid;

                    current_process->p_s.reg_a2 = message->m_payload;
                }
            }
            break;
        default:
            // Genera un'eccezione Program Trap per chiamate di sistema sconosciute
            setCAUSE((getCAUSE() & ~CAUSE_EXCCODE_MASK) | (EXC_RI << CAUSE_EXCCODE_BIT));
            passupOrDie(GENERALEXCEPT);
    }
    
    // Restituisce il controllo al processo interrotto
    LDST((state_t*)BIOSDATAPAGE);
}


void exceptionHandler ()
{
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



