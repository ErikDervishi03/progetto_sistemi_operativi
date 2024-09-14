#include "dep.h"


// semplice scheduler con politica Round Robin con time slice 5 ms (TIMESLICE), ciò porta alla necessità di un interrupt di clock
// umps3 ci offre due opzioni per gestire l'interrupt di clock: o utilizzando l'Interval Timer o il Processor's Local Timer (PLT)
// usiamo il PLT perchè l'Interval Timer è riservato all'implementazione degli Pseudo Clock ticks
void scheduler(){
    // rimuovi il processo dalla coda dei ready e salva in current process 
    if((current_process = removeProcQ(&ready_queue))){
        setTIMER(TIMESLICE * (*((cpu_t *)TIMESCALEADDR))); // carico 5 ms nel PLT
        STCK(prevTod); // salvo il tempo corrente
        LDST(&(current_process->p_s)); // faccio un Load Processor State del del processor state del current process
        return;
    }
    

    // se la ready queue è vuota
    // se c'è solo un processo startato (SSI) allora HALT
    if(processCount == 1) {
        HALT();
    }
    // se ci sono più processi startati e ci sono processi in attesa di I/O
    else if(processCount > 0 && softBlockCount > 0 ){
        setSTATUS(ALLOFF | IECON | IMON); // abilito gli interrupt e disabilito PLT
        WAIT(); // aspetto un interrupt, "il primo interrupt che accade dopo essere entrato in wait non dovrebbe essere per il PLT"
    }
    // il deadlock è così definito: se ci sono processi startati e non ci sono processi in attesa di I/O
    else if(processCount > 0 && softBlockCount == 0){
        PANIC(); // uddio un lucchetto morto    
    }
    
}