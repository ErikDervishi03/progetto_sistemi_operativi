#include "const.h"
#include "dep.h"
#include "headers/msg.h"
#include <umps3/umps/libumps.h>

// Function to pass up or terminate the process
void passupOrDie(int index) {
    if (current_process == NULL || current_process->p_supportStruct == NULL) {
        terminate_process(current_process);
        scheduler();
    } else {
        state_t *p_state = (state_t *)BIOSDATAPAGE;
        current_process->p_supportStruct->sup_exceptState[index] = *p_state;

        context_t ctx = current_process->p_supportStruct->sup_exceptContext[index];
        LDCXT(ctx.stackPtr, ctx.status, ctx.pc);
    }
}

// Check if a process is in the list
static int isProcessInList(pcb_t* p, struct list_head *list) {
    pcb_t *current;
    list_for_each_entry(current, list, p_list) {
        if (p == current)
            return 1; 
    }
    return 0;    
}

// Handle system calls
void systemcallHandler() {
    state_t *exception_state = (state_t *)BIOSDATAPAGE;

    // Ensure the process is in kernel mode
    if ((exception_state->status << 30) >> 31) {
        exception_state->cause = (exception_state->cause & CLEAREXECCODE) | (EXC_RI << CAUSESHIFT);
        passupOrDie(GENERALEXCEPT);
    } else {
        switch (exception_state->reg_a0) {
            case SENDMESSAGE: {
                if (exception_state->reg_a1 == 0) {
                    exception_state->reg_v0 = DEST_NOT_EXIST;
                    return;
                }

                pcb_t *dest_process = (pcb_PTR)exception_state->reg_a1;

                if (isProcessInList(dest_process, &pcbFree_h)) { 
                    exception_state->reg_v0 = DEST_NOT_EXIST;
                    return;
                }

                msg_t *msg = allocMsg();
                if (msg == NULL) {
                    exception_state->reg_v0 = MSGNOGOOD;
                    return;
                }

                msg->m_payload = (unsigned)exception_state->reg_a2;
                msg->m_sender = current_process;

                if (outProcQ(&msg_queue_list, dest_process) != NULL) {
                    insertProcQ(&ready_queue, dest_process);
                    softBlockCount--;
                }

                insertMessage(&dest_process->msg_inbox, msg);
                exception_state->reg_v0 = 0;
                exception_state->pc_epc += WORDLEN;
                LDST(exception_state);
                break;
            }
            case RECEIVEMESSAGE: {
                pcb_t *sender = (pcb_PTR)exception_state->reg_a1;

                msg_t *msg = popMessage(&current_process->msg_inbox, sender);

                if (msg == NULL) {
                    insertProcQ(&msg_queue_list, current_process);
                    softBlockCount++;
                    current_process->p_s = *exception_state;
                    current_process->p_time += getTimeElapsed();
                    scheduler();
                    return;
                }

                exception_state->reg_v0 = (unsigned)msg->m_sender;

                if (exception_state->reg_a2 != 0) {
                    *((unsigned *)exception_state->reg_a2) = (unsigned)msg->m_payload;
                }

                freeMsg(msg);
                exception_state->pc_epc += WORDLEN; 
                LDST(exception_state);
                break;
            }
            default: {
                if (exception_state->reg_a0 >= 1) {
                    passupOrDie(GENERALEXCEPT);
                }
                break;
            }
        }
    }
}

// Handle exceptions
void exceptionHandler() {
    STCK(prevTOD);

    switch (CAUSE_GET_EXCCODE(getCAUSE())) {
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
