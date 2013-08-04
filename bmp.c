#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "color.h"
#include "bmp.h"

#define BYTES_PER_PIXEL 3
#define OFFSET_bfSize   2
#define OFFSET_biWidth  18
#define OFFSET_biHeight 22
#define HEADERSIZE      54

static size_t
bmp_calc_padding(int32_t width) {
	return (4 - (width % 4)) % 4;
}

static const char* header_template = "BM    \0\0\0\0\x36\0\0\0\x28\0\0\0        \x01\0\x18\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

static int
bmp_write_header(bmp_write_handle_t* bmph) {
	char* header;
	uint32_t filesize;
	int32_t height;
	
	if(!(header = malloc(HEADERSIZE))) {
		fputs("Could not allocate memory for BMP header.\n", stderr);
		return 0;
	}
	
	filesize = HEADERSIZE + (bmph->width * BYTES_PER_PIXEL + bmph->line_padding) * bmph->height;
	height = -(bmph->height);
	
	memcpy(header, header_template, HEADERSIZE);
	memcpy(header + OFFSET_bfSize, &filesize, 4);
	memcpy(header + OFFSET_biWidth, &(bmph->width), 4);
	memcpy(header + OFFSET_biHeight, &height, 4);
	
	if(fwrite(header, HEADERSIZE, 1, bmph->fh) != 1) {
		fprintf(stderr, "Could not write BMP header: %s\n", strerror(errno));
		free(header);
		return 0;
	}
	
	free(header);
	return 1;
}

bmp_write_handle_t*
bmp_create(const char* fn, int32_t width, int32_t height) {
	FILE* fh = NULL;
	bmp_write_handle_t* rv;
	
	if(!(fh = fopen(fn, "wb"))) {
		return NULL;
	}
	
	if(!(rv = malloc(sizeof(bmp_write_handle_t)))) {
		fclose(fh);
		return NULL;
	}
	
	rv->fh = fh;
	rv->width = width;
	rv->height = height;
	rv->line_left = width;
	rv->line_padding = bmp_calc_padding(width);
	
	if(!bmp_write_header(rv)) {
		fclose(fh);
		free(rv);
		return NULL;
	}
	
	return rv;
}

static const char* padding = "\0\0\0\0";

int
bmp_write_pixel(bmp_write_handle_t* bmph, color_t col) {
	uint8_t pixel[3];
	pixel[0] = col.b;
	pixel[1] = col.g;
	pixel[2] = col.r;
	
	if(fwrite(pixel, 3, 1, bmph->fh) != 1) {
		return 0;
	}
	
	if(bmph->line_padding != 0) {
		if(--(bmph->line_left) == 0) {
			bmph->line_left = bmph->width;
			
			return (fwrite(padding, bmph->line_padding, 1, bmph->fh) == 1);
		}
	}
	
	return 1;
}

void
bmp_destroy(bmp_write_handle_t* bmph) {
	if(bmph->fh) {
		fclose(bmph->fh);
	}
	
	free(bmph);
}
