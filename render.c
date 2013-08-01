#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include "config.h"
#include "color.h"

/* Adding submap i to all submaps > i to reconstruct the result of single buddhabrot calculations. */
static void
add_submaps(config_t* conf, uint32_t* map) {
	int i, j;
	size_t k, mapsize, off_i, off_j;
	
	mapsize = conf->width * conf->height;
	
	for(i = conf->iters_n - 1; i <= 0; i--) {
		off_i = mapsize * i;
		for(j = 0; j < i; j++) {
			off_j = mapsize * j;
			for(k = 0; k < mapsize; k++) {
				map[off_i + k] += map[off_j + k];
			}
		}
	}
}

#define BYTES_PER_PIXEL 3
#define OFFSET_bfSize   2
#define OFFSET_biWidth  18
#define OFFSET_biHeight 22
#define HEADERSIZE      54

static const char* header_template = "BM    \0\0\0\0\x36\0\0\0\x28\0\0\0        \x01\0\x18\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

static int
bmp_write_header(FILE* fh, int32_t width, int32_t height) {
	char* header;
	uint32_t filesize;
	
	if(!(header = malloc(HEADERSIZE))) {
		fputs("Could not allocate memory for BMP header.\n", stderr);
		return 0;
	}
	
	filesize = HEADERSIZE + (width * height * BYTES_PER_PIXEL);
	height *= -1;
	
	memcpy(header, header_template, HEADERSIZE);
	memcpy(header + OFFSET_bfSize, &filesize, 4);
	memcpy(header + OFFSET_biWidth, &width, 4);
	memcpy(header + OFFSET_biHeight, &height, 4);
	
	if(fwrite(header, HEADERSIZE, 1, fh) != 1) {
		fprintf(stderr, "Could not write BMP header: %s\n", strerror(errno));
		free(header);
		return 0;
	}
	
	free(header);
	return 1;
}

static int
bmp_write_pixel(FILE* fh, color_t col) {
	uint8_t pixel[3];
	pixel[0] = col.b;
	pixel[1] = col.g;
	pixel[2] = col.r;
	
	return (fwrite(pixel, 3, 1, fh) == 1);
}

typedef struct lookup_elem_t {
	uint32_t val;
	struct lookup_elem_t* prev;
	struct lookup_elem_t* next;
} lookup_elem_t;

static lookup_elem_t*
new_lookup_elem(uint32_t val, lookup_elem_t* prev, lookup_elem_t* next) {
	lookup_elem_t* rv = malloc(sizeof(lookup_elem_t));
	if(!rv) {
		return NULL;
	}
	
	rv->val = val;
	rv->prev = prev;
	rv->next = next;
	
	return rv;
}

static int
maybe_insert(uint32_t val, lookup_elem_t* a, lookup_elem_t* b) {
	lookup_elem_t* new;
	
	if(!(new = new_lookup_elem(val, a, b))) {
		return 0;
	}
	
	if(!a) {
		b->prev = new;
		return 1;
	}
	if(!b) {
		a->next = new;
		return 1;
	}
	
	if((a->val < val) && (val < b->val)) {
		a->next = new;
		b->prev = new;
	} else {
		free(new);
	}
	return 1;
}

static int
build_lookup(uint32_t* map, size_t mapsize, lookup_elem_t** e) {
	size_t i;
	uint32_t val;
	
	for(i = 0; i < mapsize; i++) {
		val = map[i];
		
		if(!*e) {
			if(!(*e = new_lookup_elem(val, NULL, NULL))) {
				return 0;
			}
			continue;
		}
		
		for(;;) {
			if((*e)->val == val) {
				break;
			}
			
			if(val > (*e)->val) {
				if(!maybe_insert(val, *e, (*e)->next)) {
					return 0;
				}
				*e = (*e)->next;
			} else {
				if(!maybe_insert(val, (*e)->prev, *e)) {
					return 0;
				}
				*e = (*e)->prev;
			}
		}
	}
	
	while((*e)->prev) {
		*e = (*e)->prev;
	}
	
	return 1;
}

static size_t
lookup_len(lookup_elem_t* e) {
	size_t len;
	
	if(!e) {
		return 0;
	}
	
	while(e->prev) {
		e = e->prev;
	}
	
	len = 0;
	while(e) {
		len++;
		e = e->next;
	}
	return len;
}

static void
lookup_destroy(lookup_elem_t* e) {
	lookup_elem_t* next;
	while(e->prev) {
		e = e->prev;
	}
	while(e) {
		next = e->next;
		free(e);
		e = next;
	}
}

static size_t
lookup(uint32_t val, lookup_elem_t** e, size_t* pos) {
	while((*e)->val != val) {
		if(val < (*e)->val) {
			*e = (*e)->prev;
			(*pos)--;
		} else {
			*e = (*e)->next;
			(*pos)++;
		}
	}
	return *pos;
}

int
render(config_t* conf, uint32_t* map) {
	int rv = 0;
	size_t i, j;
	color_t col;
	double factor;
	FILE* fh = NULL;
	size_t mapsize = conf->width * conf->height;
	
	lookup_elem_t** lookups = NULL;
	size_t* poss = NULL;
	size_t* lens = NULL;
	
	lookups = calloc(sizeof(lookup_elem_t*), conf->iters_n);
	poss = calloc(sizeof(size_t), conf->iters_n);
	lens = calloc(sizeof(size_t), conf->iters_n);
	if((!lookups) || (!poss) || (!lens)) {
		fputs("Could not allocate memory for lookup tables.\n", stderr);
		goto tidyup;
	}
	
	if(!(fh = fopen(conf->output, "wb"))) {
		fprintf(stderr, "Could not open output file '%s': %s\n", conf->output, strerror(errno));
		goto tidyup;
	}
	
	if(!bmp_write_header(fh, conf->width, conf->height)) {
		goto tidyup;
	}
	
	add_submaps(conf, map);
	
	for(i = 0; i < conf->iters_n; i++) {
		if(!build_lookup(map+(mapsize*i), mapsize, &(lookups[i]))) {
			fputs("Could not build lookup table.\n", stderr);
			goto tidyup;
		}
		lens[i] = lookup_len(lookups[i]);
	}
	
	for(i = 0; i < mapsize; i++) {
		col.r = 0;
		col.g = 0;
		col.b = 0;
		
		for(j = 0; j < conf->iters_n; j++) {
			factor = (double) lookup(map[i + mapsize * j], &(lookups[j]), &(poss[j])) / (double) lens[j];
			col = color_add(col, color_mul(conf->colors[j], factor));
		}
		if(!bmp_write_pixel(fh, color_fix(col))) {
			fputs("Could not write pixel data.", stderr);
			goto tidyup;
		}
	}
	
	rv = 1;
	
tidyup:
	if(fh) {
		fclose(fh);
	}
	if(lookups) {
		for(i = 0; i < conf->iters_n; i++) {
			if(lookups[i]) {
				lookup_destroy(lookups[i]);
			}
		}
		free(lookups);
	}
	if(poss) {
		free(poss);
	}
	if(lens) {
		free(lens);
	}
	
	return rv;
}
