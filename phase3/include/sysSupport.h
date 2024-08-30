#ifndef SYSSUPPORT

#include "../../headers/const.h"
#include "../../headers/types.h"
#include "../../headers/listx.h"
#include "../../phase2/starterKit/src/exceptions.c"


void programTrapExceptionHandler(state_t *exception_state);
void supSyscallExceptionHandler(state_t *exception_state);
void generalExceptionHandler();

extern pcb_t* swap_mutex_process;

#endif 