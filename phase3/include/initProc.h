#ifndef INITPROC
#define INITPROC

#include "../../phase2/starterKit/src/dep.h"
#include "../../phase3/include/sst.h"
#include "./sysSupport.h"
#include "./vmSupport.h"


#define TERM0ADDR 0x10000254
#define TERMSTATMASK 0xFF
#define PRINTER0ADDR 0x100001D4
#define PRINTCHR 2
#define TERMSTATMASK 0xFF
#define RECVD 5
#define READY 1
#define SWAP_POOL_AREA 0x20020000
#define IEPBITON 0x4
#define CAUSEINTMASK 0xFD00

void test();
pcb_t *create_process(state_t *s, support_t *sup);


extern support_t structures[UPROCMAX]; 
extern state_t UProc_state[UPROCMAX];
extern swap_t swap_pool_table[POOLSIZE];
extern pcb_t *sst_pcbs[UPROCMAX];
extern pcb_t *terminal_pcbs[UPROCMAX];
extern pcb_t *printer_pcbs[UPROCMAX];

extern pcb_t *swap_mutex_pcb;
extern pcb_t *swap_mutex_process;
extern pcb_t *mutex_holder;

extern pcb_t *test_pcb;


#endif