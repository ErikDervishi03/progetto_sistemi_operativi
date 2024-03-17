#include "dep.h"

// DA FARE : file di header

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

void interruptHandler() {
  startInterrupt();
  // ...
  endInterrupt();
}




