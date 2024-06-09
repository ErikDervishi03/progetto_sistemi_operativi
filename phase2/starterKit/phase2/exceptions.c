#include "dep.h"

void saveState(state_t* dest, state_t* to_copy) {
    dest->entry_hi = to_copy->entry_hi;
    dest->cause = to_copy->cause;
    dest->status = to_copy->status;
    dest->pc_epc = to_copy->pc_epc;
    for(int i = 0; i < STATE_GPR_LEN; i++) 
        dest->gpr[i] = to_copy->gpr[i];
    dest->hi = to_copy->hi;
    dest->lo = to_copy->lo;
}

int send(pcb_t *sender, pcb_t *dest, unsigned int payload) {
    msg_t *msg = allocMsg();
    if(!msg)
        return MSGNOGOOD;
    msg->m_sender = sender;
    msg->m_payload = payload;
    insertMessage(&(dest->msg_inbox), msg); 
    return 0;
}

static void passUpOrDie(int i, state_t *exception_state) {
    if(current_process) {
        if(current_process->p_supportStruct != NULL) {
            saveState(&(current_process->p_supportStruct->sup_exceptState[i]), exception_state);
            LDCXT(current_process->p_supportStruct->sup_exceptContext[i].stackPtr,        
                  current_process->p_supportStruct->sup_exceptContext[i].status,
                  current_process->p_supportStruct->sup_exceptContext[i].pc);
        } else {
            ssi_terminate_process(current_process);
            scheduler();
        }
    }
}

static int findPCB(pcb_t* p, struct list_head *list) {
    pcb_t *tmp;
    list_for_each_entry(tmp, list, p_list) {
        if(p == tmp)
            return 1;
    }
    return 0;
}

static void syscallExceptionHandler(state_t *exception_state) {
    if((exception_state->status << 30) >> 31) {
        exception_state->cause = (exception_state->cause & CLEAREXECCODE) | (EXC_RI << CAUSESHIFT);
        passUpOrDie(GENERALEXCEPT, exception_state);
    } else {
        if(exception_state->reg_a0 == SENDMESSAGE) {
            int nogood;
            int ready;
            int not_exists;
            pcb_t *dest = (pcb_t *)(exception_state->reg_a1);
            ready = findPCB(dest, &ready_queue);
            not_exists = findPCB(dest, &pcbFree_h);

            if(not_exists) 
                exception_state->reg_v0 = DEST_NOT_EXIST;  
            else if(ready || dest == current_process) {
                nogood = send(current_process, dest, exception_state->reg_a2);
                exception_state->reg_v0 = nogood;
            } else {
                nogood = send(current_process, dest, exception_state->reg_a2);
                insertProcQ(&ready_queue, dest);
                exception_state->reg_v0 = nogood;
            }

            exception_state->pc_epc += WORDLEN;
            LDST(exception_state);
        } else if(exception_state->reg_a0 == RECEIVEMESSAGE) {
            struct list_head *msg_inbox = &(current_process->msg_inbox);
            unsigned int from = exception_state->reg_a1;
            msg_t *msg = popMessage(msg_inbox, (from == ANYMESSAGE ? NULL : (pcb_t *)(from)));

            if(!msg) {
                saveState(&(current_process->p_s), exception_state);
                getDeltaTime(current_process);
                scheduler();
            } else {
                exception_state->reg_v0 = (memaddr)(msg->m_sender);
                if(msg->m_payload != (unsigned int)NULL) {
                    unsigned int *a2 = (unsigned int *)exception_state->reg_a2;
                    *a2 = msg->m_payload;
                }
                freeMsg(msg);
                exception_state->pc_epc += WORDLEN;
                LDST(exception_state);
            }
        } else if(exception_state->reg_a0 >= 1) {
            passUpOrDie(GENERALEXCEPT, exception_state);
        }
    }
}

void exceptionHandler() {
    state_t *exception_state = (state_t *)BIOSDATAPAGE;
    int cause = getCAUSE();

    switch((cause & GETEXECCODE) >> CAUSESHIFT) {
        case IOINTERRUPTS:
            interruptHandler(cause, exception_state);
            break;
        case 1 ... 3:
            passUpOrDie(PGFAULTEXCEPT, exception_state);
            break;
        case SYSEXCEPTION:
            syscallExceptionHandler(exception_state);
            break;
        case 4 ... 7:
            passUpOrDie(GENERALEXCEPT, exception_state);
            break;
        case 9 ... 12:
            passUpOrDie(GENERALEXCEPT, exception_state);
            break;
        default: 
            PANIC();
    }
}
