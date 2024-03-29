#include "const.h"
#include "dep.h"
#include <umps3/umps/const.h>
#include <umps3/umps/libumps.h>

pcb_t *ssi_pcb(){
    while (1)
    {
        pcb_t *sender;
        int service;
        ssi_payload_t payload;
        void *arg;
        SYSCALL(RECEIVEMESSAGE, (unsigned int)sender, (unsigned int)(&payload), 0);
        insertProcQ(&BlockedPCBs,sender);
        service = payload.service_code;
        arg = payload.arg;
        void SSIRequest(pcb_t* sender, int service, void* arg);
        SYSCALL(SENDMESSAGE, (unsigned int)sender, (unsigned int)(&arg), 0);
    }    
  
};

void SSIRequest(pcb_t* sender, int service, void* arg){
    switch (service)
    {
    /*CreateProcess*/
    case 1:
        ssi_create_process_PTR ssi_create_process = (ssi_create_process_t*)arg;
        
        pcb_t *child = allocPcb();
        
        child->p_s.entry_hi= ssi_create_process->state->entry_hi;
        child->p_s.cause= ssi_create_process->state->cause;
        child->p_s.status = ssi_create_process->state->status;
        child->p_s.hi = ssi_create_process->state->hi;
        child->p_s.pc_epc= ssi_create_process->state->pc_epc;
        child->p_s.lo= ssi_create_process->state->lo;
        for(int i=0; i<=STATE_GPR_LEN; i++){
            child->p_s.gpr[i]= ssi_create_process->state->gpr[i];
        }
 
        insertChild(sender,child);
        insertProcQ(&ready_queue,child);
        processCount++;

        SYSCALL(SENDMESSAGE, (unsigned int)sender, (unsigned int)(&child), 0);
        break;
    /*Terminate Process*/
    case 2:
        while (emptyChild(sender) != 1)
        {
            removeChild(sender);
        }
        if(arg == NULL){
            outProcQ(&emptyProcQ,sender);
        }else{
            pcb_t *point = allocPcb();
            SYSCALL(SENDMESSAGE, (unsigned int)sender, (unsigned int)(&point), 0);
        }
        break;
    /*DoIO*/
    case 3:
        blockedPCBs[5 * N_DEV_PER_IL + 0] = sender; // ? 
        softBlockCount++;
        unsigned int devAddrBase = 0x10000054 + ((8 - 3) * 0x80) + (0 * 0x10);

        devregarea_t *bus_reg_area = (devregarea_t *)BUS_REG_RAM_BASE;
        devreg_t *devReg = &bus_reg_area->devreg[8 - DEV_IL_START][0];

        ssi_do_io_t *arg = (ssi_do_io_t *)arg;
        devReg->term.recv_command = arg->commandValue;

        // setSTATUS(getSTATUS() | ~getSTATUS()); mette tutti i bit a 1

        break;
    /*GetCPUTime*/
    case 4:
        SYSCALL(SENDMESSAGE, (unsigned int)sender, sender->p_time, 0);
        break;
    /*WaitForClock*/
    case 5:
        insertProcQ(&PseudoClockWP, sender);
        setCAUSE(getCAUSE() | TIMERINTERRUPT); // ?
        break;
    /*GetSupportData*/
    case 6:
        SYSCALL(SENDMESSAGE, (unsigned int)sender, (unsigned int)(sender->p_supportStruct), 0);
        break;
    /*GetProcessID*/
    case 7:
        SYSCALL(SENDMESSAGE, (unsigned int)sender, (unsigned int)((int)sender->p_pid), 0);
        break;   
    default:
        break;
    }
}


