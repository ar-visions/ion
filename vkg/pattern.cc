/*
 * Copyright (c) 2018-2022 Jean-Philippe Bruyère <jp_bruyere@hotmail.com>
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

/// free is just the inner space.  teh outter space is freed by obi
/// creation with obi looks up symbols for _free, _grab and _drop

void VkgPattern::free(VkgPattern pat) {
	free(pat->data);
}

VkgPattern VkgPattern::create_for_surface (VkgSurface surf) {
	pat->type   = VKVG_PATTERN_TYPE_SURFACE;
	pat->extend = VKVG_EXTEND_NONE;
	pat->data   = io_grab(vkg_surface, surf);
	if (surf->status) pat->status = surf->status;
	return pat;
}

VkeStatus VkgPattern::get_linear_points (VkgPattern pat, float* x0, float* y0, float* x1, float* y1) {
	if (pat->status)
		return pat->status;
	if (pat->type != VKVG_PATTERN_TYPE_LINEAR)
		return VKE_STATUS_PATTERN_TYPE_MISMATCH;

	vkg_gradient* grad = (vkg_gradient*)pat->data;

	*x0 = grad->cp[0][X];
	*y0 = grad->cp[0][Y];
	*x1 = grad->cp[0][Z];
	*y1 = grad->cp[0][W];
	return VKE_STATUS_SUCCESS;
}
VkeStatus VkgPattern::edit_linear (VkgPattern pat, float x0, float y0, float x1, float y1){
	if (pat->status)
		return pat->status;
	if (pat->type != VKVG_PATTERN_TYPE_LINEAR)
		return VKE_STATUS_PATTERN_TYPE_MISMATCH;

	vkg_gradient* grad = (vkg_gradient*)pat->data;

	grad->cp[0] = (vec4f){x0, y0, x1, y1};
	return VKE_STATUS_SUCCESS;
}
VkgPattern VkgPattern::create_linear (float x0, float y0, float x1, float y1){

	mx_init(vkg_pattern, pat);

	pat->type   = VKVG_PATTERN_TYPE_LINEAR;
	pat->extend = VKVG_EXTEND_NONE;
	mx_init(vkg_gradient, pat->data);
	VkgPattern::edit_linear(pat, x0, y0, x1, y1);
	return pat;
}

VkeStatus VkgPattern::edit_radial (VkgPattern pat,
										float cx0, float cy0, float radius0,
										float cx1, float cy1, float radius1) {
	if (pat->status)
		return pat->status;
	if (pat->type != VKVG_PATTERN_TYPE_RADIAL)
		return VKE_STATUS_PATTERN_TYPE_MISMATCH;

	vkg_gradient* grad = (vkg_gradient*)pat->data;

	vec2f c0 = {cx0, cy0};
	vec2f c1 = {cx1, cy1};

	if (radius0 > radius1 - 1.0f)
		radius0 = radius1 - 1.0f;
	vec2f u = vec2f_sub (c0, c1);
	float l = vec2f_len(u);
	if (l + radius0 + 1.0f >= radius1) {
		vec2f v;
		vec2f_div_s(v, u, l);
		vec2f_add(c0, c1, vec2f_scale (v, radius1 - radius0 - 1.0f));
	}

	grad->cp[0] = (vec4f){c0[X], c0[Y], radius0, 0};
	grad->cp[1] = (vec4f){c1[X], c1[Y], radius1, 0};
	return VKE_STATUS_SUCCESS;
}

VkgPattern VkgPattern::create_radial (float cx0, float cy0, float radius0,
										float cx1, float cy1, float radius1) {

	VkgPattern pat;
	mx_init(vkg_pattern, pat);

	pat->type = VKVG_PATTERN_TYPE_RADIAL;
	pat->extend = VKVG_EXTEND_NONE;

	mx_init(vkg_gradient, pat->data);

	if (pat->data) {
		VkgPattern::edit_radial (pat, cx0, cy0, radius0, cx1, cy1, radius1);
		pat->refs = 1;
	} else
		pat->status = VKE_STATUS_NO_MEMORY;

	return pat;
}

uint32_t VkgPattern::get_reference_count (VkgPattern pat) {
	return pat->refs;
}

VkeStatus VkgPattern::add_color_stop (VkgPattern pat, float offset, float r, float g, float b, float a) {
	if (pat->status)
		return pat->status;
	if (pat->type == VKVG_PATTERN_TYPE_SURFACE || pat->type == VKVG_PATTERN_TYPE_SOLID)
		return VKE_STATUS_PATTERN_TYPE_MISMATCH;

	vkg_gradient* grad = (vkg_gradient*)pat->data;
#ifdef VKVG_PREMULT_ALPHA
	vkg_color_t c = {a*r,a*g,a*b,a};
#else
	vkg_color_t c = {r,g,b,a};
#endif
	grad->colors[grad->count] = c;
#ifdef VKVG_ENABLE_VK_SCALAR_BLOCK_LAYOUT
	grad->stops[grad->count] = offset;
#else
	grad->stops[grad->count].r = offset;
#endif
	grad->count++;
	return VKE_STATUS_SUCCESS;
}
void VkgPattern::set_extend (vkg_extend extend){
	if (pat->status)
		return;
	pat->extend = extend;
}
void VkgPattern::set_filter (vkg_filter filter){
	if (pat->status)
		return;
	pat->filter = filter;
}

vkg_extend_t VkgPattern::get_extend (){
	if (pat->status)
		return (vkg_extend_t)0;
	return pat->extend;
}
vkg_filter_t VkgPattern::get_filter (){
	if (pat->status)
		return (vkg_filter_t)0;
	return pat->filter;
}

vkg_pattern_type VkgPattern::get_type (){
	if (pat->status)
		return (VkgPattern::type_t)0;
	return pat->type;
}

VkeStatus VkgPattern::get_color_stop_count (uint32_t* count) {
	if (pat->status)
		return pat->status;
	if (pat->type == VKVG_PATTERN_TYPE_SURFACE || pat->type == VKVG_PATTERN_TYPE_SOLID)
		return VKE_STATUS_PATTERN_TYPE_MISMATCH;
	vkg_gradient* grad = (vkg_gradient*)pat->data;
	*count = grad->count;
	return VKE_STATUS_SUCCESS;
}

VkeStatus VkgPattern::get_color_stop_rgba (uint32_t index,
												float* offset, float* r, float* g, float* b, float* a) {
	if (pat->status)
		return pat->status;
	if (pat->type == VKVG_PATTERN_TYPE_SURFACE || pat->type == VKVG_PATTERN_TYPE_SOLID)
		return VKE_STATUS_PATTERN_TYPE_MISMATCH;
	vkg_gradient* grad = (vkg_gradient*)pat->data;
	if (index >= grad->count)
		return VKE_STATUS_INVALID_INDEX;
#ifdef VKVG_ENABLE_VK_SCALAR_BLOCK_LAYOUT
	*offset = grad->stops[index];
#else
	*offset = grad->stops[index].r;
#endif
	vkg_color_t c = grad->colors[index];
	*r = c.r;
	*g = c.g;
	*b = c.b;
	*a = c.a;
	return VKE_STATUS_SUCCESS;
}

void VkgPattern::set_matrix (VkgMatrix matrix) {
	if (pat->status)
		return;
	pat->matrix		= *matrix;
	pat->hasMatrix	= true;
}

void VkgPattern::get_matrix (VkgMatrix matrix) {
	if (pat->status)
		return;
	if (pat->hasMatrix)
		*matrix = pat->matrix;
	else
		*matrix = VKVG_IDENTITY_MATRIX;
}

VkeStatus VkgPattern::status (VkgPattern pat) {
	return pat->status;
}
