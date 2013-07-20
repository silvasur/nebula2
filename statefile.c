#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "config.h"

int
state_load(config_t* conf, uint32_t* map, uint32_t* jobs_done) {
	FILE*  fh = NULL;
	size_t mapsize;
	int    errsv;

	mapsize = conf->width * conf->height * conf->iters_n;

	if(!(fh = fopen(conf->statefile, "rb"))) {
		if(errno == ENOENT) {
			memset(map, 0, mapsize);
			*jobs_done = 0;
			return 1;
		}

		return 0;
	}

	if(fread(jobs_done, sizeof(uint32_t), 1, fh) != 1) {
		errsv = errno;
		fclose(fh);
		errno = errsv;
		return 0;
	}

	if(fread(map, sizeof(uint32_t), mapsize, fh) != mapsize) {
		errsv = errno;
		fclose(fh);
		errno = errsv;
		return 0;
	}

	fclose(fh);
	return 1;
}

int
state_save(config_t* conf, uint32_t* map, uint32_t jobs_done) {
	FILE*  fh = NULL;
	size_t mapsize;
	int    errsv;

	mapsize = conf->width * conf->height * conf->iters_n;

	if(!(fh = fopen(conf->statefile, "wb"))) {
		return 0;
	}

	if(fwrite(&jobs_done, sizeof(uint32_t), 1, fh) != 1) {
		errsv = errno;
		fclose(fh);
		errno = errsv;
		return 0;
	}

	if(fwrite(map, sizeof(uint32_t), mapsize, fh) != mapsize) {
		errsv = errno;
		fclose(fh);
		errno = errsv;
		return 0;
	}

	fclose(fh);
	return 1;
}
