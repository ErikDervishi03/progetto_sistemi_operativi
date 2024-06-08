#include "dep.h"

void scheduler(){
    if(emptyProcQ(&Ready_Queue) == 0){
        current_process = removeProcQ(&Ready_Queue);
        setTIMER(TIMESLICE * (*((cpu_t *)TIMESCALEADDR)));
        STCK(prevTod);
        LDST(&(current_process->p_s));
    }
    else{
        if(processCount == 1) {
            HALT();
        }
        else if(processCount > 0 && softBlockCount > 0 ){
            current_process = NULL;
            setSTATUS(ALLOFF | IECON | IMON);
            WAIT();
        }
        else if(processCount > 0 && softBlockCount == 0){
            PANIC();
        }
    }
}