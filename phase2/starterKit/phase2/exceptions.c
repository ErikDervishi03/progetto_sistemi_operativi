#include "dep.h"
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

void systemcallHandler() {
    // Controllo se la chiamata di sistema è stata eseguita in modalità utente
    if ((getSTATUS() & USERPON) != 0) {
        setCAUSE ((getCAUSE () & ~CAUSE_EXCCODE_MASK) | (EXC_RI << CAUSE_EXCCODE_BIT));
        passupOrDie(GENERALEXCEPT);
    }

    ((state_t *)BIOSDATAPAGE)->pc_epc += WORD_SIZE; // Incremento del PC per chiamate di sistema non bloccanti

    switch (current_process->p_s.reg_a0) {
        case SENDMESSAGE:
            SYSCALL(SENDMESSAGE, current_process->p_s.reg_a1, current_process->p_s.reg_a2, 0); 
        break;
        case RECEIVEMESSAGE:
            SYSCALL(RECEIVEMESSAGE, current_process->p_s.reg_a1, current_process->p_s.reg_a2, 0);
            current_process->p_s.pc_epc = ((state_t *)BIOSDATAPAGE)->pc_epc;
            // current_process->p_time += getTimeElapsed(); ma questo lo faccio già nello scheduler 
            scheduler();
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



