#include "const.h"
#include "dep.h"
#include "headers/pcb.h"
#include <umps3/umps/libumps.h>

LIST_HEAD(Locked_disk);
LIST_HEAD(Locked_flash);
LIST_HEAD(Locked_terminal_recv);
LIST_HEAD(Locked_terminal_transm);
LIST_HEAD(Locked_ethernet);
LIST_HEAD(Locked_printer);
LIST_HEAD(Locked_pseudo_clock);

int pid_counter;
cpu_t prevTOD;
unsigned int processCount, softBlockCount;
pcb_t *ssi_pcb;

struct list_head ready_queue;
struct list_head msg_queue_list;
pcb_t *current_process;

struct list_head blockedPCBs[SEMDEVLEN - 1];
struct list_head PseudoClockWP; // pseudo-clock waiting process

extern void test ();

void memcpy(void *dest, void *src, unsigned int n)  
{  
  // Typecast src and dest addresses to (char *)  
  char *csrc = (char *)src;  
  char *cdest = (char *)dest;  
    
  // Copy contents of src[] to dest[]  
  for (int i=0; i<n; i++)  
      cdest[i] = csrc[i];  
}

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
  STCK (prevTOD);
  return getTimeElapsed;
}

int main(){

  term_puts("entrato in initial\n");
 
  passupvector_t *passupvector = (passupvector_t *) PASSUPVECTOR;
  passupvector->tlb_refill_handler = (memaddr) uTLB_RefillHandler;
  passupvector->tlb_refill_stackPtr = (memaddr) KERNELSTACK;
  passupvector->exception_handler = (memaddr) exceptionHandler;
  passupvector->exception_stackPtr = (memaddr) KERNELSTACK;
  
  initPcbs();
  initMsgs();

  /*initialize all the previously declared variables*/
  processCount = 0;
  softBlockCount = 0;
  pid_counter = 0;
  mkEmptyProcQ(&ready_queue);
  current_process = NULL; 
  mkEmptyProcQ(&PseudoClockWP);
  mkEmptyProcQ(&msg_queue_list);

    mkEmptyProcQ(&Locked_disk);
    mkEmptyProcQ(&Locked_flash);
    mkEmptyProcQ(&Locked_terminal_recv);
    mkEmptyProcQ(&Locked_terminal_transm);
    mkEmptyProcQ(&Locked_ethernet);
    mkEmptyProcQ(&Locked_printer);
    mkEmptyProcQ(&Locked_pseudo_clock);

  for(int i = 0; i < SEMDEVLEN - 1; i++) {
    INIT_LIST_HEAD(&blockedPCBs[i]);
  }
  
  /*Load the system-wide Interval Timer with 100 
    milliseconds (constant PSECOND)*/
  LDIT(PSECOND);

  /*Instantiate a first process*/
  /*instantiate ssi_pcb*/

  ssi_pcb = allocPcb();

  ssi_pcb->p_s.status = ALLOFF | IEPON | IMON | TEBITON;

  RAMTOP(ssi_pcb->p_s.reg_sp);
  /*PC set to the address of SSI_function_entry_point*/
  ssi_pcb->p_s.pc_epc = (memaddr) SSI_function_entry_point;
  /*henever one assigns a value to the PC one must also assign the
  same value to the general purpose register t9 */
  ssi_pcb->p_s.reg_t9 =  (memaddr) SSI_function_entry_point;

  insertProcQ(&ready_queue, ssi_pcb);

  /*instantiate a second process*/

  pcb_t *entryTest = allocPcb ();
  
  /*interrupts enabled, the processor Local Timer enabled, kernel-mode on*/
  // not sure about this, what status register should be set to?
  entryTest->p_s.status = ALLOFF | IECON | IEPON | TEBITON;

  /* SP set to RAMTOP - (2 * FRAMESIZE) (i.e.
    use the last RAM frame for its stack minus 
    the space needed by the first process*/
  unsigned int ramTop;

  RAMTOP(ramTop);

  entryTest->p_s.reg_sp = ramTop - (2 * PAGESIZE);

  entryTest->p_s.pc_epc = (memaddr) test;
  entryTest->p_s.reg_t9 = (memaddr) test;

  insertProcQ (&ready_queue, entryTest);
  
  processCount = 2;
  
  scheduler (); 

  return 0;

}
