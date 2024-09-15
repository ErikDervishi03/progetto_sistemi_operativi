/*
 * Questo file contiene l'implementazione delle funzioni utilizzate per
 * l'inizializzazione dei processi nel sistema operativo PandOS. Le funzioni
 * inizializzano le strutture dati necessarie per la gestione dei processi,
 * creano i processi semaforo per i dispositivi e inizializzano le tabelle di
 * pagine.
 */

#include "./include/initProc.h"

extern pcb_t *ssi_pcb;            // puntatore al processo SSI
extern pcb_t *current_process;    // puntatore al processo corrente

support_t structures[UPROCMAX];   // array di strutture di supporto
state_t UProc_state[UPROCMAX];    // array di stati dei processi utente
swap_t swap_pool_table[POOLSIZE]; // tabella di swap
pcb_t *sst_pcbs[UPROCMAX];        // array di puntatori ai processi SST
pcb_t *terminal_pcbs[UPROCMAX];   // array di puntatori ai processi terminali
pcb_t *printer_pcbs[UPROCMAX];    // array di puntatori ai processi stampanti

pcb_t *swap_mutex_pcb;            // puntatore al processo mutex per lo swap
pcb_t *swap_mutex_process;        // processo per garantire mutua esclusione per lo swap
pcb_t *test_pcb;                  // puntatore al processo di test
pcb_t *mutex_holder;              // puntatore al processo che detiene la mutua esclusione
state_t swap_mutex_state;         // stato del processo mutex 

memaddr curr;                     // indirizzo corrente


// funzione per la creazione di un processo, tramite stato e struttura di supporto
// richiede la creazione al SSI e riceve il puntatore al processo creato
pcb_t *create_process(state_t *s, support_t *sup) {
  pcb_t *p;
  ssi_create_process_t ssi_create_process = {
      .state = s,
      .support = sup,
  };
  ssi_payload_t payload = {
      .service_code = CREATEPROCESS,
      .arg = &ssi_create_process,
  };

  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&payload, 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&p), 0);

  return p;
}


// funzione per la gestione della mutua esclusione
void swapMutex() {
  for (;;) {
    // ricezione richieste di mutua esclusione
    unsigned int sender = SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, 0, 0);
    // concessione mutua esclusione
    mutex_holder = (pcb_t *)sender;
    SYSCALL(SENDMESSAGE, (unsigned int)sender, 0, 0);
    // attesa di rilascio della mutua esclusione
    SYSCALL(RECEIVEMESSAGE, sender, 0, 0);
    mutex_holder = NULL;
  }
}


// funzioni che printano sui terminal
void print_terminal0(void) { print_to(0, (unsigned int *)TERM0ADDR); }
void print_terminal1(void) { print_to(1, (unsigned int *)TERM0ADDR); }
void print_terminal2(void) { print_to(2, (unsigned int *)TERM0ADDR); }
void print_terminal3(void) { print_to(3, (unsigned int *)TERM0ADDR); }
void print_terminal4(void) { print_to(4, (unsigned int *)TERM0ADDR); }
void print_terminal5(void) { print_to(5, (unsigned int *)TERM0ADDR); }
void print_terminal6(void) { print_to(6, (unsigned int *)TERM0ADDR); }
void print_terminal7(void) { print_to(7, (unsigned int *)TERM0ADDR); }

// array di print per i terminali
void (*print_terminals[8])(void) = {
    print_terminal0,
    print_terminal1,
    print_terminal2,
    print_terminal3,
    print_terminal4,
    print_terminal5,
    print_terminal6,
    print_terminal7
};


