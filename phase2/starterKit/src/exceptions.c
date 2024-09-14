#include "dep.h"

 int isPCBInList(pcb_t* pcb, struct list_head *list) {
    pcb_t *temp;
    list_for_each_entry(temp, list, p_list) {
        if (pcb == temp)
            return 1;
    }
    return 0;
}


// se una syscall è chiamata da un processo che non è in kernel mode (quindi user-mode) causa una Trap

// Program Trap Handler
void handleException(int exceptionType, state_t *exceptionState) {
    if (current_process) {
        // se il processo ha un supporto strutturale
        if (current_process->p_supportStruct != NULL) {
            // salvo lo stato dell'eccezione
            saveState(&(current_process->p_supportStruct->sup_exceptState[exceptionType]), exceptionState);
            // salvo lo stato del processore
            LDCXT(current_process->p_supportStruct->sup_exceptContext[exceptionType].stackPtr,        
                  current_process->p_supportStruct->sup_exceptContext[exceptionType].status,
                  current_process->p_supportStruct->sup_exceptContext[exceptionType].pc);
        } else {
            // se non ha un supporto strutturale
            // termino il processo e richiamo lo scheduler
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


// una Syscall Exception avviene quando l'istruzione assembly syscall viene eseguita
// il processo in esecuzione carica valori apporpriati nei registri a0, a1, a2, a3, prima di esequire la syscall
// il kernel in base al valore trovato in a0 esegue una delle seguenti operazioni:
void syscallHandler(state_t *exceptionState) {
    if ((exceptionState->status << 30) >> 31) {
        exceptionState->cause = (exceptionState->cause & CLEAREXECCODE) | (EXC_RI << CAUSESHIFT);
        handleException(GENERALEXCEPT, exceptionState);
    } else {
        // trasmissione di un messaggio ad un processo (send asincrona)
        if (exceptionState->reg_a0 == SENDMESSAGE) {
            int msgResult;
            int isReady;
            int doesNotExist;
            // destinatario salvato nel registro a1
            pcb_t *destProc = (pcb_t *)(exceptionState->reg_a1);
            // se il destinatario è nella ready queue
            isReady = isPCBInList(destProc, &ready_queue);
            // se il destinatario è nella lista dei processi liberi
            doesNotExist = isPCBInList(destProc, &pcbFree_h);
            
            // se è nella lista dei processi liberi
            if (doesNotExist) {
                exceptionState->reg_v0 = DEST_NOT_EXIST;
            } else {
                // significa che esiste e quindi mando messaggio (a2)
                msgResult = sendMessage(current_process, destProc, exceptionState->reg_a2);
                // se il processo è in ready queue ed al momento è quello in esecuzione scrivo 0 in v0
                if (isReady || destProc == current_process) {
                    exceptionState->reg_v0 = msgResult;
                } else {
                    // se il processo non è in ready queue e sta aspettando un messaggio allora lo sveglio
                    insertProcQ(&ready_queue, destProc);
                    exceptionState->reg_v0 = msgResult;
                }
            }

            exceptionState->pc_epc += WORDLEN; // incremento il program counter, sennò il controllo torna al processo che ha chiamato la syscall e si va in loop
            LDST(exceptionState); // carico il nuovo stato

        } // è una receive sincrona (aspetta se non ha ricevuto messaggi) 
        else if (exceptionState->reg_a0 == RECEIVEMESSAGE) {
            // mittente salvato nel registro a1
            unsigned int fromProc = exceptionState->reg_a1;
            // ricavo la lista dei messaggi in arrivo
            struct list_head *inbox = &(current_process->msg_inbox);
            // prendo il messaggio spedito dal mittente
            msg_t *message = popMessage(inbox, (fromProc == ANYMESSAGE ? NULL : (pcb_t *)(fromProc)));
            
            // se non c'è nessun messaggio
            if (!message) {
                // deve aspettare, quindi salva lo stato e richiama lo scheduler
                saveState(&(current_process->p_s), exceptionState);
                getDeltaTime(current_process);
                scheduler();
            } else {
                // se c'è un messaggio
                // salvo il mittente nel registro v0
                exceptionState->reg_v0 = (memaddr)(message->m_sender);
                if (message->m_payload != (unsigned int)NULL) {
                    // salvo il payload del messaggio nel registro a2
                    unsigned int *reg_a2_ptr = (unsigned int *)exceptionState->reg_a2;
                    *reg_a2_ptr = message->m_payload;
                }
                // libero il messaggio
                freeMsg(message);
                // incremento il program counter, sennò il controllo torna al processo che ha chiamato la syscall e si va in loop
                exceptionState->pc_epc += WORDLEN;
                // carico il nuovo stato
                LDST(exceptionState);
            }
        } else if (exceptionState->reg_a0 >= 1) {
            handleException(GENERALEXCEPT, exceptionState); // se non è nè send nè receive
        }
    }
}

// funzione che viene passa nel passup vector nella fase di inizializzazione, 
// viene chiamata quando si verifica un interrupt o un eccezione
// il processor state al tempo dell'exception sarà salvato all'inizio del BIOS Data Page (0x0FFFF000)
// la causa dell'exception viene decodificata nel campo .ExcCode del registro Cause (Cause.ExcCode)

void exceptionHandler() {
    state_t *currentExceptionState = (state_t *)BIOSDATAPAGE;
    int currentCause = getCAUSE(); // per prendere l'ExcCode uso getCause()
    int exceptionCode = (currentCause & GETEXECCODE) >> CAUSESHIFT; // ricavo il codice dell'exception

    switch (exceptionCode) {
        case IOINTERRUPTS:
            interruptHandler(currentCause, currentExceptionState); // passo il controllo all'interrupt handler
            break;
        case 1 ... 3:
            handleException(PGFAULTEXCEPT, currentExceptionState); // passo il controllo al TLB exception handler
            break;
        case SYSEXCEPTION:
            syscallHandler(currentExceptionState); // passo il controllo al syscall handler
            break;
        case 4 ... 7: case 9 ... 12:
            handleException(GENERALEXCEPT, currentExceptionState); // passo il controllo al Program Trap handler
            break;
        default:
            PANIC();
            break;
    }
}



 