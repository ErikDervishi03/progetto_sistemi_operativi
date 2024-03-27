#include <umps3/umps/libumps.h>
#include <umps3/umps/types.h>
#include <umps3/umps/const.h>
#include "../include/phase1/headers/msg.h"
#include "../include/phase1/headers/pcb.h"

void scheduler(){
    pcb_t* readyproc = removeProcQ(&ready_queue);
   
    if(readyproc != NULL){
        current_process = readyproc;
        //non so che significa, presa dal github
        current_process->p_s.status = (current_process->p_s.status) | TEBITON;
        setTIMER(5000);
        LDST(&current_process->p_s);
    }
    else{
        //non so come verificare se SSI sia l'unico in esecuzione
        if(processCount == 1    )
            HALT();
        else if(processCount > 1 && softBlockCount > 0)
            WAIT();
        else if (processCount > 0 && softBlockCount == 0)
            PANIC();
    }
}
