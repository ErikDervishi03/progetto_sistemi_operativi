#include "const.h"
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

// aggiorno p_time con il tempo trascorso dall'ultimo TOD
void getDeltaTime(pcb_t *p) {
    int currTod;
    STCK(currTod);
    p->p_time += (currTod - prevTod);
    prevTod = currTod;
}

LIST_HEAD(ready_queue);  // coda dei processi "ready"
LIST_HEAD(blockedForClock); // coda dei processi bloccati per clock


struct list_head blockedForDevice[NDEV]; // coda dei processi bloccati per device

int processCount, // numero di processi startati, ma non ancora terminati
softBlockCount,   // numero di processi startati, ma non ancora terminati e bloccati a causa di un I/O o timer request
currPid;          // pid del prossimo processo da creare

cpu_t prevTod;    // tempo dell'ultimo TOD

pcb_t *current_process;  // puntatore al pcb in "running" 
pcb_t *ssi_pcb;         // puntatore al pcb del processo SSI

int main(int argc, char const *argv[]) {
    passupvector_t *passupv; // passup vector: parte del BIOS data page, qui il BIOS trova gli indirizzi alle funzioni per passare il controllo per gestire gli eventi
    passupv = (passupvector_t *) PASSUPVECTOR; // indirizzo del passup vector: 0x0FFF.F900
    passupv->tlb_refill_handler = (memaddr) uTLB_RefillHandler;
    passupv->tlb_refill_stackPtr = (memaddr) KERNELSTACK; // top of the kernel stack: 0x2000.1000
    passupv->exception_handler = (memaddr) exceptionHandler;
    passupv->exception_stackPtr = (memaddr) KERNELSTACK; // top of the kernel stack: 0x2000.1000

    // inizializzo le strutture dati
    initPcbs();
    initMsgs();

    // inizializzo le variabili globali
    processCount = 0;
    softBlockCount = 0;
    currPid = 3;
    
    // inizializzo le code dei processi bloccati per device
    for(int i = 0; i < NDEV; i++){
        INIT_LIST_HEAD(&blockedForDevice[i]);
    }

    //carico l'Interval Timer con il valore di PSECOND: 100 ms
    LDIT(PSECOND);

    // creo il processo SSI
    ssi_pcb = allocPcb(); // alloco un pcb per il processo SSI
    ssi_pcb->p_pid = 1;   // assegno il pid 1 al processo SSI   
    ssi_pcb->p_supportStruct = NULL; // il processo SSI non ha bisogno di supporto
    ssi_pcb->p_s.status = ALLOFF | IEPON | IMON | TEBITON; // abilito gli interrupt, kernel mode on
    RAMTOP(ssi_pcb->p_s.reg_sp); // imposto lo stack pointer del processo SSI al top dello stack della RAM
    ssi_pcb->p_s.pc_epc = (memaddr) SSI_entry_point; // imposto il program counter del processo SSI all'entry point dell'SSI
    ssi_pcb->p_s.gpr[24] = ssi_pcb->p_s.pc_epc; // per questioni tecniche assegno il valore del PC al general purpose register t9
    insertProcQ(&ready_queue, ssi_pcb);  // inserisco il processo SSI nella coda dei processi "ready"
    processCount++;  // incremento il numero di processi startati

    // creo il processo test
    pcb_t *toTest = allocPcb(); // alloco un pcb per il processo test
    toTest->p_pid = 2; // assegno il pid 2 al processo test
    toTest->p_supportStruct = NULL;  // il processo test non ha bisogno di supporto
    toTest->p_s.status = ALLOFF | IEPON | IMON | TEBITON; // abilito gli interrupt, kernel mode on
    RAMTOP(toTest->p_s.reg_sp);  // imposto lo stack pointer del processo test al top dello stack della RAM
    toTest->p_s.reg_sp -= (2 * PAGESIZE);  // sposto lo stack pointer a due pagine prima, le prime due sono riservate al SSI
    toTest->p_s.pc_epc = (memaddr) test;   // imposto il program counter del processo test all'entry point del test
    toTest->p_s.gpr[24] = toTest->p_s.pc_epc;
    insertProcQ(&ready_queue, toTest);  // inserisco il processo test nella coda dei processi "ready"
    processCount++;   // incremento il numero di processi startati
 
    scheduler();   // chiamo lo scheduler

    /* 
    *  una volta chiamato lo scheduler non si tornerà più nel main() 
    *  l'unico modo per tornare nel kernel è tramite una exception o un device interrupt
    *  al tempo di boot/reset time il kernel viene caricato in RAM a partire del secondo frame (il primo è riservato al kernel stack)
    *  Processor 0 sarà in kernel mode con interrupt mascherati e Local Timer disabilitato
    */
}
