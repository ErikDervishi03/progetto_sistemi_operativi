#include "dep.h"

/* Function to remove the process p from the ready queue and insert it
   into the appropriate list of blocked processes based on the device line. */
static void blockProcessOnDevice(pcb_t* p, int line, int term) {
    outProcQ(&ready_queue, p);

    if (line == IL_DISK) {
        insertProcQ(&Locked_disk, p);
    } else if (line == IL_FLASH) {
        insertProcQ(&Locked_flash, p);
    } else if (line == IL_ETHERNET) {
        insertProcQ(&Locked_ethernet, p);
    } else if (line == IL_PRINTER) {
        insertProcQ(&Locked_printer, p);
    } else if (line == IL_TERMINAL) {
        insertProcQ((term > 0) ? &Locked_terminal_transm : &Locked_terminal_recv, p);
    }
}

/* Function to map the command address to the corresponding device line and device number.
   For terminals, sets the term parameter to 0 for recv and 1 for transm. */
static void addrToDevice(memaddr command_address, pcb_t *p) {
    // Iterate over terminal devices
    for (int j = 0; j < 8; j++) { 
        termreg_t *base_address = (termreg_t *)DEV_REG_ADDR(7, j);
        if (command_address == (memaddr)&(base_address->recv_command)) {
            p->numero_dispositivo = j;
            blockProcessOnDevice(p, 7, 0);
            return;
        } else if (command_address == (memaddr)&(base_address->transm_command)) {
            p->numero_dispositivo = j;
            blockProcessOnDevice(p, 7, 1);
            return;
        }
    }

    // Iterate over other devices
    for (int i = 3; i < 7; i++) {
        for (int j = 0; j < 8; j++) { 
            dtpreg_t *base_address = (dtpreg_t *)DEV_REG_ADDR(i, j);
            if (command_address == (memaddr)&(base_address->command)) {
                p->numero_dispositivo = j;
                blockProcessOnDevice(p, i, -1);
                return;
            }
        }  
    }
}

/* Function to create a new process as a child of the sender process. */
unsigned int ssi_new_process(ssi_create_process_t *p_info, pcb_t* parent) {
    pcb_t* child = allocPcb();

    // Initialize the new process's PCB fields
    child->p_pid = pid_counter++;
    child->p_time = 0;
    child->p_supportStruct = p_info->support;
    child->p_s = *p_info->state;
    
    // Insert into the parent's child list
    insertChild(parent, child);

    // Insert into the ready queue
    insertProcQ(&Ready_Queue, child);

    processCount++;
    return (unsigned int)child;
}

/* Recursive function to terminate the process and its children. */
void ssi_terminate_process(pcb_t* proc) {
    if (proc != NULL) {
        // Recursively terminate child processes
        while (!emptyChild(proc)) {    
            ssi_terminate_process(removeChild(proc));
        }

        processCount = processCount - 1;

        // If the process is in any of the blocked lists, decrement soft_blocked_count
        if (outProcQ(&Locked_terminal_recv, proc) != NULL ||
            outProcQ(&Locked_terminal_transm, proc) != NULL ||
            outProcQ(&Locked_disk, proc) != NULL ||
            outProcQ(&Locked_flash, proc) != NULL ||
            outProcQ(&Locked_ethernet, proc) != NULL ||
            outProcQ(&Locked_printer, proc) != NULL ||
            outProcQ(&Locked_pseudo_clock, proc) != NULL) {
            softBlockCount--;  
        }
        
        outChild(proc);
        freePcb(proc);
    }
}

/* Function to handle clock wait, increments soft_blocked_count and inserts the sender into Locked_pseudo_clock list. */
void ssi_clockwait(pcb_t *sender) {
    softBlockCount++;
    insertProcQ(&Locked_pseudo_clock, sender);
}

/* Function to return the process ID of the sender if arg is NULL, otherwise returns the parent's PID. */
int ssi_getprocessid(pcb_t *sender, void *arg) {
    return (arg == NULL) ? sender->p_pid : sender->p_parent->p_pid;
}

/* Function to handle DOIO request: increments soft_blocked_count, maps the command address to the device,
   and sets the command value. */
void ssi_doio(pcb_t *sender, ssi_do_io_t *doio) {
    softBlockCount++;
    addrToDevice((memaddr)doio->commandAddr, sender);
    *(doio->commandAddr) = doio->commandValue;
}

/* Function to process service requests based on the payload received from the sender. */
unsigned int SSIRequest(pcb_t* sender, ssi_payload_t *payload) {
    unsigned int ret = 0;
    
    if (payload->service_code == CREATEPROCESS) {
        ret = (emptyProcQ(&pcbFree_h) ? NOPROC : (unsigned int)ssi_new_process((ssi_create_process_PTR)payload->arg, sender));
    } else if (payload->service_code == TERMPROCESS) {
        if (payload->arg == NULL) {
            ssi_terminate_process(sender);
            ret = -1;
        } else {
            ssi_terminate_process(payload->arg);
            ret = 0; 
        }
    } else if (payload->service_code == DOIO) {
        ssi_doio(sender, payload->arg);
        ret = -1;
    } else if (payload->service_code == GETTIME) {
        ret = (unsigned int)sender->p_time;
    } else if (payload->service_code == CLOCKWAIT) {
        ssi_clockwait(sender);
        ret = -1;
    } else if (payload->service_code == GETSUPPORTPTR) {
        ret = (unsigned int)sender->p_supportStruct;
    } else if (payload->service_code == GETPROCESSID) {
        ret = (unsigned int)ssi_getprocessid(sender, payload->arg);
    } else {
        ssi_terminate_process(sender);
        ret = MSGNOGOOD;
    }
    
    return ret;
}

/* Main entry point for the SSI process, continuously processes incoming requests in a loop. */
void SSI_function_entry_point() {
    while (1) {
        unsigned int payload;

        // Wait for a client request through SYS2
        unsigned int sender = SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (unsigned int)(&payload), 0);

        // Attempt to fulfill the request
        unsigned int ret = SSIRequest((pcb_t *)sender, (ssi_payload_t *)payload);

        // If necessary, return a response message through SYS1
        if (ret != -1) {
            SYSCALL(SENDMESSAGE, (unsigned int)sender, ret, 0);
        }
    }
}
