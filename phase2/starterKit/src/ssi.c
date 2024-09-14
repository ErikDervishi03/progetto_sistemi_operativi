#include "const.h"
#include "dep.h"
#include "headers/pcb.h"
#include <umps3/umps/const.h>

// funzione per bloccare un processo per I/O associato a un device

void addrToDevice(memaddr command_address, pcb_t *process) {
    for (int j = 0; j < N_DEV_PER_IL; j++) {
        termreg_t *base_address = (termreg_t *)DEV_REG_ADDR(IL_TERMINAL, j);
        if (command_address == (memaddr)&(base_address->recv_command)) {
            outProcQ(&ready_queue, process);
            insertProcQ(&blockedForRecv, process);
            return;
        } else if (command_address == (memaddr)&(base_address->transm_command)) {
            outProcQ(&ready_queue, process);
            insertProcQ(&blockedForTransm, process);
            return;
        }
    }

    for (int i = DEV_IL_START; i < 7; i++) {
        for (int j = 0; j < N_DEV_PER_IL; j++) {
            dtpreg_t *base_address = (dtpreg_t *)DEV_REG_ADDR(i, j);
            if (command_address == (memaddr)&(base_address->command)) {
                outProcQ(&ready_queue, process);
                insertProcQ(&blockedForDevice[EXT_IL_INDEX(i)], process);
                return;
            }
        }
    }
}

// funzione per sbloccare un processo bloccato per I/O
static int processWaitingDevice(pcb_t * process){
    int result = 0;
    // scorro la lista dei device e controllo se il processo è bloccato per uno di essi
    for (int i = 0 ; i < NDEV; i ++){
        pcb_t * unblocked = outProcQ(&blockedForDevice[i], process);

        if(unblocked){
            result = 1;
        }
    } 
    return result;
} 

// funzione ricorsiva per terminare un processo e tutta la sua progenie
void ssi_terminate_process(pcb_t* process) {

    while(!emptyChild(process))   
    {
        ssi_terminate_process(removeChild(process));
    }
    // se il processo è bloccato per clock o I/O, decremento il numero di processi bloccati
    if (outProcQ(&blockedForClock, process) != NULL ||
        processWaitingDevice(process)
        ) {

        softBlockCount--;

    }
    // libera spazio 
    outChild(process);
    freePcb(process);
    processCount--;
}

int handle_request(pcb_t* process, ssi_payload_t *payload) {
    int ssiResponse = 0;
    if (payload->service_code == CREATEPROCESS) {
        // stato del processo figlio passato come arg 
        ssi_create_process_t *process_info = (ssi_create_process_t *)payload->arg;
        pcb_t* child = allocPcb(); // alloco un pcb per il processo figlio
        if (child == NULL) {
            ssiResponse = NOPROC; // se non ci sono pcb disponibili
        } else {
            // almeno un pcb è disponibile, quindi inizializzo il processo figlio
            child->p_pid = currPid++;
            child->p_time = 0;
            child->p_supportStruct = process_info->support;
            child->p_s = *process_info->state;
            insertChild(process, child);
            insertProcQ(&ready_queue, child);
            processCount++;
            ssiResponse = (unsigned int)child;
        }

    } else if (payload->service_code == TERMPROCESS) {          
        // se arg è NULL, termino il processo chiamante         
        if (payload->arg == NULL) {         
            ssi_terminate_process(process);         
            ssiResponse = NOSSIRESPONSE;            
        } else {            
            // altrimenti termino il processo in arg (e tutta la sua progenie)
            ssi_terminate_process((pcb_t *)payload->arg);
            ssiResponse = 0;
        }
    // se il servizio richiesto è un I/O
    // tutti gli I/O sono asincroni, quindi il processo chiamante viene bloccato fino a quando non viene risolto l'I/O

    } else if (payload->service_code == DOIO) {

        ssi_do_io_t *io = (ssi_do_io_t *)payload->arg; // arg contiene una struttura con command address e command value
        softBlockCount++; // incremento il numero di processi bloccati
        addrToDevice((memaddr)io->commandAddr, process); // aggiungo il processo alla coda del device corrispondente 
        *(io->commandAddr) = io->commandValue; // scrive il valore richiesto nel registro del device
        ssiResponse = NOSSIRESPONSE;

    } else if (payload->service_code == GETTIME) {
        // serve a restituire il tempo trascorso in CPU dal processo chiamante
        ssiResponse = (int)process->p_time;

    } else if (payload->service_code == CLOCKWAIT) {
        // il kernel implementa lo pseudo clock, un virtual device che genera un interrupt ogni 100 ms (PSECOND)
        // il processo chiamante viene bloccato fino al prossimo tick (interrupt) del clock
        insertProcQ(&blockedForClock, process);
        softBlockCount++;
        ssiResponse = NOSSIRESPONSE;
    } else if (payload->service_code == GETSUPPORTPTR) {
        // restituisce il puntatore alla struttura di supporto del processo chiamante
        ssiResponse = (int)process->p_supportStruct;

    } else if (payload->service_code == GETPROCESSID) {
        // restituisce il pid del processo chiamante se arg è '0', altrimenti restituisce il pid del padre
        if (payload->arg == NULL) {
            ssiResponse = process->p_pid;
        } else {
            ssiResponse = process->p_parent->p_pid;
        }

    } else {
        // se il servizio richiesto non è valido, termino il processo chiamante
        ssi_terminate_process(process);
        ssiResponse = MSGNOGOOD;
    }

    return ssiResponse;
}

// l'SSI è parte fondamentale del kernel, gestisce la sincronizzazione tra i processi con I/O, pseudo clock ticks,
// creazione di processi (child e siblings), terminazione di processi, e gestione trap  
// se l'SSI dovesse mai terminare, il sistema andrebbe arrestato.
// è un ciclo continuo che riceve una richiesta, la processa e invia una risposta

void SSI_entry_point() {
    while (1) {
        unsigned int payload;
        unsigned int sender = SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (unsigned int)(&payload), 0);
        int ssiResponse = handle_request((pcb_t *)sender, (ssi_payload_t *)payload);
        if (ssiResponse != NOSSIRESPONSE)
            SYSCALL(SENDMESSAGE, (unsigned int)sender, ssiResponse, 0);
    }
}
