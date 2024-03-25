#include "dep.h"

/*
A device or timer interrupt occurs when:
  a)a previously initiated I/O request completes
  b)a Processor Local Timer (PLT) or 
    the Interval Timer makes a 0x0000.0000 ⇒0xFFFF.FFFF transition
*/

void startInterrupt(){
  if (currentProcess != NULL)
    /* updating the execution time of the current process so that the 
       time spent handling the interrupt is not attributed to it*/
    currentProcess->p_time += getTimeElapsed ();

  /*
    clear the Timer Enable Bit (TEBIT) in the STATUS 
    register to disable further timer interrupts while handling 
    the current interrupt
  */
  setSTATUS (getSTATUS () & ~TEBITON);
}

void endInterrupt(){
  /*
    set the Timer Enable Bit (TEBIT) in the STATUS 
    register to enable further timer interrupts
  */
  setSTATUS (getSTATUS () | TEBITON);

  // ...
}

void handlePLT(){

  /*here we areloading the timer with a new value to acknowledge the PLT interrupt */
  currentProcess->p_time += getTimeElapsed ();

  /*Copy the processor state at the time of the exception (located at the start of the BIOS Data
  Page [Section 3.2.2-pops]) into the Current Process’s PCB (p_s)*/
  currentProcess->p_s = *( (state_t*) BIOSDATAPAGE);
  
  /*Place the Current Process on the Ready Queue; transitioning the Current Process from the
  “running” state to the “ready” state*/
  insertProcQ(&ready_queue, currentProcess);
  currentProcess = NULL;

  /*Call the Scheduler*/
  sheduler ();

}

void handleIntervalTimer(){
  /*Acknowledge the interrupt by loading the Interval Timer with a new value: 100 milliseconds
  (constant PSECOND)*/
  LDIT(PSECOND);

  struct list_head* iter;

  /*Unblock all PCBs blocked waiting a Pseudo-clock tick*/
  list_for_each(iter,&BlockedPCBs) {
    pcb_t* item = container_of(iter,pcb_t,p_list);

    insertProcQ(&ready_queue, item);

    list_del(iter);

    softBlockCount--;
  }
  /*Return control to the Current Process: perform a LDST on the saved exception state (located at
  the start of the BIOS Data Page*/
  LDST((void *)BIOSDATAPAGE);
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

int highestPrioityNTint(){
  for (int line = 3; line < 8; line++){
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

  int intlineNo = highestPrioityNTint();

  int devNo = getDeviceNumber(intlineNo);

  unsigned int devAddrBase = 0x10000054 + ((intlineNo - 3) * 0x80) + (devNo * 0x10);

  /*Save off the status code from the device’s device register*/

  devregarea_t *statusCode = (devregarea_t *)BUS_REG_RAM_BASE->devreg[intlineNo][devNo];

  /*Acknowledge the outstanding interrupt. This is accomplished by writing the acknowledge com-
  mand code (ACK) in the interrupting device’s device register. Alternatively, writing a new com-
  mand in the interrupting device’s device register will also acknowledge the interrupt.*/

  statusCode->term.recv_command = ACK;

  LDST((void *)BIOSDATAPAGE);
}

void interruptHandler() {

  startInterrupt();

  if (getCAUSE() & LOCALTIMERINT){
    handlePLT();
  }

  else if(getCAUSE() & TIMERINTERRUPT){
    handleIntervalTimer();
  }

  else {
    //handleNonTimer();
  }
  endInterrupt();
}

