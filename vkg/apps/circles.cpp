#include "vkg/test.h"

void fill_and_stroke () {
	VkvgContext ctx = vkvg_create(surf);
	vkvg_clear(ctx);
	vkvg_set_source_rgba(ctx, 0.0f, 0.1f, 0.8f, 0.5f);
	vkvg_set_line_width(ctx, 16);
	vkvg_set_line_join(ctx, VKVG_LINE_JOIN_ROUND);
	vkvg_rectangle(ctx, 32, 32, 512 - 64, 512 - 64);
	vkvg_fill_preserve(ctx);
	vkvg_set_source_rgba(ctx, 1.0f, 1.0f, 0.0f, 1.0f);
	vkvg_stroke(ctx);
	vkvg_destroy(ctx);
}

int main(int argc, char *argv[]) {
	no_test_size = true;
	PERFORM_TEST (fill_and_stroke, argc, argv);
	return 0;
}
