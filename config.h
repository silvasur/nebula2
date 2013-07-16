#ifndef _nebula2_config_h_
#define _nebula2_config_h_

#include "color.h"

typedef struct {
	int width, height;
	int jobsize, jobs, procn;

	char* statefile;
	char* output;

	int      iters_n;
	int*     iters;
	color_t* colors;
} config_t;

extern void conf_destroy(config_t* conf);
extern int conf_load(char* path, config_t** conf);
extern void conf_print(config_t* conf);

#endif
