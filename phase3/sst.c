// implementazione del System Service  Thread
// è un thread di ogni processo che offre alcuni servizi
// SST condivide ASID e support sttruct con il processo utente figlio (un po' come l'SSI)
#include "include/sst.h"
#include "testers/h/tconst.h"


// killa processo
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


// scrive una stringa su un dispositivo
unsigned int write_to_device(support_t *sup, unsigned int device_type, sst_print_t *payload) {
  if (payload->string[payload->length] != '\0')
    payload->string[payload->length] = '\0';


  // terminal o printer
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


void SST_loop() {
  support_t *sup;
  // richiedo il puntatore alla support struct al SSI
   ssi_payload_t payload = {
      .service_code = GETSUPPORTPTR,
      .arg = NULL,
  };
  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&payload, 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&sup), 0);

  state_t *state = &UProc_state[sup->sup_asid - 1];
  create_process(state, sup); // Causa una trap e il processo viene terminato

  while (TRUE) {
    ssi_payload_t *payload;
    unsigned int sender;

    // attemde una richiesta 
    sender = SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (unsigned int)(&payload), 0); 

    // Gestisce la richiesta ricevuta
    unsigned int res = SSTRequest(sup, payload);

    // Restituisce il risultato della richiesta al sender
    SYSCALL(SENDMESSAGE, (unsigned int)sender, res, 0);
  }
}


// gestione richieste SST
unsigned int SSTRequest(support_t *sup, ssi_payload_t *payload) {
  unsigned int result = 0;

  switch (payload->service_code) {
    // il sender ottiene il numero di microsecondi trascorsi dall'ultimo reset
  case GET_TOD:
    unsigned int timestamp;
    STCK(timestamp);
    result = timestamp;
    break;

  // indovina un po', si ammazzano padre (SST) e figlio (U-proc) :(
  case TERMINATE:
    result = terminate_sst_process(sup->sup_asid);
    break;
  // stampa una stringa su un printer con lo stesso ASID del sender
  // il sender aspetta una risposta vuota dal SST per completare la scrittura
  case WRITEPRINTER:
    result = write_to_device(sup, 6, (sst_print_t *)payload->arg);
    break;

  // stampa una stringa su un terminal con lo stesso ASID del sender
  // il sender aspetta una risposta vuota dal SST per completare la scrittura
  case WRITETERMINAL:
    result = write_to_device(sup, 7, (sst_print_t *)payload->arg);
    break;

  default: // Gestisce i casi sconosciuti e TERMINATE
    result = terminate_sst_process(sup->sup_asid);
    break;
  }

  return result;
}