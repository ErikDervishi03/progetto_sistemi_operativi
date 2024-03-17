#include "dep.h"

cpu_t prevTOD;
unsigned int processCount, softBlockCount;

struct list_head ready_queue;
pcb_t *currentProcess;

// pcb_t blocked_pcbs[SEMDEVLEN - 1]; 
extern void test ();

void uTLB_RefillHandler() {
  setENTRYHI(0x80000000);
  setENTRYLO(0x00000000);
  TLBWR();
  LDST((state_t*) 0x0FFFF000);
}

cpu_t getTimeElapsed () {
  cpu_t currTOD;
  STCK (currTOD);
  cpu_t getTimeElapsed = currTOD - prevTOD;
  // update prevTOD
  STCK (prevTOD);
  return getTimeElapsed;
}


void set_ramaining_PCBfield(pcb_t *p) {
  /*Set all the Process Tree fields to NULL*/
  p->p_parent = NULL;
  INIT_LIST_HEAD(&p->p_child);
  INIT_LIST_HEAD(&p->p_sib);

  /*Set the accumulated time field (p_time) to zero*/
  p->p_time = 0;

  /*Set the Support Structure pointer (p_supportStruct) to NULL*/
  p->p_supportStruct = NULL;
}

int main(){

  passupvector_t *passupvector = (passupvector_t *) PASSUPVECTOR;
  // passupvector->tlb_refill_handler = (memaddr) uTLB_RefillHandler;
  passupvector->tlb_refill_stackPtr = (memaddr) KERNELSTACK;
  // passupvector->exception_handler = (memaddr) exceptionHandler;
  passupvector->exception_stackPtr = (memaddr) KERNELSTACK;

  uTLB_RefillHandler(); // should i call this?

  initPcbs();
  initMsgs();

  /*initialize all the previously declared variables*/
  processCount = 0;
  softBlockCount = 0;
  mkEmptyProcQ(&ready_queue);
  current_process = NULL; 

  /*Load the system-wide Interval Timer with 100 
    milliseconds (constant PSECOND)*/
  LDIT(PSECOND);

  /*Instantiate a first process*/
  pcb_t *first_p = allocPcb ();
  
  insertProcQ (&ready_queue, first_p);
  processCount++;

  // not sure about this, what status register should be set to?
  first_p->p_s.status = ALLOFF | IECON;

  /*set SP to RAMTOP*/
  RAMTOP(first_p->p_s.reg_sp); 
  
  /*PC set to the address of SSI_function_entry_point*/
  first_p->p_s.pc_epc = /*address of SSI_function_entry_point (messa a 0 per non avere errori)*/ 0;
  first_p->p_s.reg_t9 = /*henever one assigns a value to the PC one must also assign the
                          same value to the general purpose register t9 
                          (messa a 0 per non avere errori)*/ 0;

  set_ramaining_PCBfield(first_p);

  /*instantiate a second process*/

  pcb_t *second_p = allocPcb ();

  insertProcQ (&ready_queue, second_p);
  processCount++;

  /*interrupts enabled, the processor Local Timer enabled, kernel-mode on*/
  // not sure about this, what status register should be set to?
  second_p->p_s.status = ALLOFF | IECON | TEBITON;

  /* SP set to RAMTOP - (2 * FRAMESIZE) (i.e.
    use the last RAM frame for its stack minus 
    the space needed by the first process*/
  // how can i find the space needed by the first process?
  second_p->p_s.reg_sp = RAMTOP(second_p->p_s.reg_sp) - (2 * PAGESIZE);

  second_p->p_s.pc_epc = (memaddr) test;

  set_ramaining_PCBfield(second_p);

  scheduler (); // TO DO

  return 0;

}

