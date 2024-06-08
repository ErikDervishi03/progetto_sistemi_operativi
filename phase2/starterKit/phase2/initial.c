#include "dep.h"
#include <umps3/umps/const.h>

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

LIST_HEAD(Ready_Queue);
LIST_HEAD(Locked_disk);
LIST_HEAD(Locked_flash);
LIST_HEAD(Locked_terminal_recv);
LIST_HEAD(Locked_terminal_transm);
LIST_HEAD(Locked_ethernet);
LIST_HEAD(Locked_printer);
LIST_HEAD(Locked_pseudo_clock);

int processCount, softBlockCount, currPid;
cpu_t prevTod;

pcb_t *current_process;
pcb_t *ssi_pcb;

void main(int argc, char const *argv[]) {
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

    mkEmptyProcQ(&Ready_Queue);
    mkEmptyProcQ(&Locked_disk);
    mkEmptyProcQ(&Locked_flash);
    mkEmptyProcQ(&Locked_terminal_recv);
    mkEmptyProcQ(&Locked_terminal_transm);
    mkEmptyProcQ(&Locked_ethernet);
    mkEmptyProcQ(&Locked_printer);
    mkEmptyProcQ(&Locked_pseudo_clock);

    LDIT(PSECOND);

    ssi_pcb = allocPcb();
    ssi_pcb->p_pid = 1;
    ssi_pcb->p_supportStruct = NULL;
    ssi_pcb->p_s.status = ALLOFF | IEPON | IMON | TEBITON;
    RAMTOP(ssi_pcb->p_s.reg_sp);
    ssi_pcb->p_s.pc_epc = (memaddr) SSILoop;
    ssi_pcb->p_s.gpr[24] = ssi_pcb->p_s.pc_epc;
    insertProcQ(&Ready_Queue, ssi_pcb);
    processCount++;

    pcb_t *toTest = allocPcb();
    toTest->p_pid = 2;
    toTest->p_supportStruct = NULL;
    toTest->p_s.status = ALLOFF | IEPON | IMON | TEBITON;
    RAMTOP(toTest->p_s.reg_sp);
    toTest->p_s.reg_sp -= (2 * PAGESIZE);
    toTest->p_s.pc_epc = (memaddr) test;
    toTest->p_s.gpr[24] = toTest->p_s.pc_epc;
    insertProcQ(&Ready_Queue, toTest);
    processCount++;

    scheduler();
}
