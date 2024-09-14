#ifndef VMSUPPORT_H
#define VMSUPPORT_H

#include "../../phase2/starterKit/src/dep.h"
#include "./initProc.h"


#define READ 0
#define WRITE 1

extern pcb_t* current_process;
extern pcb_t* ssi_pcb;

void uTLB_RefillHandler();
void pager();

#endif