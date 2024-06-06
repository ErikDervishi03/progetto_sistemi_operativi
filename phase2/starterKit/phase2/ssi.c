#include "dep.h"

void termProcess(pcb_t *sender) {
  if (sender == NULL) {
    return;
  }
  outChild(sender);

  pcb_PTR child = NULL;
  while ((child = removeChild(sender)) != NULL) {
    termProcess(child);
  }

  pcb_PTR removedProc = outProcQ(&ready_queue, sender);

  if (outProcQ(&PseudoClockWP, sender) != NULL) {
    softBlockCount--;
  } else if (removedProc == NULL) {
    for (int i = 0; i < SEMDEVLEN - 1; i++) {
      if (outProcQ(&blockedPCBs[i], sender) != NULL) {
        softBlockCount--;
        break;
      }
    }
  }
}

void SSIRequest(pcb_t* sender, const int service, void* arg){
    
    switch (service)
    {
    /*CreateProcess*/
    case 1:
        {
        term_puts("entro in  create process\n");
        /*If the new process cannot be created due to lack of resources (e.g.
        no more free PBCs), an error code of -1 (constant NOPROC) will be returned*/
        if(emptyProcQ(&pcbFree_h)){
            // strano, arg2 e' un unsigned int e noi ritorniamo NOPROC che e' -1
            arg = (void *)NOPROC;
            break;
        }
        /*allocate a new PCB*/
        pcb_t *child = allocPcb();
        /*arg should contain a pointer to a 
        struct ssi_create_process_t (ssi_create_process_t *).*/
        ssi_create_process_PTR ssi_create_process = (ssi_create_process_PTR)arg;
        
        /* initializes its fields*/

        /*p_s from arg->state*/
        child->p_s = *ssi_create_process->state;
        /*p_supportStruct from arg->support. If no parameter is provided, 
        this field is set to NULL*/
        child->p_supportStruct = ssi_create_process->support;

        /*p_time is set to zero; the new process has yet to accumulate any CPU time*/
        child->p_time = 0;

        /*The process queue field (e.g. p_list) by the call to insertProcQ*/
        insertProcQ(&sender->p_list, child);

        /*The process tree field (e.g. p_sib) by the call to insertChild.*/
        insertChild(sender,child);

        /*The newly populated PCB is placed on the Ready Queue*/
        insertProcQ(&ready_queue, child);

        /* Process Count is incremented by one*/
        processCount++;

        arg = &child; // possibile che questo sia sbagliato
                             // l'idea era che una volta tornati nel 
                             // SSI_function_entry_point la send mandasse l'arg giusto
        
        /* control is returned to the Current Process*/
        //LDST(&current_process->p_s);    
        }
        break;
    /*DoIO*/
    case 3: // non sicuro
    {
        ssi_do_io_t *do_io = (ssi_do_io_t *)arg;
        unsigned int *commandAddr = do_io->commandAddr;
        unsigned int commandValue = do_io->commandValue;

        *commandAddr = commandValue;

        int term0dev = EXT_IL_INDEX(IL_TERMINAL) * N_DEV_PER_IL + 0;

        //blockedPCBs[term0dev] = sender;

        softBlockCount++;

        // setSTATUS(getSTATUS() | ~getSTATUS()); mette tutti i bit a 1
    }
        break;
    /*GetCPUTime*/
    case 4:
        arg = &(sender->p_time); 
        break;
    /*WaitForClock*/
    case 5:
        softBlockCount++;
        insertProcQ(&PseudoClockWP, sender);
        arg = NULL;
        break;
    /*GetSupportData*/
    case 6:
        arg = &(sender->p_supportStruct);
        break;
    /*GetProcessID*/
    case 7:
        if((*(int *)arg) == 0){
            arg = &(sender->p_pid);
        }
        else if(sender->p_parent != NULL){
            arg = &(sender->p_parent->p_pid);
        }
        else{
            arg = 0;
        }
        break;   
    default:
        /*Terminate Process*/
        /* comprehends case 2
         If service does not match any of those provided by the SSI,
         the SSI should terminate the process and its progeny TO DO*/
        {
            /*This service terminates the sender process if arg is NULL. Otherwise,
             arg should be a pcb_t pointer*/
            if(arg == NULL){
                termProcess(sender);
            }else{
                termProcess((pcb_t*)arg);
            }
        }
        break;
    }
}

void SSI_function_entry_point(){
    while (1)
    {
        ssi_payload_t payload;
        unsigned int senderAdd = SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (unsigned int)(&payload), 0);
        const int service = payload.service_code;
        void *arg = payload.arg;
        pcb_t *senderPCB = (pcb_t *)senderAdd;
        SSIRequest(senderPCB, service, arg);

        // dubbio : ma ha senso mandare la risposta qua?
        // non dovrebbe essere meglio mandarla in SSIRequest direttamente?
        // magari in SSIRequest modifichiamo solo arg (il quale sara' la risposta)
        
        SYSCALL(SENDMESSAGE, senderAdd, (unsigned int)arg, 0);
    }    
  
}



