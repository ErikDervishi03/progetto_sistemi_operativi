#include "dep.h"

void scheduler(){
    pcb_t* readyproc = removeProcQ(&ready_queue);
   
    if(readyproc != NULL){
        currentProcess = readyproc;
        //non so che significa, presa dal github
        currentProcess->p_s.status = (currentProcess->p_s.status) | TEBITON;
        setTIMER(5000);
        LDST(&currentProcess->p_s);
    }
    else{
        //non so come verificare se SSI sia l'unico in esecuzione
        if(processCount == 1)
            HALT();
        else if(processCount > 1 && softBlockCount > 0)
            WAIT();
        else if (processCount > 0 && softBlockCount == 0)
            PANIC();
    }
}
