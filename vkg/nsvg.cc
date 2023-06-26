/*
 * Copyright (c) 2018-2020 Jean-Philippe Bruyère <jp_bruyere@hotmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
 * Software, and to permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <vkg/internal.hh>

#define NANOSVG_IMPLEMENTATION	// Expands implementation
#include "nanosvg.h"
#include <vkg/vkvg-svg.h>

void _svg_set_color (VkgContext ctx, uint32_t c, float alpha) {
	float a = (c >> 24 & 255) / 255.f;
	float b = (c >> 16 & 255) / 255.f;
	float g = (c >> 8 & 255) / 255.f;
	float r = (c & 255) / 255.f;
	vkg_set_source_rgba(ctx,r,g,b,a*alpha);
}

VkgSurface _svg_load (VkgDevice dev, NSVGimage* svg) {
	if (svg == NULL) {
		vke_log(VKE_LOG_ERR, "nsvg error");
		return NULL;
	}
	VkgSurface surf = _create_surface(dev, FB_COLOR_FORMAT);
	if (!surf)
		return NULL;

	surf->width = (uint32_t)svg->width;
	surf->height = (uint32_t)svg->height;
	surf->newSurf = true;

	_create_surface_images (surf);

	VkgContext ctx = vkg_create(surf);
	vkg_svg_render(svg, ctx, NULL);
	io_drop(vkg_context, ctx);

	nsvgDelete(svg);

	io_grab(vkg_device, surf->dev); /// surf references device
	return surf;
}

/// 72 is set for svgs being saved
VkgSurface vkg_surface_create_from_svg (VkgDevice dev, uint32_t width, uint32_t height, const char* filePath) {
	return _svg_load(dev, nsvgParseFromFile(filePath, "px", (float)dev->vke->physinfo->hdpi));
}
VkgSurface vkg_surface_create_from_svg_fragment (VkgDevice dev, uint32_t width, uint32_t height, char* svgFragment) {
	return _svg_load(dev, nsvgParse(svgFragment, "px", (float)dev->vke->physinfo->hdpi));
}
VkgSvg vkg_svg_load (const char* svgFilePath) {
	return nsvgParseFromFile(svgFilePath, "px", 96.0f);
}
VkgSvg vkg_svg_load_fragment (char* svgFragment) {
	return nsvgParse (svgFragment, "px", 96.0f);
}
void vkg_svg_destroy(VkgSvg svg) {
	nsvgDelete(svg);
}
void vkg_svg_get_dimensions (VkgSvg svg, uint32_t* width, uint32_t* height) {
	*width  = (uint32_t)svg->width;
	*height = (uint32_t)svg->height;
}

void vkg_svg_render (VkgSvg svg, VkgContext ctx, const char* subId){
	NSVGshape* shape;
	NSVGpath* path;
	vkg_save (ctx);

	vkg_set_fill_rule(ctx, VKVG_FILL_RULE_EVEN_ODD);

	vkg_set_source_rgba(ctx,0.0,0.0,0.0,1);

	for (shape = svg->shapes; shape != NULL; shape = shape->next) {
		if (subId != NULL) {
			if (strcmp(shape->id, subId)!=0)
				continue;
		}

		vkg_new_path(ctx);

		float o = shape->opacity;

		vkg_set_line_width(ctx, shape->strokeWidth);

		for (path = shape->paths; path != NULL; path = path->next) {
			float* p = path->pts;
			vkg_move_to(ctx, p[0],p[1]);
			for (int i = 1; i < path->npts; i += 3) {
				p = &path->pts[i*2];
				vkg_curve_to(ctx, p[0],p[1], p[2],p[3], p[4],p[5]);
			}
			if (path->closed)
				vkg_close_path(ctx);
		}

		if (shape->fill.type == NSVG_PAINT_COLOR)
			_svg_set_color(ctx, shape->fill.color, o);
		else if (shape->fill.type == NSVG_PAINT_LINEAR_GRADIENT){
			NSVGgradient* g = shape->fill.gradient;
			_svg_set_color(ctx, g->stops[0].color, o);
		}

		if (shape->fill.type != NSVG_PAINT_NONE){
			if (shape->stroke.type == NSVG_PAINT_NONE){
				vkg_fill(ctx);
				continue;
			}
			vkg_fill_preserve (ctx);
		}

		if (shape->stroke.type == NSVG_PAINT_COLOR)
			_svg_set_color(ctx, shape->stroke.color, o);
		else if (shape->stroke.type == NSVG_PAINT_LINEAR_GRADIENT){
			NSVGgradient* g = shape->stroke.gradient;
			_svg_set_color(ctx, g->stops[0].color, o);
		}

		vkg_stroke(ctx);
	}
	vkg_restore (ctx);
}
