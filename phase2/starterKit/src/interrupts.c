#include "dep.h"



// funzione che tramite il device number rilascia il processo bloccato per quel device
pcb_t *releaseProcessByDeviceNumber(int dev_num, struct list_head *proc_list) {
    pcb_t* temp_proc;
    int i = 0;
    list_for_each_entry(temp_proc, proc_list, p_list) {
        if (i == dev_num) {
            return outProcQ(proc_list, temp_proc);
        }

        i++;
    }
    return NULL;
}

void handleNonTimer(int line_num, int cause, state_t *exc_state) {
    devregarea_t *dev_reg_area = (devregarea_t *)BUS_REG_RAM_BASE;
    
    // accedo alla bitmap dei device per la linea su cui è stato rilevato un interrupt
    unsigned int intr_devices_bitmap = dev_reg_area->interrupt_dev[line_num - DEV_IL_START];
    unsigned int dev_status;
    unsigned int dev_num;

    // calcolo numero del device in ordine di priorità
    if (intr_devices_bitmap & DEV7ON) dev_num = 7;
    else if (intr_devices_bitmap & DEV6ON) dev_num = 6;
    else if (intr_devices_bitmap & DEV5ON) dev_num = 5;
    else if (intr_devices_bitmap & DEV4ON) dev_num = 4;
    else if (intr_devices_bitmap & DEV3ON) dev_num = 3;
    else if (intr_devices_bitmap & DEV2ON) dev_num = 2;
    else if (intr_devices_bitmap & DEV1ON) dev_num = 1;
    else if (intr_devices_bitmap & DEV0ON) dev_num = 0;
    else return;

    pcb_t* unblocked_proc = NULL;

    // se il device è un terminale
    if (line_num == IL_TERMINAL) {
        // accedo al registro del device
        termreg_t *dev_reg = (termreg_t *)DEV_REG_ADDR(IL_TERMINAL, dev_num);

        // gestione intrerupt per i terminali --> 2 subdevice: trasmissione e ricezione
        // se il device è in trasmissione        
        if (((dev_reg->transm_status) & 0x000000FF) == 5) {
            dev_status = dev_reg->transm_status;
            dev_reg->transm_command = ACK;
            unblocked_proc = releaseProcessByDeviceNumber(dev_num, &blockedForTransm);
        } else {
            // se il device è in ricezione
            dev_status = dev_reg->recv_status;
            dev_reg->recv_command = ACK;
            unblocked_proc = releaseProcessByDeviceNumber(dev_num, &blockedForRecv);
        }
    } else {
        // se il device è un device generico
        dtpreg_t *dev_reg = (dtpreg_t *)DEV_REG_ADDR(line_num, dev_num);
        dev_status = dev_reg->status;
        dev_reg->command = ACK;

        struct list_head *proc_list;
        // se la lista dei processi bloccati per quel device non è vuota
        if ((proc_list = &blockedForDevice[EXT_IL_INDEX(line_num)])) {
            unblocked_proc = releaseProcessByDeviceNumber(dev_num, proc_list);
        }
    }

    if (unblocked_proc) {
        unblocked_proc->p_s.reg_v0 = dev_status;
        // invio il messaggio al processo sbloccato
        sendMessage(ssi_pcb, unblocked_proc, (memaddr)(dev_status));
        insertProcQ(&ready_queue, unblocked_proc);
        softBlockCount--;
    }

    if (current_process) 
        LDST(exc_state);
    else 
        scheduler();
}


// se un processo ha terminato il suo time slice a disposizione ma non ha ancora finito il suo CPU burst
// allora PLT interrupt! 
void handleLocalTimerInterrupt(state_t *exc_state) {
    setTIMER(-1);
    // aggiorno il tempo di esecuzione del processo corrente
    getDeltaTime(current_process);
    // salvo lo stato del processo corrente
    saveState(&(current_process->p_s), exc_state);
    // inserisco il processo nella ready queue: running -> ready
    insertProcQ(&ready_queue, current_process);
    // chiamo lo scheduler per far partire il prossimo processo
    scheduler();
}


// il kernel sblocca tutti i pcb bloccati per lo pseudo clock (pseudo clock tick)
// 
void handlePseudoClockInterrupt(state_t* exc_state) {
    // carico l'Interval Timer con il valore di PSECOND: 100 ms
    LDIT(PSECOND);
    pcb_t *unblocked_proc;
    // sblocco tutti i processi bloccati per il clock
    while ((unblocked_proc = removeProcQ(&blockedForClock)) != NULL) {
        sendMessage(ssi_pcb, unblocked_proc, 0);
        insertProcQ(&ready_queue, unblocked_proc);
        softBlockCount--;
    }
    // se c'è un processo in esecuzione, salvo il suo stato, altrimenti chiamo lo scheduler
    if (current_process) 
        LDST(exc_state);
    else 
        scheduler();
}


// più è "basso" il numero di linea, più è alta la priorità
int getHighestPriorityNTint(int cause){
  for (int line = DEV_IL_START; line < N_INTERRUPT_LINES; line++){
    if (CAUSE_IP_GET(cause, line)){
      return line;
    }
  }
  return -1;
}




// gestore degli interrupt
// un interrupt può avvenire per: 
//    - completamento di un I/O
//    - il PLT o l'Interval Timer passano da 0x00000.0000 => 0xFFFF.FFFF 

void interruptHandler(int cause, state_t *exc_state) {
    // se Processor Local Timer
    if (CAUSE_IP_GET(cause, IL_CPUTIMER)) {
        handleLocalTimerInterrupt(exc_state);
    // se Interval Timer
    } else if (CAUSE_IP_GET(cause, IL_TIMER)) {
        handlePseudoClockInterrupt(exc_state);
    // se I/O
    } else{
        // prendo la linea con la priorità più alta
        int line = getHighestPriorityNTint(cause);

        if(line != -1)
            handleNonTimer(line, cause, exc_state);
    }

   
}
