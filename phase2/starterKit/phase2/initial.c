/*Implement the main function and export the Nucleus's global variables, such as
process count, soft-blocked count, blocked PCBs lists/pointers, etc.*/
#include "dep.h"
/* GLOBAL VARIABLES*/
// started but not terminated processes
unsigned int processCount;
// processes waiting for a resource
unsigned int softBlockCount;
// tail pointer to the ready state queue processes
struct list_head ready_queue;
pcb_t *current_process;
struct list_head blockedPCBs[SEMDEVLEN - 1]; 
// waiting for a message
struct list_head msg_queue;
// pcb waiting for clock tick
struct list_head PseudoClockWP;
// SSI process
pcb_t *ssi_pcb;
//accumulated CPU time
cpu_t prevTOD;
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

int main(int argc, char *argv[])
{
    // 1. Initialize the nucleus
    initKernel();

    // 2. scheduler
    scheduler();

    return 0;
}

void initKernel() {
  // block interrupts
  //setSTATUS(ALLOFF);
  //setMIE(ALLOFF);

  passupvector_t *passupvector = (passupvector_t *)PASSUPVECTOR;
  // populate the passup vector
  passupvector->tlb_refill_handler = (memaddr)uTLB_RefillHandler; 
  passupvector->tlb_refill_stackPtr = (memaddr)KERNELSTACK;
  passupvector->exception_handler =
      (memaddr)exceptionHandler; 
  passupvector->exception_stackPtr = (memaddr)KERNELSTACK;
  
  // initialize the nucleus data structures
  initPcbs();
  initMsgs();

  // Initialize other variables
  softBlockCount = 0;
  processCount = 0;

  INIT_LIST_HEAD(&ready_queue);
  for (int i = 0; i < SEMDEVLEN-1; i++) {
    INIT_LIST_HEAD(&blockedPCBs[i]);
  }
  INIT_LIST_HEAD(&PseudoClockWP);
  current_process = NULL;

  INIT_LIST_HEAD(&msg_queue);
  
  // load the system wide interval timer
  LDIT(PSECOND);

  // init the first process
  
  ssi_pcb = allocPcb();

  ssi_pcb->p_s.status = ALLOFF | IEPON | IMON | TEBITON;

  RAMTOP(ssi_pcb->p_s.reg_sp);
  /*PC set to the address of SSI_function_entry_point*/
  ssi_pcb->p_s.pc_epc = (memaddr) SSI_function_entry_point;
  /*henever one assigns a value to the PC one must also assign the
  same value to the general purpose register t9 */
  ssi_pcb->p_s.reg_t9 =  (memaddr) SSI_function_entry_point;

  insertProcQ(&ready_queue, ssi_pcb);

  ssi_pcb->p_pid = SSIPID;

  processCount++;

  pcb_t *second_process = allocPcb();

  RAMTOP(second_process->p_s.reg_sp); // Set SP to RAMTOP - 2 * FRAME_SIZE
  second_process->p_s.reg_sp -= 2 * PAGESIZE; 
  second_process->p_s.pc_epc = (memaddr)test; 
  second_process->p_s.status = ALLOFF | IEPON | IMON | TEBITON;
  second_process->p_s.reg_t9 = (memaddr)test;
  processCount++; 

  insertProcQ(&ready_queue, second_process);
}


void copyState(state_t *source, state_t *dest) {
    dest->entry_hi = source->entry_hi;
    dest->cause = source->cause;
    dest->status = source->status;
    dest->pc_epc = source->pc_epc;
    dest->reg_t9= source->reg_t9;
    for (unsigned i = 0; i < 32; i++){
        dest->gpr[i] = source->gpr[i];
    }
}

cpu_t deltaTime(void){
  cpu_t current_time_TOD;
  return (STCK(current_time_TOD) - prevTOD);
}