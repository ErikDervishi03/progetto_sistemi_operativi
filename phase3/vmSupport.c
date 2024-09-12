#include "include/vmSupport.h"

#define GET_VPN(x) MIN(ENTRYHI_GET_VPN(x), 31)

static int getPage() {

  for (int i = 0; i < POOLSIZE; i++)
    if (swap_pool_table[i].sw_asid ==
        NOPROC) // -1 significa che la pagina e' libera
      return i;

  // se non c'e' una pagina libera, use un algoritmo FIFO per trovare una pagina
  // da rimpiazzare
  static int i = -1;
  return i = (i + 1) % POOLSIZE;
}

static void updateTLB(pteEntry_t p) {

  // Imposta il registro ENTRYHI con il valore entryHI dalla swap_pool_table
  // all'indice dato
  setENTRYHI(p.pte_entryHI);

  // Prova il TLB per trovare una voce corrispondente
  TLBP();

  // Se la voce non e' presente nel TLB, aggiorna il registro ENTRYLO e scrivilo
  // nel TLB
  if ((getINDEX() & PRESENTFLAG) == 0) {
    // setENTRYHI(p.pte_entryHI);
    setENTRYLO(p.pte_entryLO);

    /**
     * Esegue l'operazione TLBWI (Translation Lookaside Buffer Write Index).
     * Questa operazione scrive l'entry corrente della TLB (Translation
     * Lookaside Buffer) nell'indice specificato dal registro EntryHi.
     */
    TLBWI();
  }
}

/**
 * @brief Funzione ausiliaria usata per invalidare una pagina e aggiornare
 * il TLB (in modo atomico disabilitando gli interrupt)
 *
 * @param sp_index indizio della pagina da invalidare
 */
static void cleanDirtyPage(int sp_index) {
  setSTATUS(getSTATUS() & (~IECON)); // disabilito interrupt per avere atomicità

  swap_pool_table[sp_index].sw_pte->pte_entryLO &=
      (~VALIDON); // invalido la pagina
  updateTLB(*(swap_pool_table[sp_index].sw_pte));

  setSTATUS(getSTATUS() |
            IECON); // riabilito interrupt per rilasciare l'atomicita'
}

/**
 * Funzione per effettuare DoIO sui vari flash device tramite SSI
 */
static int RWBackingStore(int page_no, int asid, memaddr addr, int w) {
  dtpreg_t *device_register = (dtpreg_t *)DEV_REG_ADDR(IL_FLASH, asid - 1);
  device_register->data0 = addr;

  unsigned int value =
      w ? FLASHWRITE | (page_no << 8) : FLASHREAD | (page_no << 8);
  unsigned int status;

  ssi_do_io_t do_io = {
      .commandAddr = (unsigned int *)&(device_register->command),
      .commandValue = value,
  };
  ssi_payload_t payload = {
      .service_code = DOIO,
      .arg = &do_io,
  };

  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&payload), 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&status), 0);

  return status;
}

static void kill_proc() {
  for (int i = 0; i < POOLSIZE; i++) {
    if (swap_pool_table[i].sw_asid ==
        current_process->p_supportStruct->sup_asid)
      swap_pool_table[i].sw_asid = NOPROC;
  }

  ssi_payload_t term_process_payload = {
      .service_code = TERMPROCESS,
      .arg = NULL,
  };

  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb,
          (unsigned int)(&term_process_payload), 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, 0, 0);
}