// funzioni che printano sui printers
void print_printer0(void) { print_to(0, (unsigned int *)PRINTER0ADDR); }
void print_printer1(void) { print_to(1, (unsigned int *)PRINTER0ADDR); }
void print_printer2(void) { print_to(2, (unsigned int *)PRINTER0ADDR); }
void print_printer3(void) { print_to(3, (unsigned int *)PRINTER0ADDR); }
void print_printer4(void) { print_to(4, (unsigned int *)PRINTER0ADDR); }
void print_printer5(void) { print_to(5, (unsigned int *)PRINTER0ADDR); }
void print_printer6(void) { print_to(6, (unsigned int *)PRINTER0ADDR); }
void print_printer7(void) { print_to(7, (unsigned int *)PRINTER0ADDR); }

// array di print per i printers
void (*print_printers[8])(void) = {
    print_printer0,
    print_printer1,
    print_printer2,
    print_printer3,
    print_printer4,
    print_printer5,
    print_printer6,
    print_printer7
};



// funzione di stampa su un dispositivo passato per argomento
void print_to(int device_number, unsigned int *base_address) {
  while (1) {
    char *string;
    unsigned int sender;
    // ricezione richiesta di stampa
    sender = (unsigned int)SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (unsigned int)(&string), 0);

    unsigned int *base = base_address + 4 * device_number; // indirizzo base del dispositivo
    unsigned int *command;

    // per terminali
    if (base_address == (unsigned int *)TERM0ADDR)
      command = base + 3;
    else
      command = base + 1;
    
    // per stampanti
    unsigned int *data0 = base + 2; 
    unsigned int status;



    // ciclo fin quando non si raggiunge la fine della stringa
    while (*string != EOS) 
    {
      unsigned int value;
      if (base_address == (unsigned int *)TERM0ADDR)
        value = PRINTCHR | (((unsigned int)*string) << 8);
      else {
        // caricamento registro data0 per le stampanti
        value = PRINTCHR;
        *data0 = (unsigned int)*string;
      }

      // mando la DOIO a SSI per fare la print di un singolo char
      ssi_do_io_t do_io = {
          .commandAddr = command,
          .commandValue = value,
      };
      ssi_payload_t payload = {
          .service_code = DOIO,
          .arg = &do_io,
      };

      SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&payload), 0);
      SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&status),
              0);

      if (base_address == (unsigned int *)TERM0ADDR &&
          (status & TERMSTATMASK) != RECVD)
        PANIC();
      if (base_address == (unsigned int *)PRINTER0ADDR && status != READY)
        PANIC();

      // next char
      string++;
    }

    // comunico al sst la fine dell'operazione
    SYSCALL(SENDMESSAGE, (unsigned int)sender, 0, 0);
  }
}

 // funzione per inizializzare una entry nella tabella delle pagine
static void initPageTableEntry(pteEntry_t *entry, int asid, int i) {
  if (i < 31)
    entry->pte_entryHI = KUSEG + (i << VPNSHIFT) + (asid << ASIDSHIFT);
  else
    entry->pte_entryHI = 0xBFFFF000 + (asid << ASIDSHIFT); // stack page
  entry->pte_entryLO = DIRTYON;
}


  // Funzione per l'inizializzazione dei processi utente.
  // Inizializza gli stati dei processi, le strutture di supporto e le tabelle delle pagine.
 
static void initUProc() {
  for (int asid = 1; asid <= UPROCMAX; asid++) {
    // inizializzazione stato
    UProc_state[asid - 1].reg_sp = (memaddr)USERSTACKTOP;
    UProc_state[asid - 1].pc_epc = (memaddr)UPROCprevTodADDR;
    UProc_state[asid - 1].status = ALLOFF | IEPON | IMON | USERPON | TEBITON;
    UProc_state[asid - 1].reg_t9 = (memaddr)UPROCprevTodADDR;
    UProc_state[asid - 1].entry_hi = asid << ASIDSHIFT;

    // inizializzazione strutture di supporto SST
    structures[asid - 1].sup_asid = asid;
    structures[asid - 1].sup_exceptContext[PGFAULTEXCEPT].stackPtr = (memaddr)curr;
    structures[asid - 1].sup_exceptContext[PGFAULTEXCEPT].status = ALLOFF | IEPON | IMON | TEBITON;
    structures[asid - 1].sup_exceptContext[PGFAULTEXCEPT].pc = (memaddr)pager;

    curr -= PAGESIZE;

    structures[asid - 1].sup_exceptContext[GENERALEXCEPT].stackPtr = (memaddr)curr;
    structures[asid - 1].sup_exceptContext[GENERALEXCEPT].status = ALLOFF | IEPON | IMON | TEBITON;
    structures[asid - 1].sup_exceptContext[GENERALEXCEPT].pc = (memaddr)generalExceptionHandler;

    curr -= PAGESIZE;

    // inizializzazione tabella delle pagine del processo
    for (int i = 0; i < USERPGTBLSIZE; i++)
      initPageTableEntry(&(structures[asid - 1].sup_privatePgTbl[i]), asid, i);
  }
}


