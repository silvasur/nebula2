#ifndef _nebula2_bmp_h_
#define _nebula2_bmp_h_

#include <stdio.h>
#include <stdint.h>

#include "color.h"

typedef struct {
	int32_t width, height;
	size_t  line_padding;
	size_t  line_left;
	FILE*   fh;
} bmp_write_handle_t;

extern bmp_write_handle_t* bmp_create(const char* fn, int32_t width, int32_t height);
extern int bmp_write_pixel(bmp_write_handle_t* bmph, color_t col);
extern void bmp_destroy(bmp_write_handle_t* bmph);

#endif
