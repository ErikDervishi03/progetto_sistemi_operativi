#include "dep.h"

void scheduler(){
    if(emptyProcQ(&Ready_Queue) == 0){
        //  1. Se c'e' almeno un processo pronto ad essere eseguito, lo prendo dalla Ready e lo assegno al current_process.
        current_process = removeProcQ(&Ready_Queue);

        //  2. carico il processo localt timer a 5ms [Section 4.1.4-pops].
        setPLT(TIMESLICE * (*((cpu_t *)TIMESCALEADDR)));

        //  3. Eseguo un Load Processor State (LDST) sul Current Process (p_s).
        start = getTOD(); 
        LDST(&(current_process->p_s));
    }
    else{

        // nel caso la Ready Queue dovesse risultare vuota si entra in questo ramo else in cui avviena la deadlock detection
        if(process_count == 1) {
            HALT();
        }
        else if(process_count > 0 && soft_blocked_count > 0 ){
            current_process = NULL;
            setSTATUS(ALLOFF | IECON | IMON);
            WAIT();
        }
        else if(process_count > 0 && soft_blocked_count == 0){
            PANIC();
        }
    }
}