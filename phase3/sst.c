/**
 * @file sst.c
 * @brief Implementazione delle funzioni per SST.
 */

#include "include/sst.h"
#include "testers/h/tconst.h"

/**
 * @brief Richiede il puntatore al supporto del processo SST alla SSI (System
 * Support Interface).
 * @return Il puntatore al supporto del processo SST.
 */
support_t *support_request() {
  support_t *sup;
  ssi_payload_t payload = {
      .service_code = GETSUPPORTPTR,
      .arg = NULL,
  };
  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&payload, 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&sup), 0);
  return sup;
}

/**
 * @brief Termina il processo SST.
 * @param asid L'ASID (Address Space Identifier) del processo SST.
 * @return Il valore di ritorno della terminazione del processo SST.
 */
unsigned int sst_terminate(int asid) {
  // Mandare messaggio al test per comunicare la terminazione del processo SST
  for (int i = 0; i < POOLSIZE; ++i)
    if (swap_pool_table[i].sw_asid == asid)
      swap_pool_table[i].sw_asid = -1; // Libero il frame
  SYSCALL(SENDMESSAGE, (unsigned int)test_pcb, 0, 0);

  // Richiesta di terminazione del processo alla SSI
  int ret;
  ssi_payload_t payload = {
      .service_code = TERMPROCESS,
      .arg = NULL,
  };
  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&payload, 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&ret), 0);

  return 1;
}

/**
 * @brief Scrive una stringa su un dispositivo specificato.
 * @param sup Il puntatore al supporto del processo SST.
 * @param device_type Il tipo di dispositivo su cui scrivere (6 per la
 * stampante, 7 per il terminale).
 * @param payload Il payload contenente la stringa da scrivere.
 * @return 1.
 */
unsigned int sst_write(support_t *sup, unsigned int device_type,
                       sst_print_t *payload) {
  if (payload->string[payload->length] != '\0')
    payload->string[payload->length] = '\0';

  pcb_t *dest = NULL;
  switch (device_type) {
  case 6:
    dest = printer_pcbs[sup->sup_asid - 1];
    break;
  case 7:
    dest = terminal_pcbs[sup->sup_asid - 1];
    break;
  default:
    break;
  }

  SYSCALL(SENDMESSAGE, (unsigned int)dest, (unsigned int)payload->string, 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)dest, 0, 0);

  return 1;
}

/**
 * @brief loop del SST, che è sempre in attesa di nuove richieste.
 */
void SST_loop() {
  support_t *sup = support_request();
  state_t *state = &UProc_state[sup->sup_asid - 1];

  create_process(state, sup); // causa la trap e viene killato

  while (TRUE) {
    ssi_payload_t *payload;
    unsigned int sender;

    sender = SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (unsigned int)(&payload),
                     0); // SI BLOCCA QUI IN ATTESA DELLA SECONDA RECEVE

    // Tentativo di soddisfare la richiesta
    unsigned int ret = SSTRequest(sup, payload);

    // Se necessario, restituisco ret (specialmente nel caso sia getTOD,
    // altrimenti basta un valore a caso solo per ACK)
    SYSCALL(SENDMESSAGE, (unsigned int)sender, ret, 0);
  }
}

/**
 * @brief Funzione di supporto per la gestione delle richieste del processo SST.
 * @param sup Il puntatore al supporto del processo SST.
 * @param payload Il payload contenente la richiesta di servizio.
 * @return Il valore di ritorno della richiesta di servizio.
 */
unsigned int SSTRequest(support_t *sup, ssi_payload_t *payload) {
  unsigned int ret = 0;

  switch (payload->service_code) {
  case GET_TOD:
    unsigned int tmp;
    STCK(tmp);
    ret=tmp; 
    break;

  case TERMINATE:
    ret = sst_terminate(sup->sup_asid);
    break;

  case WRITEPRINTER:
    ret = sst_write(sup, 6, (sst_print_t *)payload->arg);
    break;

  case WRITETERMINAL:
    ret = sst_write(sup, 7, (sst_print_t *)payload->arg);
    break;

  default: // per case TERMINATE e tutti gli altri
    ret = sst_terminate(sup->sup_asid);
    break;
  }

  return ret;
}