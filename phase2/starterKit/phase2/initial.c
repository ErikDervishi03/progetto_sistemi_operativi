#include "dep.h"
#include <umps3/umps/types.h>

// Initialize various ready and locked queues
LIST_HEAD(Ready_Queue);
LIST_HEAD(Locked_disk);
LIST_HEAD(Locked_flash);
LIST_HEAD(Locked_terminal_recv);
LIST_HEAD(Locked_terminal_transm);
LIST_HEAD(Locked_ethernet);
LIST_HEAD(Locked_printer);
LIST_HEAD(Locked_pseudo_clock);

extern void test();

void memcpy(void *dest, void *src, unsigned int n)  
{  
  // Typecast src and dest addresses to (char *)  
  char *csrc = (char *)src;  
  char *cdest = (char *)dest;  
    
  // Copy contents of src[] to dest[]  
  for (int i=0; i<n; i++)  
      cdest[i] = csrc[i];  
}

// Global process counters and pointers
unsigned int processCount = 0;
unsigned int softBlockCount = 0;
int pid_counter = 3;    
pcb_t *current_process = NULL;
pcb_t *ssi_pcb = NULL;

// TLB refill handler
void uTLB_RefillHandler() {
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST((state_t*) 0x0FFFF000);
}

int main(int argc, char const *argv[]) {
    // Set up the pass-up vector
    passupvector_t *psv = (passupvector_t *)PASSUPVECTOR;
    psv->tlb_refill_handler  = (memaddr)uTLB_RefillHandler; 
    psv->tlb_refill_stackPtr = (memaddr)KERNELSTACK;
    psv->exception_handler   = (memaddr)exceptionHandler; 
    psv->exception_stackPtr  = (memaddr)KERNELSTACK;

    // Initialize process control blocks and message queues
    initPcbs();
    initMsgs();

    // Set interval timer
    LDIT(PSECOND);

    // Initialize the ready queue and other device queues
    mkEmptyProcQ(&Ready_Queue);
    mkEmptyProcQ(&Locked_disk);
    mkEmptyProcQ(&Locked_flash);
    mkEmptyProcQ(&Locked_terminal_recv);
    mkEmptyProcQ(&Locked_terminal_transm);
    mkEmptyProcQ(&Locked_ethernet);
    mkEmptyProcQ(&Locked_printer);
    mkEmptyProcQ(&Locked_pseudo_clock);

    // Set up the first system startup process (SSI)
    ssi_pcb = allocPcb();
    ssi_pcb->p_pid = 1;
    ssi_pcb->p_supportStruct = NULL;
    ssi_pcb->p_s.status = ALLOFF | IEPON | IMON | TEBITON;
    RAMTOP(ssi_pcb->p_s.reg_sp);
    ssi_pcb->p_s.pc_epc = (memaddr) SSI_function_entry_point;
    ssi_pcb->p_s.reg_t9 = ssi_pcb->p_s.pc_epc;
    insertProcQ(&Ready_Queue, ssi_pcb);
    processCount++;

    // Set up the second process
    pcb_t *secondProc = allocPcb();
    secondProc->p_pid = 2;
    secondProc->p_supportStruct = NULL;
    secondProc->p_s.status = ALLOFF | IEPON | IMON | TEBITON;
    RAMTOP(secondProc->p_s.reg_sp);
    secondProc->p_s.reg_sp -= (2 * PAGESIZE); // Adjust stack pointer for second process
    secondProc->p_s.pc_epc = (memaddr) test;
    secondProc->p_s.reg_t9 = secondProc->p_s.pc_epc;
    insertProcQ(&Ready_Queue, secondProc);
    processCount++;

    // Start the scheduler
    scheduler();
}
