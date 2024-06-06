#include <umps3/umps/libumps.h>
#include <umps3/umps/types.h>
#include <umps3/umps/const.h>
#include <umps3/umps/arch.h>
#include <umps3/umps/cp0.h>
#include "const.h"
#include "headers/pcb.h"
#include "headers/msg.h"
#include "debug.h"


#define SSIPID 0xFFFFFFFE 
extern cpu_t prevTOD;
extern unsigned int processCount, softBlockCount;
extern cpu_t time_interrupt_start;
extern pcb_t* prova;

extern struct list_head ready_queue;
extern struct list_head msg_queue;
extern pcb_t *current_process;

extern struct list_head blockedPCBs[SEMDEVLEN - 1];
extern struct list_head PseudoClockWP; // pseudo-clock waiting process

extern pcb_t *ssi_pcb;

extern struct list_head pcbFree_h;

extern void terminate_process(pcb_t *arg);

extern void memcpy(void *dest, void *src, unsigned int n);

extern cpu_t getTimeElapsed ();

extern void interruptHandler(); 

extern void scheduler();

extern void exceptionHandler ();

extern void SSI_function_entry_point ();

extern cpu_t getTimeElapsed ();

extern pcb_PTR print_pcb;