#pragma once


typedef struct _vkg_svg_t* VkgSvg;

namespace ion {

vke_public
VkgSurface vkg_surface_create_from_svg(VkgDevice dev, uint32_t width, uint32_t height, const char* svgFilePath);

vke_public
VkgSurface vkg_surface_create_from_svg_fragment(VkgDevice dev, uint32_t width, uint32_t height, char* svgFragment);

vke_public
void vkg_svg_get_dimensions (VkgSvg svg, uint32_t* width, uint32_t* height);

vke_public
VkgSvg vkg_svg_load (const char* svgFilePath);

vke_public
VkgSvg vkg_svg_load_fragment (char* svgFragment);

vke_public
void vkg_svg_render (VkgSvg svg, VkgContext ctx, const char* id);

vke_public
void vkg_svg_destroy(VkgSvg svg);

}
