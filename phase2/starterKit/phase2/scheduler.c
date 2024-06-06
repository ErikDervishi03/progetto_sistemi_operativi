#include "const.h"
#include "dep.h"
#include "headers/pcb.h"
#include <umps3/umps/const.h>
#include <umps3/umps/libumps.h>
#include <umps3/umps/types.h>

void scheduler(){
    term_puts("entrato in scheduler\n");

    if(current_process != NULL){
      current_process->p_time += deltaTime();
    }
    current_process = removeProcQ(&ready_queue);

    if(current_process!=NULL){
        STCK(prevTOD);
        setTIMER(TIMESLICE);

        term_puts("passo il controllo a current process\n");

        //if(current_process == print_pcb) term_puts("current_process == print_pcb\n");
        LDST(&current_process->p_s);
    }
    else{
        if(processCount == 0 ||  (processCount == 1 && isInList(&msg_queue, ssi_pcb->p_pid))) {
            HALT();
        }
        if(processCount > 0 && softBlockCount > 0 ){
            setSTATUS(ALLOFF | IECON | IMON);
            WAIT();
        }
        else if(processCount > 0 && softBlockCount == 0){
            term_puts("vado in panic nello scheduler\n");
            PANIC();
        }
    }
}




