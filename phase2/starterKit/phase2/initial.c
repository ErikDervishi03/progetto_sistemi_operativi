#include "dep.h"
#include <umps3/umps/const.h>

void memcpy(void *dest, void *src, unsigned int n)  
{  
  // Typecast src and dest addresses to (char *)  
  char *csrc = (char *)src;  
  char *cdest = (char *)dest;  
    
  // Copy contents of src[] to dest[]  
  for (int i=0; i<n; i++)  
      cdest[i] = csrc[i];  
}

void getDeltaTime(pcb_t *p) {
    int currTod;
    STCK(currTod);
    p->p_time += (currTod - prevTod);
    prevTod = currTod;
}

void uTLB_RefillHandler() {
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST((state_t*) 0x0FFFF000);
}

extern void test();

LIST_HEAD(ready_queue);
LIST_HEAD(blockedForDisk);
LIST_HEAD(blockedForFlash);
LIST_HEAD(blockedForRecv);
LIST_HEAD(blockedForTransm);
LIST_HEAD(blockedForEthernet);
LIST_HEAD(blockedForPrinter);
LIST_HEAD(blockedForClock);

int processCount, softBlockCount, currPid;
cpu_t prevTod;

pcb_t *current_process;
pcb_t *ssi_pcb;

int main(int argc, char const *argv[]) {
    passupvector_t *passupv;
    passupv = (passupvector_t *) PASSUPVECTOR;
    passupv->tlb_refill_handler = (memaddr) uTLB_RefillHandler;
    passupv->tlb_refill_stackPtr = (memaddr) KERNELSTACK;
    passupv->exception_handler = (memaddr) exceptionHandler;
    passupv->exception_stackPtr = (memaddr) KERNELSTACK;

    initPcbs();
    initMsgs();

    processCount = 0;
    softBlockCount = 0;
    currPid = 3;

    mkEmptyProcQ(&ready_queue);
    mkEmptyProcQ(&blockedForDisk);
    mkEmptyProcQ(&blockedForFlash);
    mkEmptyProcQ(&blockedForRecv);
    mkEmptyProcQ(&blockedForTransm);
    mkEmptyProcQ(&blockedForEthernet);
    mkEmptyProcQ(&blockedForPrinter);
    mkEmptyProcQ(&blockedForClock);

    LDIT(PSECOND);

    ssi_pcb = allocPcb();
    ssi_pcb->p_pid = 1;
    ssi_pcb->p_supportStruct = NULL;
    ssi_pcb->p_s.status = ALLOFF | IEPON | IMON | TEBITON;
    RAMTOP(ssi_pcb->p_s.reg_sp);
    ssi_pcb->p_s.pc_epc = (memaddr) SSI_entry_point;
    ssi_pcb->p_s.gpr[24] = ssi_pcb->p_s.pc_epc;
    insertProcQ(&ready_queue, ssi_pcb);
    processCount++;

    pcb_t *toTest = allocPcb();
    toTest->p_pid = 2;
    toTest->p_supportStruct = NULL;
    toTest->p_s.status = ALLOFF | IEPON | IMON | TEBITON;
    RAMTOP(toTest->p_s.reg_sp);
    toTest->p_s.reg_sp -= (2 * PAGESIZE);
    toTest->p_s.pc_epc = (memaddr) test;
    toTest->p_s.gpr[24] = toTest->p_s.pc_epc;
    insertProcQ(&ready_queue, toTest);
    processCount++;

    scheduler();
}
