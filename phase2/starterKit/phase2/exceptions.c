#include "dep.h"
//funzione per copia dello stato
void saveState(state_t* dest, state_t* to_copy) {
    //copia di tutti i parametri di uno stato nell'altro
    dest->entry_hi = to_copy->entry_hi;
    dest->cause = to_copy->cause;
    dest->status = to_copy->status;
    dest->pc_epc = to_copy->pc_epc;
    for(int i = 0; i < STATE_GPR_LEN; i++) 
        dest->gpr[i] = to_copy->gpr[i];
    dest->hi = to_copy->hi;
    dest->lo = to_copy->lo;
}

/*codice che inizializza un messaggio e lo inserisce nell'inbox
del destinatario*/
int send(pcb_t *sender, pcb_t *dest, unsigned int payload) {
    msg_t *msg = allocMsg();
    if(!msg) //non ci sono più messaggi disponibili
        return MSGNOGOOD;

    msg->m_sender = sender;
    msg->m_payload = payload;
    insertMessage(&(dest->msg_inbox), msg); 
    return 0;
}

/*codice che implementa il Pass Up or Die per la gestione
delle trap*/
static void passUpOrDie(int i, state_t *exception_state) {
    if(current_process) { //per sicurezzza
        if(current_process->p_supportStruct != NULL) {
            saveState(&(current_process->p_supportStruct->sup_exceptState[i]), exception_state); //salvataggio stato 
            //caricamento nella CPU di stack pointer, status e program counter
            LDCXT(current_process->p_supportStruct->sup_exceptContext[i].stackPtr,        
                  current_process->p_supportStruct->sup_exceptContext[i].status,
                  current_process->p_supportStruct->sup_exceptContext[i].pc
            );
        }
        else {
            ssi_terminate_process(current_process);
            scheduler();
        }
    }
}

//funzione che ricerca un PCB all'interno di una lista 
static int findPCB(pcb_t* p, struct list_head *list) {
    pcb_t *tmp;
    list_for_each_entry(tmp, list, p_list){
        if(p == tmp)
            return 1; //trovato
    }
    return 0;     //non trovato
}

static void syscallExceptionHandler(state_t *exception_state){
    if((exception_state->status << 30) >> 31) { //controllo che il processo non sia in kernel-mode
      //scrivo come codice dell'eccezione il valore EXC_RI e invoco il Pass Up or Die
      exception_state->cause = (exception_state->cause & CLEAREXECCODE) | (EXC_RI << CAUSESHIFT);
      passUpOrDie(GENERALEXCEPT, exception_state);
    }
    else {
        //SYS1  
        if(exception_state->reg_a0 == SENDMESSAGE) {
            int nogood;     //valore di ritorno da inserire in reg_v0
            int ready;      //se uguale a 1 il destinatario è nella ready queue, 0 altrimenti
            int not_exists; //se uguale a 1 il destinatario è nella lista pcbFree_h
            pcb_t *dest = (pcb_t *)(exception_state->reg_a1);
            ready = findPCB(dest, &Ready_Queue);
            not_exists = findPCB(dest, &pcbFree_h);

            if(not_exists) 
                exception_state->reg_v0 = DEST_NOT_EXIST;  
            else if(ready || dest == current_process) {
                nogood = send(current_process, dest, exception_state->reg_a2);
                exception_state->reg_v0 = nogood;
            }
            else {
                nogood = send(current_process, dest, exception_state->reg_a2);
                insertProcQ(&Ready_Queue, dest);   //il processo era bloccato quindi lo inserisco sulla ready queue
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
                saveState(&(current_process->p_s), exception_state);
                updateCPUtime(current_process);
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
            passUpOrDie(GENERALEXCEPT, exception_state);
        }
    }
}

void exceptionHandler() {
	state_t *exception_state = (state_t *)BIOSDATAPAGE;
	int cause = getCAUSE();

  switch((cause & GETEXECCODE) >> CAUSESHIFT){
		  case IOINTERRUPTS:
			    interruptHandler(cause, exception_state);
			    break;
		  case 1 ... 3:
			    //TLB exceptions --> passo controllo al rispettivo gestore
          passUpOrDie(PGFAULTEXCEPT, exception_state);
			    break;
		  case SYSEXCEPTION:
          syscallExceptionHandler(exception_state);
			    break;
      case 4 ... 7:
          //Program traps --> passo controllo al rispettivo gestore
          passUpOrDie(GENERALEXCEPT, exception_state);
			    break;
      case 9 ... 12:
			    //Program traps --> passo controllo al rispettivo gestore
          passUpOrDie(GENERALEXCEPT, exception_state);
			    break;
		  default: 
          PANIC();
 }
}

/*void uTLB_RefillHandler() {
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST((state_t*) 0x0FFFF000);
}*/