#include "include/vmSupport.h"

// GET_VPN(x) estrae il numero della pagina virtuale da x e lo limita a un massimo di 31
#define GET_VPN(x) MIN(ENTRYHI_GET_VPN(x), 31)

// Gestore della ricarica della TLB (Translation Lookaside Buffer)
void uTLB_RefillHandler() {
  // Ottengo lo stato dell'eccezione dalla BIOSDATAPAGE per recuperare il numero della pagina dal registro entry_hi
  state_t *exception_state = (state_t *)BIOSDATAPAGE;
  int page_number = GET_VPN(exception_state->entry_hi);

  // Imposto i registri ENTRYHI e ENTRYLO della CPU con i valori della pagina page_number
  setENTRYHI(current_process->p_supportStruct->sup_privatePgTbl[page_number].pte_entryHI);
  setENTRYLO(current_process->p_supportStruct->sup_privatePgTbl[page_number].pte_entryLO);

  // Scrivo nella TLB i valori appena impostati
  TLBWR();

  // Ripristino il controllo al processo corrente per riprovare l'istruzione che ha causato l'eccezione di TLB-Refill
  LDST(exception_state);
}

// Funzione per ottenere l'indice della pagina da rimpiazzare usando una politica FIFO
static int getVictimPageIndex() {
  for (int i = 0; i < POOLSIZE; i++) {
    if (swap_pool_table[i].sw_asid == NOPROC) // -1 indica che la pagina è libera
      return i;
  }

  // Se non ci sono pagine libere, utilizza una politica FIFO per scegliere una pagina da rimpiazzare
  static int index = -1;
  return index = (index + 1) % POOLSIZE;
}

// Funzione per aggiornare la TLB dopo che è stata modificata la Page Table (per garantire consistenza)
static void updateTLBEntry(pteEntry_t page_entry) {
  // Imposto il registro ENTRYHI con il valore entryHI dalla voce della swap pool
  setENTRYHI(page_entry.pte_entryHI);

  // Verifico se la voce è presente nella TLB
  TLBP();

  // Se la voce non è presente nel TLB, aggiorno il registro ENTRYLO e lo scrivo nella TLB
  if ((getINDEX() & PRESENTFLAG) == 0) {
    setENTRYLO(page_entry.pte_entryLO);

    // Scrivo la voce corrente nella TLB all'indice specificato dal registro ENTRYHI
    TLBWI();

  }
}

// Funzione per invalidare una pagina che è stata modificata
static void invalidateDirtyPage(int swap_index) {
  setSTATUS(getSTATUS() & (~IECON)); // Disabilito gli interrupt per garantire l'atomicità

  swap_pool_table[swap_index].sw_pte->pte_entryLO &= (~VALIDON); // Invalido la pagina
  updateTLBEntry(*(swap_pool_table[swap_index].sw_pte));

  setSTATUS(getSTATUS() | IECON); // Riabilito gli interrupt
}

// Funzione per effettuare operazioni di I/O sui dispositivi flash tramite SSI
static int performIOOnBackingStore(int page_number, int asid, memaddr address, int write) {
  dtpreg_t *device_register = (dtpreg_t *)DEV_REG_ADDR(IL_FLASH, asid - 1);
  device_register->data0 = address;


  // dalle spec: COMMAND field with the device block number (high order three bytes) and the command to read (or write) in the lower order byte
  unsigned int command_value = write ? FLASHWRITE | (page_number << 8) : FLASHREAD | (page_number << 8);
  unsigned int status;

  ssi_do_io_t io_request = {
      .commandAddr = (unsigned int *)&(device_register->command),
      .commandValue = command_value,
  };
  ssi_payload_t io_payload = {
      .service_code = DOIO,
      .arg = &io_request,
  };

  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&io_payload), 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&status), 0);

  return status;
}

