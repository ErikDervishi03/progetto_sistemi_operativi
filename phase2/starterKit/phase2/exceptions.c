#include "dep.h"

 int isPCBInList(pcb_t* pcb, struct list_head *list) {
    pcb_t *temp;
    list_for_each_entry(temp, list, p_list) {
        if (pcb == temp)
            return 1;
    }
    return 0;
}

void handleException(int exceptionType, state_t *exceptionState) {
    if (current_process) {
        if (current_process->p_supportStruct != NULL) {
            saveState(&(current_process->p_supportStruct->sup_exceptState[exceptionType]), exceptionState);
            LDCXT(current_process->p_supportStruct->sup_exceptContext[exceptionType].stackPtr,        
                  current_process->p_supportStruct->sup_exceptContext[exceptionType].status,
                  current_process->p_supportStruct->sup_exceptContext[exceptionType].pc);
        } else {
            ssi_terminate_process(current_process);
            scheduler();
        }
    }
}

void saveState(state_t* destination, state_t* source) {
    destination->entry_hi = source->entry_hi;
    destination->pc_epc = source->pc_epc;
    destination->hi = source->hi;
    destination->lo = source->lo;
    destination->cause = source->cause;
    destination->status = source->status;
    
    for (int i = 0; i < STATE_GPR_LEN; i++) 
        destination->gpr[i] = source->gpr[i];
}


int sendMessage(pcb_t *sender, pcb_t *receiver, unsigned int data) {
    msg_t *message = allocMsg();
    if (!message)
        return MSGNOGOOD;
    message->m_payload = data;
    message->m_sender = sender;
    insertMessage(&(receiver->msg_inbox), message); 
    return 0;
}

void syscallHandler(state_t *exceptionState) {
    if ((exceptionState->status << 30) >> 31) {
        exceptionState->cause = (exceptionState->cause & CLEAREXECCODE) | (EXC_RI << CAUSESHIFT);
        handleException(GENERALEXCEPT, exceptionState);
    } else {
        if (exceptionState->reg_a0 == SENDMESSAGE) {
            int msgResult;
            int isReady;
            int doesNotExist;
            pcb_t *destProc = (pcb_t *)(exceptionState->reg_a1);
            isReady = isPCBInList(destProc, &ready_queue);
            doesNotExist = isPCBInList(destProc, &pcbFree_h);

            if (doesNotExist) {
                exceptionState->reg_v0 = DEST_NOT_EXIST;
            } else {
                msgResult = sendMessage(current_process, destProc, exceptionState->reg_a2);
                if (isReady || destProc == current_process) {
                    exceptionState->reg_v0 = msgResult;
                } else {
                    insertProcQ(&ready_queue, destProc);
                    exceptionState->reg_v0 = msgResult;
                }
            }

            exceptionState->pc_epc += WORDLEN;
            LDST(exceptionState);
        } else if (exceptionState->reg_a0 == RECEIVEMESSAGE) {
            unsigned int fromProc = exceptionState->reg_a1;
            struct list_head *inbox = &(current_process->msg_inbox);
            msg_t *message = popMessage(inbox, (fromProc == ANYMESSAGE ? NULL : (pcb_t *)(fromProc)));

            if (!message) {
                saveState(&(current_process->p_s), exceptionState);
                getDeltaTime(current_process);
                scheduler();
            } else {
                exceptionState->reg_v0 = (memaddr)(message->m_sender);
                if (message->m_payload != (unsigned int)NULL) {
                    unsigned int *reg_a2_ptr = (unsigned int *)exceptionState->reg_a2;
                    *reg_a2_ptr = message->m_payload;
                }
                freeMsg(message);
                exceptionState->pc_epc += WORDLEN;
                LDST(exceptionState);
            }
        } else if (exceptionState->reg_a0 >= 1) {
            handleException(GENERALEXCEPT, exceptionState);
        }
    }
}

void exceptionHandler() {
    state_t *currentExceptionState = (state_t *)BIOSDATAPAGE;
    int currentCause = getCAUSE();
    int exceptionCode = (currentCause & GETEXECCODE) >> CAUSESHIFT;

    switch (exceptionCode) {
        case IOINTERRUPTS:
            interruptHandler(currentCause, currentExceptionState);
            break;
        case 1 ... 3:
            handleException(PGFAULTEXCEPT, currentExceptionState);
            break;
        case SYSEXCEPTION:
            syscallHandler(currentExceptionState);
            break;
        case 4 ... 7: case 9 ... 12:
            handleException(GENERALEXCEPT, currentExceptionState);
            break;
        default:
            PANIC();
            break;
    }
}



 