#ifndef SYSSUPPORT

#include "../../headers/const.h"
#include "../../headers/types.h"
#include "../../headers/listx.h"


void programTrapExceptionHandler(state_t *exception_state);
void supSyscallExceptionHandler(state_t *exception_state);
void generalExceptionHandler();

extern pcb_t* swap_mutex_process;

#endif 