void pager() {
  // prendo la support struct
  support_t *sup_st;
  ssi_payload_t getsup_payload = {
      .service_code = GETSUPPORTPTR,
      .arg = NULL,
  };
  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&getsup_payload),
          0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&sup_st), 0);

  // Se la cuasa è una TLB-Modification exception, la considero come
  // una program trap
  int cause = sup_st->sup_exceptState[PGFAULTEXCEPT].cause;
  if ((cause & GETEXECCODE) >> CAUSESHIFT == 1) {
    kill_proc();
  } else {
    // Vedo se posso PRENDERE la MUTUA ESCLUSIONE mandando allo
    // swap_mutex_process e attendo un riscontro.
    SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex_process, 0, 0);

    // Attendo la risposta dallo swap_mutex_process per ottenere la mutua
    // esclusione
    SYSCALL(RECEIVEMESSAGE, (unsigned int)swap_mutex_process, 0, 0);

    // Prendo la pagina virtuale dalla entry_hi
    // supp_p->sup_exceptState[PGFAULTEXCEPT].entry_hi
    int p = GET_VPN(sup_st->sup_exceptState[PGFAULTEXCEPT].entry_hi);

    // Uso il mio algoritmo di rimpiazzamento per trovare la pagina da
    // sostituire
    int i = getPage(); // pagina vittima

    // calcolo l'indirizzo della pagina tenendo conto dell'offset SWAP_POOL_AREA
    // e ogni singola pagina fino alla i-esima
    memaddr victim_addr = (memaddr)SWAP_POOL_AREA + (i * PAGESIZE);

    // è necessario aggiornare la pagina se questa era occupata da un altro
    // frame appartenente ad un altro processo e in caso serve "pulirna" poichè
    // ormai obsoleta (ovviamente tutto ciò in modo atomico per evitare
    // inconsistenza)
    int status;

    swap_t *swap_pool_entry = &swap_pool_table[i];

    if (swap_pool_entry->sw_asid != NOPROC) {
      cleanDirtyPage(i);

      // write backing store/flash
      status = RWBackingStore(swap_pool_entry->sw_pageNo,
                              swap_pool_entry->sw_asid, victim_addr, WRITE);

      if (status != 1) {
        SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex_process, 0, 0);
        kill_proc(); // tratto gli errori come se fossere program trap
      }
    }

    // read backing store/flash
    status = RWBackingStore(p, sup_st->sup_asid, victim_addr, READ);
    if (status != 1) {
      SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex_process, 0, 0);
      kill_proc(); // tratto gli errori come se fossere program trap
    }

    /*
      Aggiorna l'entry i nella Swap Pool table per riflettere i nuovi
       contenuti del frame i: la pagina p appartenente all'ASID del processo
       corrente e un puntatore all'entry della tabella delle pagine del processo
       corrente per la pagina p.
    */
    swap_pool_entry->sw_asid = sup_st->sup_asid;
    swap_pool_entry->sw_pageNo = p;
    swap_pool_entry->sw_pte = &(sup_st->sup_privatePgTbl[p]);

    setSTATUS(getSTATUS() &
              (~IECON)); // disabilito interrupt per avere atomicita'

    // 11 Aggiorno la tabella delle pagine del processo corrente per la pagina
    // p, indicando che la pagina e' presente (V bit) e occupa il frame i

    sup_st->sup_privatePgTbl[p].pte_entryLO |= VALIDON;
    sup_st->sup_privatePgTbl[p].pte_entryLO |= DIRTYON;
    sup_st->sup_privatePgTbl[p].pte_entryLO &= 0xFFF;
    sup_st->sup_privatePgTbl[p].pte_entryLO |= (victim_addr);
    updateTLB(sup_st->sup_privatePgTbl[p]);

    setSTATUS(getSTATUS() |
              IECON); // riabilito interrupt per rilasciare l'atomicita'

    // 13 RILASCIARE MUTUA ESCLUSIONE
    SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex_process, 0, 0);

    // FINE MUTUA ESCLUSIONE
    LDST(&(sup_st->sup_exceptState[PGFAULTEXCEPT]));
  }
}

void uTLB_RefillHandler() {
  // prendo l'exception_state dalla BIOSDATAPAGE al fine di trovare
  // il numero della pagina dal registro entry_hi
  state_t *exception_state = (state_t *)BIOSDATAPAGE;
  int p = GET_VPN(exception_state->entry_hi);

  setENTRYHI(current_process->p_supportStruct->sup_privatePgTbl[p].pte_entryHI);
  setENTRYLO(current_process->p_supportStruct->sup_privatePgTbl[p].pte_entryLO);
  TLBWR();

  // Restituisco il controllo al processo corrente per riprovare l'istruzione
  // che aveva causato l'evento di TLB-Refill
  LDST(exception_state);
}