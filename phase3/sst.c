/**
 * @file sst.c
 * @brief Implementazione delle funzioni per la gestione dei servizi SST (System Support Task).
 */

#include "include/sst.h"
#include "testers/h/tconst.h"

/**
 * @brief Richiede il puntatore al supporto del processo SST dalla SSI (System Support Interface).
 * @return Il puntatore al supporto del processo SST.
 */
support_t *request_support() {
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
 * @brief Termina il processo SST specificato.
 * @param asid L'ASID (Address Space Identifier) del processo SST.
 * @return Il valore di ritorno della terminazione del processo SST.
 */
unsigned int terminate_sst_process(int asid) {
  // Notifica il test della terminazione del processo SST
  for (int i = 0; i < POOLSIZE; ++i)
    if (swap_pool_table[i].sw_asid == asid)
      swap_pool_table[i].sw_asid = -1; // Libera il frame

  SYSCALL(SENDMESSAGE, (unsigned int)test_pcb, 0, 0);

  // Richiede la terminazione del processo alla SSI
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
 * @param device_type Il tipo di dispositivo su cui scrivere (6 per la stampante, 7 per il terminale).
 * @param payload Il payload contenente la stringa da scrivere.
 * @return 1.
 */
unsigned int write_to_device(support_t *sup, unsigned int device_type,
                             sst_print_t *payload) {
  if (payload->string[payload->length] != '\0')
    payload->string[payload->length] = '\0';

  pcb_t *destination = NULL;
  switch (device_type) {
  case 6:
    destination = printer_pcbs[sup->sup_asid - 1];
    break;
  case 7:
    destination = terminal_pcbs[sup->sup_asid - 1];
    break;
  default:
    break;
  }

  SYSCALL(SENDMESSAGE, (unsigned int)destination, (unsigned int)payload->string, 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)destination, 0, 0);

  return 1;
}

/**
 * @brief Loop principale del processo SST, sempre in attesa di nuove richieste.
 */
void SST_loop() {
  support_t *sup = request_support();
  state_t *state = &UProc_state[sup->sup_asid - 1];

  create_process(state, sup); // Causa una trap e il processo viene terminato

  while (TRUE) {
    ssi_payload_t *payload;
    unsigned int sender;

    sender = SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (unsigned int)(&payload), 0); // Attende il messaggio

    // Gestisce la richiesta ricevuta
    unsigned int result = SSTRequest(sup, payload);

    // Restituisce il risultato della richiesta al mittente
    SYSCALL(SENDMESSAGE, (unsigned int)sender, result, 0);
  }
}

/**
 * @brief Gestisce le richieste del processo SST.
 * @param sup Il puntatore al supporto del processo SST.
 * @param payload Il payload contenente la richiesta di servizio.
 * @return Il valore di ritorno della richiesta di servizio.
 */
unsigned int SSTRequest(support_t *sup, ssi_payload_t *payload) {
  unsigned int result = 0;

  switch (payload->service_code) {
  case GET_TOD:
    unsigned int timestamp;
    STCK(timestamp);
    result = timestamp;
    break;

  case TERMINATE:
    result = terminate_sst_process(sup->sup_asid);
    break;

  case WRITEPRINTER:
    result = write_to_device(sup, 6, (sst_print_t *)payload->arg);
    break;

  case WRITETERMINAL:
    result = write_to_device(sup, 7, (sst_print_t *)payload->arg);
    break;

  default: // Gestisce i casi sconosciuti e TERMINATE
    result = terminate_sst_process(sup->sup_asid);
    break;
  }

  return result;
}
