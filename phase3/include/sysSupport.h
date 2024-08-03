#ifndef SYSSUPPORT

#include "../../headers/const.h"
#include "../../headers/types.h"
#include "../../headers/listx.h"
#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/msg.h"
#include "../../phase2/include/timers.h"
#include "../../phase2/include/exceptions.h"
#include <umps/const.h>
#include <umps/libumps.h>
#include <umps/arch.h>
#include <umps/cp0.h>

void programTrapExceptionHandler(state_t *exception_state);
void supSyscallExceptionHandler(state_t *exception_state);
void generalExceptionHandler();

extern pcb_t* swap_mutex_process;

#endif 