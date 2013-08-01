#ifndef _nebula2_bmp_h_
#define _nebula2_bmp_h_

#include <stdio.h>
#include <stdint.h>

#include "color.h"

extern int bmp_write_header(FILE* fh, int32_t width, int32_t height);
extern int bmp_write_pixel(FILE* fh, color_t col);

#endif