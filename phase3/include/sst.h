#ifndef SST
#define SST

#include "../../phase2/starterKit/src/dep.h"
#include <stdlib.h>

void SST_loop();
unsigned int terminate_sst_process(int asid);
unsigned int write_to_device(support_t *sup, unsigned int device_type, sst_print_t *payload);
unsigned int SSTRequest(support_t *sup, ssi_payload_t *payload);

#endif