// Funzione per terminare un processo e liberare le sue risorse
static void terminateProcess() {
  for (int i = 0; i < POOLSIZE; i++) {
    if (swap_pool_table[i].sw_asid == current_process->p_supportStruct->sup_asid) {
      swap_pool_table[i].sw_asid = NOPROC;
    }
  }

  ssi_payload_t terminate_payload = {
      .service_code = TERMPROCESS,
      .arg = NULL,
  };

  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&terminate_payload), 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, 0, 0);
}

// Funzione principale del pager per gestire i page fault
void pager() {
  // Ottengo la struttura di supporto del processo
  support_t *support_struct;
  ssi_payload_t get_support_payload = {
      .service_code = GETSUPPORTPTR,
      .arg = NULL,
  };

  // Richiedo il puntatore alla struttura di supporto al SSI e aspetto la risposta
  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&get_support_payload), 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&support_struct), 0);

  // Verifico se l'eccezione è causata da una modifica della TLB, in tal caso la tratto come un errore del programma
  int cause = support_struct->sup_exceptState[PGFAULTEXCEPT].cause;
  if ((cause & GETEXECCODE) >> CAUSESHIFT == 1) {
    terminateProcess();
  } else {
    // Acquisisco la mutua esclusione per accedere alla swap pool
    SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex_process, 0, 0);
    SYSCALL(RECEIVEMESSAGE, (unsigned int)swap_mutex_process, 0, 0);

    // Recupero il numero della pagina dalla entry_hi
    int page_number = GET_VPN(support_struct->sup_exceptState[PGFAULTEXCEPT].entry_hi);

    // Politica FIFO per selezionare una pagina da rimpiazzare
    int victim_index = getVictimPageIndex(); // Indice della pagina vittima

    // Calcolo l'indirizzo della pagina da rimpiazzare
    memaddr victim_address = (memaddr)SWAP_POOL_AREA + (victim_index * PAGESIZE);

    // Verifico se la pagina vittima è già occupata da un altro processo e, in tal caso, la pulisco
    int status;
    swap_t *victim_entry = &swap_pool_table[victim_index];

    if (victim_entry->sw_asid != NOPROC) {
      invalidateDirtyPage(victim_index);

      // Scrivo la pagina vittima nel backing store
      status = performIOOnBackingStore(victim_entry->sw_pageNo, victim_entry->sw_asid, victim_address, WRITE);

      if (status != 1) {
        SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex_process, 0, 0);
        terminateProcess(); // Tratto gli errori come errori del programma
      }
    }

    // Leggo la nuova pagina dal backing store
    status = performIOOnBackingStore(page_number, support_struct->sup_asid, victim_address, READ);
    if (status != 1) {
      SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex_process, 0, 0);
      terminateProcess(); // Tratto gli errori come errori del programma
    }

    // Aggiorno la tabella della swap pool con i nuovi dati della pagina
    victim_entry->sw_asid = support_struct->sup_asid;
    victim_entry->sw_pageNo = page_number;
    victim_entry->sw_pte = &(support_struct->sup_privatePgTbl[page_number]);

    setSTATUS(getSTATUS() & (~IECON)); // Disabilito gli interrupt per garantire l'atomicità

    // Aggiorno la tabella delle pagine del processo corrente con la nuova pagina
    support_struct->sup_privatePgTbl[page_number].pte_entryLO |= VALIDON;
    support_struct->sup_privatePgTbl[page_number].pte_entryLO |= DIRTYON;
    support_struct->sup_privatePgTbl[page_number].pte_entryLO &= 0xFFF;
    support_struct->sup_privatePgTbl[page_number].pte_entryLO |= victim_address;
    updateTLBEntry(support_struct->sup_privatePgTbl[page_number]);

    setSTATUS(getSTATUS() | IECON); // Riabilito gli interrupt

    // Rilascio la mutua esclusione
    SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex_process, 0, 0);

    // Ripristino il controllo al processo corrente per far riprovare l'istruzione che ha causato l'eccezione di Page Fault
    LDST(&(support_struct->sup_exceptState[PGFAULTEXCEPT]));
  }
}