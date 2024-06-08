#include "dep.h"

/*  Funzione che rimuove il processo p dalla ready queue e lo inserisce 
    nella corretta lista di processi bloccati    */
static void blockProcessOnDevice(pcb_t* p, int line, int term){
  outProcQ(&Ready_Queue, p);
  switch (line) {
    case IL_DISK:
        insertProcQ(&Locked_disk, p);
        break;
    case IL_FLASH:
        insertProcQ(&Locked_flash, p);
        break;
    case IL_ETHERNET:
        insertProcQ(&Locked_ethernet, p);
        break;
    case IL_PRINTER:
        insertProcQ(&Locked_printer, p);
        break;
    case IL_TERMINAL:
        insertProcQ(((term > 0) ? &Locked_terminal_transm : &Locked_terminal_recv ), p);
        break;
    default:
        break;
  }
}

/*  Funzione che, dato l'indirizzo passato alla richiesta DOIO,
    determina la corrispondente linea e il corrispondente numero di device.
    In caso di terminale imposta nella chiamata a blockProcessOnDevice() 
    il campo term a 0 per recv, a 1 per transm.   */
static void addrToDevice(memaddr command_address, pcb_t *p){
    //ciclo for che itera fra i dispositvi terminali
    for (int j = 0; j < 8; j++){ 
        //calcolo indirizzo di base
        termreg_t *base_address = (termreg_t *)DEV_REG_ADDR(7, j);
        if(command_address == (memaddr)&(base_address->recv_command) ){
            //inizializzazione del campo aggiuntivo del pcb
            p->dev_no = j;
            blockProcessOnDevice(p, 7, 0);
            return;
        }
        else if(command_address == (memaddr)&(base_address->transm_command)){
            //inizializzazione del campo aggiuntivo del pcb
            p->dev_no = j;
            blockProcessOnDevice(p, 7, 1);
            return;
        }
    }
    //ciclo for che itera fra tutti gli altri dispositivi
    for (int i = 3; i < 7; i++)
    {
        for (int j = 0; j < 8; j++)
        { 
            dtpreg_t *base_address = (dtpreg_t *)DEV_REG_ADDR(i, j);
            if(command_address == (memaddr)&(base_address->command) ){
                //inizializzazione del campo aggiuntivo del pcb
                p->dev_no = j;
                blockProcessOnDevice(p, i, -1);
                return;
            }
        }  
    }
}

/*  Esecuzione continua del processo SSI attraverso un ciclo while   */
void SSILoop(){
    while(TRUE){
        unsigned int payload;

        //attesa di una richiesta da parte di un client attraverso SYS2
        unsigned int sender = SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (unsigned int)(&payload), 0);

        //tentativo di soddisfare la richiesta 
        unsigned int ret = SSIRequest((pcb_t *)sender, (ssi_payload_t *)payload);

        //se necessario restituzione di messaggio di ritorno attraverso SYS1
        if(ret != -1)
            SYSCALL(SENDMESSAGE, (unsigned int)sender, ret, 0);
    }
}

/*  Funzione per la creazione di un nuovo processo come figlio del processo sender  */
unsigned int ssi_new_process(ssi_create_process_t *p_info, pcb_t* parent){
    pcb_t* child = allocPcb();

    //inizializzazione dei campi del pcb del nuovo processo
    child->p_pid = pid_counter++;
    child->p_time = 0;
    child->p_supportStruct = p_info->support;
    saveState(&(child->p_s), p_info->state);
    
    //inserimento nella lista child del sender
    insertChild(parent, child);

    //inserimento nella ready queue
    insertProcQ(&Ready_Queue, child);

    process_count++;
    return (unsigned int)child;
}

/*  Funzione ricorsiva che termina il processo sender e la prole di questo  */
void ssi_terminate_process(pcb_t* proc){
    if(!(proc == NULL)){
        //ciclo in cui iterativamente si scorre la lista dei figli e chiama la funzione ricorsivamente sui fratelli
        while (!emptyChild(proc)){    
            ssi_terminate_process(removeChild(proc));
        }
    }  
    --process_count;

    //Se il processo si trova in una delle liste dei processi bloccati dei dispositivi, decremento il soft_blocked_count
    if( outProcQ(&Locked_terminal_recv, proc) != NULL ||
        outProcQ(&Locked_terminal_transm, proc) != NULL ||
        outProcQ(&Locked_disk, proc) != NULL ||
        outProcQ(&Locked_flash, proc) != NULL ||
        outProcQ(&Locked_ethernet, proc) != NULL ||
        outProcQ(&Locked_printer, proc) != NULL ||
        outProcQ(&Locked_pseudo_clock, proc) != NULL    ){
            soft_blocked_count--;  
        }
        
    outChild(proc);
    freePcb(proc);
}

/*  Funzione per il clockwait che incrementa il soft_blocked_count e inserisce il sender nella lista Locked_pseudo_clock */
void ssi_clockwait(pcb_t *sender){
    soft_blocked_count++;
    insertProcQ(&Locked_pseudo_clock, sender);
}

/*  Funzione che ritorna il pid del sender se arg = NULL, altrimenti il pid del parent */
int ssi_getprocessid(pcb_t *sender, void *arg){
    return (arg == NULL ? sender->p_pid : sender->p_parent->p_pid);
}

/*  Funzione che incrementa il soft_blocked_count, chiama la funzione addrToDevice() passando 
    commandAddr e imposta quest'ultimo uguale al valore del campo commandValue  */
void ssi_doio(pcb_t *sender, ssi_do_io_t *doio){
    soft_blocked_count++;
    addrToDevice((memaddr)doio->commandAddr, sender);
    *(doio->commandAddr) = doio->commandValue;
}

/*  Funzione che gestisce mediante il payload ricevuto dal sender la richiesta di un servizio   */
unsigned int SSIRequest(pcb_t* sender, ssi_payload_t *payload){
    unsigned int ret = 0;
    switch(payload->service_code){
        case CREATEPROCESS:
            ret = (emptyProcQ(&pcbFree_h) ? NOPROC : (unsigned int) ssi_new_process((ssi_create_process_PTR)payload->arg, sender));
            break;

        case TERMPROCESS:
            //terminates the sender process if arg is NULL, otherwise terminates arg
            if(payload->arg == NULL) {
                ssi_terminate_process(sender);
                ret = -1;
            }
            else {
                ssi_terminate_process(payload->arg);
                ret = 0; 
            }
            break;

        case DOIO:  //gestione DOIO
            ssi_doio(sender, payload->arg);
            ret = -1;
            break;

        case GETTIME:
            ret = (unsigned int)sender->p_time;
            break;

        case CLOCKWAIT:
            ssi_clockwait(sender);
            ret = -1;
            break;

        case GETSUPPORTPTR:
            ret = (unsigned int)sender->p_supportStruct;
            break;

        case GETPROCESSID:
            ret = (unsigned int)ssi_getprocessid(sender, payload->arg);
            break;
        default:
            ssi_terminate_process(sender);
            ret = MSGNOGOOD;
            break;
    }
    return ret;
}