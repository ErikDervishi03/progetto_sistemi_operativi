#include "dep.h"

/*
 * semplice libreria che funge da wrapper 
 * per le funzioni di utilizzo dei vari timer
 */

// ritorna il valore del TOD al momento della chiamata
unsigned int getTOD() {
    unsigned int tmp;
    STCK(tmp);
    return tmp;
}

// aggiorna p_time del processo p
void updateCPUtime(pcb_t *p) {
    int end = getTOD();
    p->p_time += (end - start);
    start = end;
}

// funzione che imposta il valore dell'interval timer
void setIntervalTimer(unsigned int t) {
    LDIT(t);
}

// funzione che imposta il valore del processor local timer
void setPLT(unsigned int t) {
    setTIMER(t);
}

// funzione che permette di ottenere il valore del processor local timer
unsigned int getPLT() {
    return getTIMER();
}

void uTLB_RefillHandler() {
setENTRYHI(0x80000000);
setENTRYLO(0x00000000);
TLBWR();
LDST((state_t*) 0x0FFFF000);
}

extern void test();

LIST_HEAD(Ready_Queue);
LIST_HEAD(Locked_disk);
LIST_HEAD(Locked_flash);
LIST_HEAD(Locked_terminal_recv);
LIST_HEAD(Locked_terminal_transm);
LIST_HEAD(Locked_ethernet);
LIST_HEAD(Locked_printer);
LIST_HEAD(Locked_pseudo_clock);


int process_count;
int soft_blocked_count;
int start;
int pid_counter;

pcb_t *current_process;
pcb_t *ssi_pcb;

// funzione che si occupa di inizializzare il passupvector ad un indirizzo specifico e 
// memorizza uTLB_RefillHandler e del exceptionHandler.
void initPassupVector(){
    passupvector_t *passupv;
    passupv = (passupvector_t *) PASSUPVECTOR;

    passupv-> tlb_refill_handler  = (memaddr) uTLB_RefillHandler;
    passupv->tlb_refill_stackPtr  = (memaddr) KERNELSTACK;
    passupv-> exception_handler  = (memaddr) exceptionHandler;
    passupv->exception_stackPtr   = (memaddr) KERNELSTACK;
}



// Funzione che inserisce nella Ready Queue i processi del SSI e del test. Questi avranno lo status settato 
// in modo da avere la maschera dell'interrupt abilitata, l'interval timer abilitato e che siano in 
// modalita' kernel. Avranno rispettivamente pid 1 e 2.
void initFirstProcesses(){
    //  6. Instantiate a first process, place its PCB in the Ready Queue, and increment Process Count.
    ssi_pcb = allocPcb();
    ssi_pcb->p_pid = 1;
    ssi_pcb->p_supportStruct = NULL;

    // pongo tutti i Bit della maschera a 0 (ALLOFF) e accendo quelli descritti a inizio funzione
    ssi_pcb->p_s.status = ALLOFF | IEPON | IMON | TEBITON;

    // setto SP in modo da porre sull'ultimo frame della RAM il processo SSI attraverso RAMTOP
    RAMTOP(ssi_pcb->p_s.reg_sp);
    ssi_pcb->p_s.pc_epc = (memaddr) SSILoop;

    // general purpose register t9
    ssi_pcb-> p_s.gpr[24] = ssi_pcb-> p_s.pc_epc;
    
    // inserisco il processo nella ReadyQueue
    insertProcQ(&Ready_Queue, ssi_pcb);
    
    // incremento il Process Counter
    ++process_count;


    //  7.  
    // Creo un nuovo processo, lo inserisco nella Ready Queue, e  incremento il Process Counter
    pcb_t *toTest = allocPcb();
    toTest->p_pid = 2;
    toTest->p_supportStruct = NULL;

    // pongo tutti i Bit della maschera a 0 (ALLOFF) e accendo quelli descritti a inizio funzione
    toTest->p_s.status = ALLOFF | IEPON | IMON | TEBITON;

    // setto SP in modo da porre sull'ultimo frame della RAM il processo test attraverso RAMTOP - 2 FRAME
    RAMTOP(toTest->p_s.reg_sp);
    toTest->p_s.reg_sp -= (2 * PAGESIZE);
    toTest->p_s.pc_epc = (memaddr) test;

    // general purpose register t9
    toTest-> p_s.gpr[24] = toTest-> p_s.pc_epc;

    // inserisco il processo nella ReadyQueue
    insertProcQ(&Ready_Queue, toTest);

    // incremento il Process Counter
    ++process_count;

}


/* 
 *main 
*/
void main(int argc, char const *argv[])
{
    //  2. Passup Vector
    initPassupVector();

    //  3. Inizzializzo le data structures del livello 2 (Phase 1):
    initPcbs();
    initMsgs();

    //  4. 
    //  intero che indica il numero dei processi totali
    process_count = 0;

    //  numero dei processi bloccati non ancora terminati
    soft_blocked_count = 0;

    // variabile globale che viene usata per creare nuovi processi; viene incrementata per ogni processo in modo da assegnarli tutti in maniera diversa
    pid_counter = 3;    //pid 1 is SSI, pid 2 is test

    //  Lista dei processi in "ready" state
    mkEmptyProcQ(&Ready_Queue);

    //  liste dei processi bloccati per ogni external (sub)device
    mkEmptyProcQ(&Locked_disk);
    mkEmptyProcQ(&Locked_flash);
    mkEmptyProcQ(&Locked_terminal_recv);
    mkEmptyProcQ(&Locked_terminal_transm);
    mkEmptyProcQ(&Locked_ethernet);
    mkEmptyProcQ(&Locked_printer);
    mkEmptyProcQ(&Locked_pseudo_clock);


    //  5. carico l'Interval Timer a 100 ms
    setIntervalTimer(PSECOND);

    // inizializzo i primi due processi (punti 6 e 7)
    initFirstProcesses();

    //  8. chiamo lo Scheduler.
    scheduler();
}