// funzione per inizializzare i devproc, eseguono le richieste I/O degli SST sui devices 
static void initDevProc() {
  // stati degli UPROCMAX * 2 processi device
  state_t dev_state[UPROCMAX * 2];

  for (int i = 0; i < UPROCMAX * 2; i++) {
    dev_state[i].reg_sp = (memaddr)curr;

    if (i < UPROCMAX) {
      dev_state[i].pc_epc = print_terminals[i];
      dev_state[i].entry_hi = (i + 1) << ASIDSHIFT;
    } else {
      dev_state[i].pc_epc = print_printers[i - UPROCMAX];
      dev_state[i].entry_hi = (i - UPROCMAX + 1) << ASIDSHIFT;
    }
  
    dev_state[i].status = ALLOFF | TEBITON | CAUSEINTMASK | IEPBITON;

    if (i < UPROCMAX) {
      terminal_pcbs[i] = create_process(&dev_state[i], &structures[i]);
    } else {
      printer_pcbs[i - UPROCMAX] = create_process(&dev_state[i], &structures[i - UPROCMAX]);
    }

    curr -= PAGESIZE;
  }
}


// funzione di test, inizializza la processi utente, processi device, la tabella di swap, i mutex, i processi SST
void test() {
  test_pcb = current_process;

  RAMTOP(curr);
  curr -= (3 * PAGESIZE); // inizia dopo lo spazio per test e ssi

  // inizializzo i processi utente
  initUProc();

  // inizializzo i processi per i dispositivi
  initDevProc();

  // inizializzo la tabella di swap
   for (int i = 0; i < POOLSIZE; i++) {
    swap_pool_table[i].sw_asid = NOPROC;
  }

  // inizializzo il processo mutex per la mutua esclusione
  swap_mutex_state.reg_sp = (memaddr)curr;
  swap_mutex_state.pc_epc = (memaddr)swapMutex;
  swap_mutex_state.status = ALLOFF | IEPON | IMON | TEBITON;
  curr -= PAGESIZE;
  swap_mutex_process = create_process(&swap_mutex_state, NULL); // pid 3


  // inizializzo i processi SST
  state_t SST_state[UPROCMAX];
  for (int asid = 1; asid <= UPROCMAX; asid++) {
    SST_state[asid - 1].reg_sp = (memaddr)curr;
    SST_state[asid - 1].pc_epc = (memaddr)SST_loop;
    SST_state[asid - 1].status = ALLOFF | IEPON | IMON | TEBITON;
    SST_state[asid - 1].entry_hi = asid << ASIDSHIFT;
    sst_pcbs[asid - 1] = create_process(&SST_state[asid - 1], &structures[asid - 1]);
    curr -= PAGESIZE;
  }

  for (int i = 0; i < UPROCMAX; i++)
    SYSCALL(RECEIVEMESSAGE, (unsigned int)sst_pcbs[i], 0, 0);

  // chiedo al processo SSI la terminazione del processo di test
  ssi_payload_t payload = {
      .service_code = TERMPROCESS,
      .arg = NULL,
  };
  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&payload), 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, 0, 0);
}