#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "color.h"

#include "iniparser/src/iniparser.h"

void
conf_destroy(config_t* conf) {
	if(conf->iters) {
		free(conf->iters);
	}
	if(conf->colors) {
		free(conf->colors);
	}
	free(conf);
}

static int
parse_color(dictionary* ini, char* key, color_t* col) {
	char  r[3];
	char  g[3];
	char  b[3];
	char* endptr;
	char* s = iniparser_getstring(ini, key, "");

	if(strlen(s) != 6) {
		fprintf(stderr, "Config value '%s' is not a color or is missing.\n", key);
		return 0;
	}

	memcpy(r, s,     2);
	memcpy(g, s + 2, 2);
	memcpy(b, s + 4, 2);
	r[2] = '\0';
	g[2] = '\0';
	b[2] = '\0';

	col->r = strtol(r, &endptr, 16);
	if(*endptr != '\0') {
		fprintf(stderr, "Config value '%s' is not a color.\n", key);
		return 0;
	}
	col->g = strtol(g, &endptr, 16);
	if(*endptr != '\0') {
		fprintf(stderr, "Config value '%s' is not a color.\n", key);
		return 0;
	}
	col->b = strtol(b, &endptr, 16);
	if(*endptr != '\0') {
		fprintf(stderr, "Config value '%s' is not a color.\n", key);
		return 0;
	}

	return 1;
}

#define NAMEBUF_SIZE 32

static int
conf_get_int(dictionary* ini, char* key, int* val) {
	if(!iniparser_find_entry(ini, key)) {
		fprintf(stderr, "Missing config key: %s\n", key);
		return 0;
	}

	*val = iniparser_getint(ini, key, -1);

	if(*val <= 0) {
		fprintf(stderr, "Value for key '%s' is invalid.\n", key);
		return 0;
	}

	return 1;
}

static char*
conf_get_string(dictionary* ini, char* key, char* def) {
	char*  _s;
	char*  s;
	size_t l;

	_s = iniparser_getstring(ini, key, def);

	l = strlen(_s) + 1;
	if(!(s = malloc(sizeof(char) * l))) {
		return NULL;
	}

	return strcpy(s, _s);
}

int
conf_load(char* path, config_t** conf) {
	int         i;
	dictionary* ini = NULL;
	char        namebuf[NAMEBUF_SIZE];

	if(!(*conf = malloc(sizeof(config_t)))) {
		fputs("Could not allocate memory.\n", stderr);
		goto failed;
	}

	(*conf)->iters     = NULL;
	(*conf)->colors    = NULL;
	(*conf)->statefile = NULL;
	(*conf)->output    = NULL;

	if(!(ini = iniparser_load(path))) {
		fputs("Could not parse ini file.\n", stderr);
		goto failed;
	}

	if(
	        (!conf_get_int(ini, "nebula2:width", &((*conf)->width))) ||
	        (!conf_get_int(ini, "nebula2:height", &((*conf)->height))) ||
	        (!conf_get_int(ini, "nebula2:jobsize", &((*conf)->jobsize))) ||
	        (!conf_get_int(ini, "nebula2:jobs", &((*conf)->jobs))) ||
	        (!conf_get_int(ini, "nebula2:threads", &((*conf)->threads)))) {
		goto failed;
	}

	if(!((*conf)->statefile = conf_get_string(ini, "nebula2:statefile", ""))) {
		goto failed;
	}
	if(!((*conf)->output    = conf_get_string(ini, "nebula2:output", ""))) {
		goto failed;
	}

	for((*conf)->iters_n = 0;; ((*conf)->iters_n)++) {
		if(snprintf(namebuf, NAMEBUF_SIZE, "nebula2:iter%d", (*conf)->iters_n) < 0) {
			fputs("Error while counting iterX values.\n", stderr);
			goto failed;
		}

		if(!iniparser_find_entry(ini, namebuf)) {
			break;
		}
	}

	if((*conf)->iters_n == 0) {
		fputs("Need at least one iterX value.\n", stderr);
		goto failed;
	}

	if(!((*conf)->iters = malloc(sizeof(int) * ((*conf)->iters_n)))) {
		fputs("Could not allocate memory.\n", stderr);
		goto failed;
	}
	if(!((*conf)->colors = malloc(sizeof(color_t) * ((*conf)->iters_n)))) {
		fputs("Could not allocate memory.\n", stderr);
		goto failed;
	}

	for(i = 0; i < (*conf)->iters_n; i++) {
		if(snprintf(namebuf, NAMEBUF_SIZE, "nebula2:iter%d", i) < 0) {
			fputs("Error while reading iterX values.\n", stderr);
			goto failed;
		}

		if(!conf_get_int(ini, namebuf, &((*conf)->iters[i]))) {
			goto failed;
		}

		if(snprintf(namebuf, NAMEBUF_SIZE, "nebula2:color%d", i) < 0) {
			fputs("Error while reading colorX values.\n", stderr);
			goto failed;
		}

		if(!parse_color(ini, namebuf, &((*conf)->colors[i]))) {
			goto failed;
		}
	}

	iniparser_freedict(ini);
	return 1;

failed:
	if(*conf) {
		conf_destroy(*conf);
		*conf = NULL;
	}
	if(ini) {
		iniparser_freedict(ini);
	}
	return 0;
}

void
conf_print(config_t* conf) {
	int     i;
	color_t col;

	printf("width: %d\n",     conf->width);
	printf("height: %d\n",    conf->height);
	printf("jobsize: %d\n",   conf->jobsize);
	printf("jobs: %d\n",      conf->jobs);
	printf("threads: %d\n",     conf->threads);
	printf("statefile: %s\n", conf->statefile);
	printf("output: %s\n",    conf->output);

	for(i = 0; i < conf->iters_n; i++) {
		col = conf->colors[i];
		printf("Iteration %d: %d, %02x%02x%02x\n", i, conf->iters[i], col.r, col.g, col.b);
	}
}
