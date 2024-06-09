#include "dep.h"

void scheduler(){
    if((current_process = removeProcQ(&ready_queue))){
        setTIMER(TIMESLICE * (*((cpu_t *)TIMESCALEADDR)));
        STCK(prevTod);
        LDST(&(current_process->p_s));
        return;
    }
    
    if(processCount == 1) {
        HALT();
    }
    else if(processCount > 0 && softBlockCount > 0 ){
        setSTATUS(ALLOFF | IECON | IMON);
        WAIT();
    }
    else if(processCount > 0 && softBlockCount == 0){
        PANIC();
    }
    
}