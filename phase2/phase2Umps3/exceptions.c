#include "dep.h"
#include <stdlib.h>

void receiveMessage(pid_t *sender, void *payload);
void sendMessage(pid_t destination, void *payload);

void passupOrDie(int index)
{
    if (currentProcess->p_supportStruct == NULL)
    {
        while(removeProcQ(&currentProcess->p_child));
        while(removeProcQ(&currentProcess->p_sib));
        free(currentProcess);
        currentProcess = NULL;
        scheduler ();
    }
    else
    {
        currentProcess->p_supportStruct->sup_exceptState[index] = *(state_t*)BIOSDATAPAGE;
        context_t ctx = currentProcess->p_supportStruct->sup_exceptContext[index];
        LDCXT (ctx.stackPtr, ctx.status, ctx.pc);
    }

}

void systemcallHandler() {
    // Controllo se la chiamata di sistema è stata eseguita in modalità utente
    if ((getSTATUS() & USERPON) != 0) {
        // Genera un'eccezione Program Trap
        setCAUSE((getCAUSE() & ~CAUSE_EXCCODE_MASK) | (Cause.ExeCode << CAUSE_EXCCODE_BIT));
        passupOrDie(GENERALEXCEPT);
    }

    ((state_t *)BIOSDATAPAGE)->pc_epc += WORD_SIZE; // Incremento del PC per chiamate di sistema non bloccanti

    switch (GPR(a0)) {
        case SENDMESSAGE:
            // Codice per inviare un messaggio (SYS1)
            sendMessage((pid_t)GPR(a1), (void *)GPR(a2));
            SYSCALL(SENDMESSAGE, (unsigned int)destination, (unsigned int)payload, 0);          
            break;
        case RECEIVEMESSAGE:
            // Codice per ricevere un messaggio (SYS2)
            receiveMessage((pid_t)GPR(a1), (void *)GPR(a2));
            SYSCALL(RECEIVEMESSAGE, (unsigned int)sender, (unsigned int)payload, 0);
            break;
        default:
            // Genera un'eccezione Program Trap per chiamate di sistema sconosciute
            setCAUSE((getCAUSE() & ~CAUSE_EXCCODE_MASK) | (EXC_RI << CAUSE_EXCCODE_BIT));
            passupOrDie(GENERALEXCEPT);
    }

    // Restituisce il controllo al processo interrotto
    LDST((state_t*)BIOSDATAPAGE);
}

void receiveMessage(pid_t *sender, void *payload) {
    // Controllo se il processo è bloccato
    if (currentProcess->p_state != BLOCKED) {
        // Genera un'eccezione Program Trap
        setCAUSE((getCAUSE() & ~CAUSE_EXCCODE_MASK) | (Cause.ExeCode << CAUSE_EXCCODE_BIT));
        passupOrDie(GENERALEXCEPT);
    }

    // Controllo se il processo ha un messaggio in attesa
    if (currentProcess->p_msg == NULL) {
        // Blocca il processo
        currentProcess->p_state = BLOCKED;
        insertProcQ(&blockedQueue, currentProcess);
        // Restituisce il controllo al processo interrotto
        LDST((state_t*)BIOSDATAPAGE);
    } else {
        // Copia il mittente e il payload del messaggio
        *sender = currentProcess->p_msg->sender;
        *payload = currentProcess->p_msg->payload;

        // Rimuove il messaggio dalla coda
        free(currentProcess->p_msg);
        currentProcess->p_msg = NULL;

        // Sblocca il processo
        currentProcess->p_state = READY;
        insertProcQ(&readyQueue, currentProcess);

        // Restituisce il controllo al processo interrotto
        LDST(state_t*BIOSDATAPAGE);
    }
}
void sendMessage(pid_t destination, void *payload) {
    // Check if the process is blocked
    if (currentProcess->p_state != BLOCKED) {
        setCAUSE((getCAUSE() & ~CAUSE_EXCCODE_MASK) | (Cause.ExeCode << CAUSE_EXCCODE_BIT));
        passupOrDie(GENERALEXCEPT);
    }

    // Check if the destination process exists
    pcb_t *receiver = searchProcQ(&blockedQueue, destination);
    if (receiver == NULL) {
        // Set return register to DEST_NOT_EXIST
        GPR(v0) = DEST_NOT_EXIST;
        return;
    }

    // Create a new message
    msg_t *message = (msg_t *)malloc(sizeof(msg_t));
    message->sender = currentProcess->p_pid;
    message->payload = payload;

    // Insert the message into the receiver's inbox
    receiver->p_msg = message;

    // Check if the receiver is waiting for a message
    if (receiver->p_state == BLOCKED) {
        // Awake the receiver and put it into the Ready Queue
        receiver->p_state = READY;
        insertProcQ(&readyQueue, receiver);
    }

    // Set return register to 0 (success)
    GPR(v0) = 0;
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
        passupordie(GENERALEXCEPT);
        break;
    }
}



