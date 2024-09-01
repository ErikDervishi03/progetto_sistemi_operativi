#ifndef SST
#define SST

#include "../../phase2/starterKit/src/dep.h"
#include <stdlib.h>

void SST_loop();
support_t *support_request();
unsigned int sst_terminate(int asid);
unsigned int sst_write(support_t *sup, unsigned int device_type, sst_print_t *payload);
unsigned int SSTRequest(support_t *sup, ssi_payload_t *payload);

#endif