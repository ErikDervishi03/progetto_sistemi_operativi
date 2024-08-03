#include "dep.h"

pcb_t *releaseProcessByDeviceNumber(int dev_num, struct list_head *proc_list) {
    pcb_t* temp_proc;
    int i = 0;
    list_for_each_entry(temp_proc, proc_list, p_list) {
        if (i == dev_num) {
            return outProcQ(proc_list, temp_proc);
        }

        i++;
    }
    return NULL;
}

void handleNonTimer(int line_num, int cause, state_t *exc_state) {
    devregarea_t *dev_reg_area = (devregarea_t *)BUS_REG_RAM_BASE;
    unsigned int intr_devices_bitmap = dev_reg_area->interrupt_dev[line_num - DEV_IL_START];
    unsigned int dev_status;
    unsigned int dev_num;

    if (intr_devices_bitmap & DEV7ON) dev_num = 7;
    else if (intr_devices_bitmap & DEV6ON) dev_num = 6;
    else if (intr_devices_bitmap & DEV5ON) dev_num = 5;
    else if (intr_devices_bitmap & DEV4ON) dev_num = 4;
    else if (intr_devices_bitmap & DEV3ON) dev_num = 3;
    else if (intr_devices_bitmap & DEV2ON) dev_num = 2;
    else if (intr_devices_bitmap & DEV1ON) dev_num = 1;
    else if (intr_devices_bitmap & DEV0ON) dev_num = 0;
    else return;

    pcb_t* unblocked_proc = NULL;

    if (line_num == IL_TERMINAL) {
        termreg_t *dev_reg = (termreg_t *)DEV_REG_ADDR(IL_TERMINAL, dev_num);
        if (((dev_reg->transm_status) & 0x000000FF) == 5) {
            dev_status = dev_reg->transm_status;
            dev_reg->transm_command = ACK;
            unblocked_proc = releaseProcessByDeviceNumber(dev_num, &blockedForTransm);
        } else {
            dev_status = dev_reg->recv_status;
            dev_reg->recv_command = ACK;
            unblocked_proc = releaseProcessByDeviceNumber(dev_num, &blockedForRecv);
        }
    } else {
        dtpreg_t *dev_reg = (dtpreg_t *)DEV_REG_ADDR(line_num, dev_num);
        dev_status = dev_reg->status;
        dev_reg->command = ACK;

        struct list_head *proc_list;

        if ((proc_list = &blockedForDevice[EXT_IL_INDEX(line_num)])) {
            unblocked_proc = releaseProcessByDeviceNumber(dev_num, proc_list);
        }
    }

    if (unblocked_proc) {
        unblocked_proc->p_s.reg_v0 = dev_status;
        sendMessage(ssi_pcb, unblocked_proc, (memaddr)(dev_status));
        insertProcQ(&ready_queue, unblocked_proc);
        softBlockCount--;
    }

    if (current_process) 
        LDST(exc_state);
    else 
        scheduler();
}

void handleLocalTimerInterrupt(state_t *exc_state) {
    setTIMER(-1);
    getDeltaTime(current_process);
    saveState(&(current_process->p_s), exc_state);
    insertProcQ(&ready_queue, current_process);
    scheduler();
}

void handlePseudoClockInterrupt(state_t* exc_state) {
    LDIT(PSECOND);
    pcb_t *unblocked_proc;
    while ((unblocked_proc = removeProcQ(&blockedForClock)) != NULL) {
        sendMessage(ssi_pcb, unblocked_proc, 0);
        insertProcQ(&ready_queue, unblocked_proc);
        softBlockCount--;
    }

    if (current_process) 
        LDST(exc_state);
    else 
        scheduler();
}

int getHighestPriorityNTint(int cause){
  for (int line = DEV_IL_START; line < N_INTERRUPT_LINES; line++){
    if (CAUSE_IP_GET(cause, line)){
      return line;
    }
  }
  return -1;
}

void interruptHandler(int cause, state_t *exc_state) {
    if (CAUSE_IP_GET(cause, IL_CPUTIMER)) {
        handleLocalTimerInterrupt(exc_state);
    } else if (CAUSE_IP_GET(cause, IL_TIMER)) {
        handlePseudoClockInterrupt(exc_state);
    } else{
        int line = getHighestPriorityNTint(cause);

        if(line != -1)
            handleNonTimer(line, cause, exc_state);
    }

   
}
