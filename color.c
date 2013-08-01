#include "color.h"

static int
fix_range(int x, int min, int max) {
	if(x < min) {
		return min;
	}
	if(x > max) {
		return max;
	}
	return x;
}

color_t
color_fix(color_t col) {
	col.r = fix_range(col.r, 0, 255);
	col.g = fix_range(col.g, 0, 255);
	col.b = fix_range(col.b, 0, 255);
	return col;
}

color_t
color_add(color_t a, color_t b) {
	a.r += b.r;
	a.g += b.g;
	a.b += b.b;
	return a;
}

color_t
color_mul(color_t col, double s) {
	col.r = ((double) col.r) * s;
	col.g = ((double) col.g) * s;
	col.b = ((double) col.b) * s;
	return col;
}
