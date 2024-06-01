#include "const.h"
#include "dep.h"
#include <umps3/umps/libumps.h>

/*
A device or timer interrupt occurs when:
  a)a previously initiated I/O request completes
  b)a Processor Local Timer (PLT) or 
    the Interval Timer makes a 0x0000.0000 ⇒0xFFFF.FFFF transition
*/
void startInterrupt(){
  if (current_process != NULL)
    /* updating the execution time of the current process so that the 
       time spent handling the interrupt is not attributed to it*/
    current_process->p_time += getTimeElapsed ();

  /*
    clear the Timer Enable Bit (TEBIT) in the STATUS 
    register to disable further timer interrupts while handling 
    the current interrupt
  */
  setSTATUS (getSTATUS () & ~TEBITON);
}

void endInterrupt ()
{
  setSTATUS (getSTATUS () | TEBITON);
  STCK (prevTOD);
  if (current_process != NULL)
    LDST ((state_t *)BIOSDATAPAGE);
  else
    scheduler ();
}


void handlePLT(){
  /*here we are loading the timer with a new value to acknowledge the PLT interrupt */
  current_process->p_time += getTimeElapsed ();

  /*Copy the processor state at the time of the exception (located at the start of the BIOS Data
  Page [Section 3.2.2-pops]) into the Current Process’s PCB (p_s)*/

  state_t *p_state = (state_t *)BIOSDATAPAGE;
  current_process->p_s = *p_state;
  
  /*Place the Current Process on the Ready Queue; transitioning the Current Process from the
  “running” state to the “ready” state*/
  insertProcQ(&ready_queue, current_process);
  current_process = NULL;

  /*Call the Scheduler*/
  scheduler ();

}

void handleIntervalTimer(){
  
  /*Acknowledge the interrupt by loading the Interval Timer with a new value: 100 milliseconds
  (constant PSECOND)*/
  LDIT(PSECOND);

  struct list_head* iter;

  /*Unblock all PCBs blocked waiting a Pseudo-clock tick*/
  list_for_each(iter,&PseudoClockWP) {
    pcb_t* item = container_of(iter,pcb_t,p_list);

    insertProcQ(&ready_queue, item);

    list_del(iter);

    softBlockCount--;// ?
  }

  /*Return control to the Current Process: perform a LDST on the saved exception state (located at
  the start of the BIOS Data Page*/
  LDST((state_t *)BIOSDATAPAGE);
}

int getDeviceNumber (int line)
{
  devregarea_t *bus_reg_area = (devregarea_t *)BUS_REG_RAM_BASE;
  unsigned int bitmap = bus_reg_area->interrupt_dev[EXT_IL_INDEX (line)];
  for (int number = 0, mask = 1; number < N_DEV_PER_IL; number++, mask <<= 1)
    if (bitmap & mask)
      return number;
  return -1;
}

int getHighestPriorityNTint(){
  for (int line = DEV_IL_START; line < N_INTERRUPT_LINES; line++){
    if (getCAUSE() & (1 << line)){
      return line;
    }
  }
  return -1;
}


void handleNonTimer(){
  
  /*Calculate the address for this device’s device register [Section 5.1-pops]:
  devAddrBase = 0x10000054 + ((IntlineNo - 3) * 0x80) + (DevNo * 0x10)
  Tip: to calculate the device number you can use a switch among constants DEVxON */

  int intlineNo = getHighestPriorityNTint();

  if(intlineNo == -1)
    return;

  int devNo = getDeviceNumber(intlineNo);

  if(devNo == -1)
    return;

  unsigned int devAddrBase = DEV_REG_ADDR(intlineNo, devNo);

  /*Save off the status code from the device’s device register*/

  devregarea_t *bus_reg_area = (devregarea_t *)BUS_REG_RAM_BASE;
  devreg_t *devReg = &bus_reg_area->devreg[intlineNo - DEV_IL_START][devNo];
  unsigned int status = devReg->dtp.status;

  /*Acknowledge the outstanding interrupt. This is accomplished by writing the acknowledge com-
  mand code (ACK) in the interrupting device’s device register. Alternatively, writing a new com-
  mand in the interrupting device’s device register will also acknowledge the interrupt.*/

  devReg->dtp.command = ACK;
  //devReg->term.recv_command = ACK; this is done automatically cause it's a union (go see devreg_t in types.h)

  /*Send a message and unblock the PCB waiting the status response from this (sub)device. This
  operation should unblock the process (PCB) which initiated this I/O operation and then re-
  quested the status response via a SYS2 operation.
  Important: Use of SYSCALL is discourage because both use BIOSDATAPAGE*/

  pcb_t *waitingProcess = NULL;//blockedPCBs[(intlineNo - DEV_IL_START) * N_DEV_PER_IL + devNo]; 
  
  if(waitingProcess != NULL){
    /*Place the stored off status code in the newly unblocked PCB’s v0 register*/
    waitingProcess->p_s.reg_v0 = status;
    /*Insert the newly unblocked PCB on the Ready Queue, transitioning this process from the
    “blocked” state to the “ready” state*/
    insertProcQ(&ready_queue, waitingProcess);
    //blockedPCBs[(intlineNo - 2) * N_DEV_PER_IL + devNo] = NULL; va fatto?

  }

  if(current_process == NULL)
    scheduler();
  else
    LDST((state_t *)BIOSDATAPAGE);
}

void interruptHandler() {

  term_puts("dentro l'interrupt handler\n");

  startInterrupt();

  if (getCAUSE() & LOCALTIMERINT){
    handlePLT();
  }

  else if(getCAUSE() & TIMERINTERRUPT){
    handleIntervalTimer();
  }

  else {
    handleNonTimer();
  }
  endInterrupt();
}

