#include "dep.h"

static pcb_t *unblockProcessByDeviceNumber(int device_number, struct list_head *list) {
    pcb_t* tmp;
    list_for_each_entry(tmp, list, p_list) {
        if (tmp->dev_no == device_number) {
            return outProcQ(list, tmp);
        }
    }
    return NULL;
}

static void handleTerminalInterrupt(int device_number, unsigned int *device_status, pcb_t **unblocked_pcb) {
    termreg_t *device_register = (termreg_t *)DEV_REG_ADDR(IL_TERMINAL, device_number);
    if (((device_register->transm_status) & 0x000000FF) == 5) {
        *device_status = device_register->transm_status;
        device_register->transm_command = ACK;
        *unblocked_pcb = unblockProcessByDeviceNumber(device_number, &blockedForTransm);
    } else {
        *device_status = device_register->recv_status;
        device_register->recv_command = ACK;
        *unblocked_pcb = unblockProcessByDeviceNumber(device_number, &blockedForRecv);
    }
}

static void handleNonTerminalInterrupt(int line, int device_number, unsigned int *device_status, pcb_t **unblocked_pcb) {
    dtpreg_t *device_register = (dtpreg_t *)DEV_REG_ADDR(line, device_number);
    *device_status = device_register->status;
    device_register->command = ACK;

    struct list_head *list = NULL;

    if (line == IL_DISK) {
        list = &blockedForDisk;
    } else if (line == IL_FLASH) {
        list = &blockedForFlash;
    } else if (line == IL_ETHERNET) {
        list = &blockedForEthernet;
    } else if (line == IL_PRINTER) {
        list = &blockedForPrinter;
    }

    if (list) {
        *unblocked_pcb = unblockProcessByDeviceNumber(device_number, list);
    }
}

static void deviceInterruptHandler(int line, int cause, state_t *exception_state) {
    devregarea_t *device_register_area = (devregarea_t *)BUS_REG_RAM_BASE;
    unsigned int interrupting_devices_bitmap = device_register_area->interrupt_dev[line - 3];
    unsigned int device_status;
    unsigned int device_number;

    if (interrupting_devices_bitmap & DEV7ON) device_number = 7;
    else if (interrupting_devices_bitmap & DEV6ON) device_number = 6;
    else if (interrupting_devices_bitmap & DEV5ON) device_number = 5;
    else if (interrupting_devices_bitmap & DEV4ON) device_number = 4;
    else if (interrupting_devices_bitmap & DEV3ON) device_number = 3;
    else if (interrupting_devices_bitmap & DEV2ON) device_number = 2;
    else if (interrupting_devices_bitmap & DEV1ON) device_number = 1;
    else if (interrupting_devices_bitmap & DEV0ON) device_number = 0;
    else return;

    pcb_t* unblocked_pcb = NULL;

    if (line == IL_TERMINAL) {
        handleTerminalInterrupt(device_number, &device_status, &unblocked_pcb);
    } else {
        handleNonTerminalInterrupt(line, device_number, &device_status, &unblocked_pcb);
    }

    if (unblocked_pcb) {
        unblocked_pcb->p_s.reg_v0 = device_status;
        sendMessage(ssi_pcb, unblocked_pcb, (memaddr)(device_status));
        insertProcQ(&ready_queue, unblocked_pcb); 
        softBlockCount--;
    }

    if (current_process) 
        LDST(exception_state);
    else 
        scheduler();
}

static void localTimerInterruptHandler(state_t *exception_state) {
    setTIMER(-1);
    getDeltaTime(current_process);
    saveState(&(current_process->p_s), exception_state);
    insertProcQ(&ready_queue, current_process);
    scheduler();
}

static void pseudoClockInterruptHandler(state_t* exception_state) {
    LDIT(PSECOND);
    pcb_t *unblocked_pcb;
    while ((unblocked_pcb = removeProcQ(&blockedForClock)) != NULL) {
        sendMessage(ssi_pcb, unblocked_pcb, 0);
        insertProcQ(&ready_queue, unblocked_pcb);
        softBlockCount--;
    }

    if (current_process) 
        LDST(exception_state);
    else 
        scheduler();
}

void interruptHandler(int cause, state_t *exception_state) {
    if (CAUSE_IP_GET(cause, IL_CPUTIMER)) {
        localTimerInterruptHandler(exception_state);
    } else if (CAUSE_IP_GET(cause, IL_TIMER)) {
        pseudoClockInterruptHandler(exception_state);
    } else if (CAUSE_IP_GET(cause, IL_DISK)) {
        deviceInterruptHandler(IL_DISK, cause, exception_state);
    } else if (CAUSE_IP_GET(cause, IL_FLASH)) {
        deviceInterruptHandler(IL_FLASH, cause, exception_state);
    } else if (CAUSE_IP_GET(cause, IL_ETHERNET)) {
        deviceInterruptHandler(IL_ETHERNET, cause, exception_state);
    } else if (CAUSE_IP_GET(cause, IL_PRINTER)) {
        deviceInterruptHandler(IL_PRINTER, cause, exception_state);
    } else if (CAUSE_IP_GET(cause, IL_TERMINAL)) {
        deviceInterruptHandler(IL_TERMINAL, cause, exception_state);
    }
}