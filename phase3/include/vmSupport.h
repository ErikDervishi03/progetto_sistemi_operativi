#ifndef VMSUPPORT_H
#define VMSUPPORT_H

#include "../../phase2/include/initial.h"
#include "./initProc.h"

extern pcb_t* current_process;
extern pcb_t* ssi_pcb;

void uTLB_RefillHandler();
void pager();

#endif