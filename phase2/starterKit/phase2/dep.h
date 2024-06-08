#ifndef DEP_H
#define DEP_H

#include <umps3/umps/libumps.h>
#include <umps3/umps/types.h>
#include <umps3/umps/const.h>
#include <umps3/umps/arch.h>
#include <umps3/umps/cp0.h>
#include "const.h"
#include "headers/pcb.h"
#include "headers/msg.h"
#include "debug.h"

extern cpu_t start;
extern int process_count;

extern pcb_t* prova;

extern struct list_head Ready_Queue;
extern struct list_head Locked_disk;
extern struct list_head Locked_flash;
extern struct list_head Locked_terminal_recv;
extern struct list_head Locked_terminal_transm;
extern struct list_head Locked_ethernet;
extern struct list_head Locked_printer;
extern struct list_head Locked_pseudo_clock;

extern int pid_counter;
extern pcb_t *current_process;

extern struct list_head blockedPCBs[SEMDEVLEN - 1];
extern struct list_head PseudoClockWP; // pseudo-clock waiting process

extern int soft_blocked_count;

extern pcb_t *ssi_pcb;

extern struct list_head pcbFree_h;

extern void terminate_process(pcb_t *arg);

extern void memcpy(void *dest, void *src, unsigned int n);

extern cpu_t getTimeElapsed ();

extern void interruptHandler(); 

extern void scheduler();

extern void exceptionHandler ();

extern void SSI_function_entry_point ();

extern void getRemainTime(pcb_t *target);

extern pcb_PTR print_pcb;

extern struct list_head msg_queue_list;

extern void ssi_terminate_process(pcb_t* proc);

extern unsigned int SSIRequest(pcb_t* sender, ssi_payload_t *payload);

extern void saveState(state_t* dest, state_t* to_copy);

extern void SSILoop();

extern int send(pcb_t *sender, pcb_t *dest, unsigned int payload);

extern unsigned int getTOD();

extern void updateCPUtime(pcb_t *p) ;

extern void setIntervalTimer(unsigned int t);

extern void setPLT(unsigned int t);

extern unsigned int getPLT() ;

#endif