#include "const.h"
#include "dep.h"
#include "headers/pcb.h"
#include <umps3/umps/const.h>
#include <umps3/umps/libumps.h>
#include <umps3/umps/types.h>

void scheduler(){
    current_process = removeProcQ(&ready_queue);

    if(current_process){
        current_process->p_time += getTimeElapsed();
        
        setTIMER(TIMESLICE);

        LDST(&current_process->p_s);
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




