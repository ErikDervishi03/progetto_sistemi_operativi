#include "include/sysSupport.h"
#include "include/vmSupport.h"
#include "testers/h/tconst.h"

/*
 * Gestore delle eccezioni di tipo "program trap" a livello utente,
 * che non riguardano le system call.
 */
void handleUserProgramTrap(state_t *exception_state) {
  if (current_process == mutex_holder) {
    // Rilascia il mutex se il processo corrente è il detentore
    SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex_process, 0, 0);
  }

  ssi_payload_t terminate_payload = {
      .service_code = TERMPROCESS,
      .arg = NULL,
  };
  // Invia un messaggio al PCB per terminare il processo corrente
  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&terminate_payload), 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, 0, 0);
}

/*
 * Gestore delle eccezioni causate dalle system call a livello di supporto.
 */
void handleSupportSyscallException(state_t *exception_state) {
  if (exception_state->reg_a0 == SENDMSG) {
    if (exception_state->reg_a1 == PARENT) {
      // Invia un messaggio dal processo utente al suo processo genitore
      SYSCALL(SENDMESSAGE, (unsigned int)current_process->p_parent, exception_state->reg_a2, 0);
    } else {
      // Invia un messaggio al processo specificato
      SYSCALL(SENDMESSAGE, exception_state->reg_a1, exception_state->reg_a2, 0);
    }
  } else if (exception_state->reg_a0 == RECEIVEMSG) {
    // Ricevi un messaggio dal processo specificato
    SYSCALL(RECEIVEMESSAGE, exception_state->reg_a1, exception_state->reg_a2, 0);
  }
}

/*
 * Gestore generale delle eccezioni a livello di supporto.
 */
void generalExceptionHandler() {
  support_t *support_struct;
  ssi_payload_t get_support_payload = {
      .service_code = GETSUPPORTPTR,
      .arg = NULL,
  };

  // Richiesta del puntatore alla struttura di supporto
  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&get_support_payload), 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&support_struct), 0);

  state_t *exception_state = &(support_struct->sup_exceptState[GENERALEXCEPT]);

  // Avanza il Program Counter (PC) per la prossima istruzione
  exception_state->pc_epc += WORDLEN;

  int exception_code = (exception_state->cause & GETEXECCODE) >> CAUSESHIFT;

  switch (exception_code) {
  case SYSEXCEPTION:
    handleSupportSyscallException(exception_state);
    break;

  default:
    handleUserProgramTrap(exception_state);
    break;
  }

  // Ripristina lo stato del processo e riprende l'esecuzione
  LDST(exception_state);
}
