#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

void
usage(void) {
	fputs("nebula2 needs the name of a config file as 1st argument.\n", stderr);
}

int
main(int argc, char** argv) {
	int       rv   = 1;
	config_t* conf = NULL;

	if(argc < 2) {
		usage();
		goto tidyup;
	}

	if(!conf_load(argv[1], &conf)) {
		goto tidyup;
	}

	conf_print(conf);

	/*nebula2(conf);*/

	rv = 0; /* All OK, we return with no error. */
tidyup:
	if(conf) {
		conf_destroy(conf);
	}

	return rv;
}
