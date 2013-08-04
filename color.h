#ifndef _nebula2_color_h_
#define _nebula2_color_h_

typedef struct {
	int r, g, b;
} color_t;

extern color_t color_fix(color_t col);
extern color_t color_add(color_t a, color_t b);
extern color_t color_mul(color_t col, double s);

#endif
