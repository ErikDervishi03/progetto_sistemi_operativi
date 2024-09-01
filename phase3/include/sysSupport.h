#ifndef SYSSUPPORT
#define SYSSUPPORT

#include "../../phase2/starterKit/src/dep.h"

void programTrapExceptionHandler(state_t *exception_state);
void supSyscallExceptionHandler(state_t *exception_state);
void generalExceptionHandler();

extern pcb_t* swap_mutex_process;

#endif 