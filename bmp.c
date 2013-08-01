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

static const char* header_template = "BM    \0\0\0\0\x36\0\0\0\x28\0\0\0        \x01\0\x18\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

int
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

int
bmp_write_pixel(FILE* fh, color_t col) {
	uint8_t pixel[3];
	pixel[0] = col.b;
	pixel[1] = col.g;
	pixel[2] = col.r;
	
	return (fwrite(pixel, 3, 1, fh) == 1);
}
