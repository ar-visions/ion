#pragma once


typedef struct _vkvg_svg_t* VkvgSvg;

namespace ion {

vke_public
VkvgSurface vkvg_surface_create_from_svg(VkvgDevice dev, uint32_t width, uint32_t height, const char* svgFilePath);

vke_public
VkvgSurface vkvg_surface_create_from_svg_fragment(VkvgDevice dev, uint32_t width, uint32_t height, char* svgFragment);

vke_public
void vkvg_svg_get_dimensions (VkvgSvg svg, uint32_t* width, uint32_t* height);

vke_public
VkvgSvg vkvg_svg_load (const char* svgFilePath);

vke_public
VkvgSvg vkvg_svg_load_fragment (char* svgFragment);

vke_public
void vkvg_svg_render (VkvgSvg svg, VkvgContext ctx, const char* id);

vke_public
void vkvg_svg_destroy(VkvgSvg svg);

}
