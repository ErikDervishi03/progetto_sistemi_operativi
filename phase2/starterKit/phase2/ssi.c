#include "const.h"
#include "dep.h"
#include "headers/pcb.h"
#include <umps3/umps/const.h>

void blockProcessOnDevice(pcb_t* process, int line, int term) {
    outProcQ(&ready_queue, process);
    if (line == IL_DISK) {
        insertProcQ(&blockedForDisk, process);
    } else if (line == IL_FLASH) {
        insertProcQ(&blockedForFlash, process);
    } else if (line == IL_ETHERNET) {
        insertProcQ(&blockedForEthernet, process);
    } else if (line == IL_PRINTER) {
        insertProcQ(&blockedForPrinter, process);
    } else if (line == IL_TERMINAL) {
        if (term > 0) {
            insertProcQ(&blockedForTransm, process);
        } else {
            insertProcQ(&blockedForRecv, process);
        }
    }
}

void addrToDevice(memaddr command_address, pcb_t *process) {
    for (int j = 0; j < N_DEV_PER_IL; j++) {
        termreg_t *base_address = (termreg_t *)DEV_REG_ADDR(7, j);
        if (command_address == (memaddr)&(base_address->recv_command)) {
            process->dev_no = j;
            blockProcessOnDevice(process, 7, 0);
            return;
        } else if (command_address == (memaddr)&(base_address->transm_command)) {
            process->dev_no = j;
            blockProcessOnDevice(process, 7, 1);
            return;
        }
    }
    for (int i = 3; i < 7; i++) {
        for (int j = 0; j < N_DEV_PER_IL; j++) {
            dtpreg_t *base_address = (dtpreg_t *)DEV_REG_ADDR(i, j);
            if (command_address == (memaddr)&(base_address->command)) {
                process->dev_no = j;
                blockProcessOnDevice(process, i, -1);
                return;
            }
        }
    }
}

void ssi_terminate_process(pcb_t* process) {
    
    if(process){
        while(!emptyChild(process))   
        {
            ssi_terminate_process(removeChild(process));
        }
    }

    if (outProcQ(&ready_queue, process) != NULL ||
        outProcQ(&blockedForRecv, process) != NULL ||
        outProcQ(&blockedForTransm, process) != NULL ||
        outProcQ(&blockedForDisk, process) != NULL ||
        outProcQ(&blockedForFlash, process) != NULL ||
        outProcQ(&blockedForEthernet, process) != NULL ||
        outProcQ(&blockedForPrinter, process) != NULL ||
        outProcQ(&blockedForClock, process) != NULL) {
        softBlockCount--;
    }

    outChild(process);
    freePcb(process);
    processCount--;
}

int handle_request(pcb_t* process, ssi_payload_t *payload) {
    int ssiResponse = 0;
    if (payload->service_code == CREATEPROCESS) {

        ssi_create_process_t *process_info = (ssi_create_process_t *)payload->arg;
        pcb_t* child = allocPcb();
        if (child == NULL) {
            ssiResponse = NOPROC;
        } else {
            child->p_pid = currPid++;
            child->p_time = 0;
            child->p_supportStruct = process_info->support;
            child->p_s = *process_info->state;
            insertChild(process, child);
            insertProcQ(&ready_queue, child);
            processCount++;
            ssiResponse = (unsigned int)child;
        }

    } else if (payload->service_code == TERMPROCESS) {

        if (payload->arg == NULL) {
            ssi_terminate_process(process);
            ssiResponse = NOSSIRESPONSE;
        } else {
            ssi_terminate_process((pcb_t *)payload->arg);
            ssiResponse = 0;
        }

    } else if (payload->service_code == DOIO) {

        ssi_do_io_t *io = (ssi_do_io_t *)payload->arg;
        softBlockCount++;
        addrToDevice((memaddr)io->commandAddr, process);
        *(io->commandAddr) = io->commandValue;
        ssiResponse = NOSSIRESPONSE;

    } else if (payload->service_code == GETTIME) {

        ssiResponse = (int)process->p_time;

    } else if (payload->service_code == CLOCKWAIT) {

        insertProcQ(&blockedForClock, process);
        softBlockCount++;
        ssiResponse = NOSSIRESPONSE;

    } else if (payload->service_code == GETSUPPORTPTR) {

        ssiResponse = (int)process->p_supportStruct;

    } else if (payload->service_code == GETPROCESSID) {

        if (payload->arg == NULL) {
            ssiResponse = process->p_pid;
        } else {
            ssiResponse = process->p_parent->p_pid;
        }

    } else {

        ssi_terminate_process(process);
        ssiResponse = MSGNOGOOD;

    }

    return ssiResponse;
}

void SSI_entry_point() {
    while (1) {
        unsigned int payload;
        unsigned int sender = SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (unsigned int)(&payload), 0);
        int ssiResponse = handle_request((pcb_t *)sender, (ssi_payload_t *)payload);
        if (ssiResponse != NOSSIRESPONSE)
            SYSCALL(SENDMESSAGE, (unsigned int)sender, ssiResponse, 0);
    }
}
