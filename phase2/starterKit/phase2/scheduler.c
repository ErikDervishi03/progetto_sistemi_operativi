#include "const.h"
#include "dep.h"
#include <umps3/umps/libumps.h>
#include <umps3/umps/types.h>

int counter = 0;
void scheduler(){
    pcb_t* current_process = removeProcQ(&ready_queue);
    if(current_process != NULL){
        current_process->p_time += getTimeElapsed();
        current_process->p_s.status |= TEBITON;
        setTIMER(TIMESLICE);
        LDST(&current_process->p_s);
    }
    else{
        if(processCount == 1 && current_process == ssi_pcb){
            HALT();
        }
        else if(processCount > 1 && softBlockCount > 0){
            setSTATUS((getSTATUS() | IECON | IMON) & ~TEBITON);
            WAIT();
        }
        else if (processCount > 0 && softBlockCount == 0){
            PANIC();
        }
    }
}


