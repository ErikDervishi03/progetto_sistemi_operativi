#include "../headers/const.h"
#include <umps3/umps/libumps.h>

unsigned process_count, soft_block_count;

pcb_t *ready_queue;
struct list_head current_process;

// pcb_t blocked_pcbs[SEMDEVLEN - 1]; 
extern void test ();

void uTLB_RefillHandler() {
  setENTRYHI(0x80000000);
  setENTRYLO(0x00000000);
  TLBWR();
  LDST((state_t*) 0x0FFFF000);
}

int main(){

  uTLB_RefillHandler();

  initPcbs();
  initMsgs();

  process_count = 0;
  soft_block_count = 0;
  mkEmptyProcQ(&ready_queue);
  current_process = NULL; // dubbio

  LDIT(PSECOND);

  pcb_t init = allocPcb ();
  
  init->p_s.status
    = ALLOFF | IMON | IEPON
      | TEBITON;

  RAMTOP(init->p_s.s_sp);
  
  /*Set all the Process Tree fields to NULL*/
  init->p_parent = NULL;
  init->p_child = NULL;
  init->p_sib = NULL;

  /*Set the accumulated time field (p_time) to zero*/
  init->p_time = 0;

  /*Set the Support Structure pointer (p_supportStruct) to NULL*/
  init->p_supportStruct = NULL;


  insertProcQ (&readyQueue, init);
  processCount++;
  scheduler ();

  return 0;

}

