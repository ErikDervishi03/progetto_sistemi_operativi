#include "dep.h"

static void blockProcessOnDevice(pcb_t* p, int line, int term) {
    outProcQ(&Ready_Queue, p);
    switch (line) {
        case IL_DISK:
            insertProcQ(&Locked_disk, p);
            break;
        case IL_FLASH:
            insertProcQ(&Locked_flash, p);
            break;
        case IL_ETHERNET:
            insertProcQ(&Locked_ethernet, p);
            break;
        case IL_PRINTER:
            insertProcQ(&Locked_printer, p);
            break;
        case IL_TERMINAL:
            insertProcQ((term > 0) ? &Locked_terminal_transm : &Locked_terminal_recv, p);
            break;
        default:
            break;
    }
}

static void addrToDevice(memaddr command_address, pcb_t *p) {
    for (int j = 0; j < 8; j++) {
        termreg_t *base_address = (termreg_t *)DEV_REG_ADDR(7, j);
        if (command_address == (memaddr)&(base_address->recv_command)) {
            p->dev_no = j;
            blockProcessOnDevice(p, 7, 0);
            return;
        } else if (command_address == (memaddr)&(base_address->transm_command)) {
            p->dev_no = j;
            blockProcessOnDevice(p, 7, 1);
            return;
        }
    }
    for (int i = 3; i < 7; i++) {
        for (int j = 0; j < 8; j++) {
            dtpreg_t *base_address = (dtpreg_t *)DEV_REG_ADDR(i, j);
            if (command_address == (memaddr)&(base_address->command)) {
                p->dev_no = j;
                blockProcessOnDevice(p, i, -1);
                return;
            }
        }
    }
}

void SSILoop() {
    while (TRUE) {
        unsigned int payload;
        unsigned int sender = SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (unsigned int)(&payload), 0);
        unsigned int ret = SSIRequest((pcb_t *)sender, (ssi_payload_t *)payload);
        if (ret != -1)
            SYSCALL(SENDMESSAGE, (unsigned int)sender, ret, 0);
    }
}

unsigned int ssi_new_process(ssi_create_process_t *p_info, pcb_t* parent) {
    pcb_t* child = allocPcb();
    child->p_pid = currPid++;
    child->p_time = 0;
    child->p_supportStruct = p_info->support;
    saveState(&(child->p_s), p_info->state);
    insertChild(parent, child);
    insertProcQ(&Ready_Queue, child);
    processCount++;
    return (unsigned int)child;
}

void ssi_terminate_process(pcb_t* proc) {
    if (proc != NULL) {
        while (!emptyChild(proc)) {
            ssi_terminate_process(removeChild(proc));
        }
    }
    processCount--;
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

void ssi_clockwait(pcb_t *sender) {
    softBlockCount++;
    insertProcQ(&Locked_pseudo_clock, sender);
}

int ssi_getprocessid(pcb_t *sender, void *arg) {
    return (arg == NULL ? sender->p_pid : sender->p_parent->p_pid);
}

void ssi_doio(pcb_t *sender, ssi_do_io_t *doio) {
    softBlockCount++;
    addrToDevice((memaddr)doio->commandAddr, sender);
    *(doio->commandAddr) = doio->commandValue;
}

unsigned int SSIRequest(pcb_t* sender, ssi_payload_t *payload) {
    unsigned int ret = 0;
    switch(payload->service_code) {
        case CREATEPROCESS:
            ret = (emptyProcQ(&pcbFree_h) ? NOPROC : (unsigned int) ssi_new_process((ssi_create_process_PTR)payload->arg, sender));
            break;
        case TERMPROCESS:
            if (payload->arg == NULL) {
                ssi_terminate_process(sender);
                ret = -1;
            } else {
                ssi_terminate_process(payload->arg);
                ret = 0;
            }
            break;
        case DOIO:
            ssi_doio(sender, payload->arg);
            ret = -1;
            break;
        case GETTIME:
            ret = (unsigned int)sender->p_time;
            break;
        case CLOCKWAIT:
            ssi_clockwait(sender);
            ret = -1;
            break;
        case GETSUPPORTPTR:
            ret = (unsigned int)sender->p_supportStruct;
            break;
        case GETPROCESSID:
            ret = (unsigned int)ssi_getprocessid(sender, payload->arg);
            break;
        default:
            ssi_terminate_process(sender);
            ret = MSGNOGOOD;
            break;
    }
    return ret;
}
