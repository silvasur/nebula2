#ifndef _nebula2_statefile_h_
#define _nebula2_statefile_h_

#include <stdint.h>
#include "config.h"

extern int state_load(config_t* conf, uint32_t* map, uint32_t* jobs_done);
extern int state_save(config_t* conf, uint32_t* map, uint32_t jobs_done);

#endif
