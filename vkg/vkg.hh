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

/* alterations by kalen novis */

#pragma once

#include <mx/mx.hpp>
#include <vke/vke.hh>
#include <math/math.hpp>
#include <vulkan/vulkan.h>
#include <math.h>
#include <stdbool.h>

namespace ion {
///
#define FONT_PAGE_SIZE			1024
#define FONT_CACHE_INIT_LAYERS	1
#define FONT_FILE_NAME_MAX_SIZE 1024
#define FONT_NAME_MAX_SIZE		128

enum vkg_direction {
	VKVG_HORIZONTAL	= 0,
	VKVG_VERTICAL	= 1
};

enum vkg_format {
	VKVG_FORMAT_ARGB32,
	VKVG_FORMAT_RGB24,
	VKVG_FORMAT_A8,
	VKVG_FORMAT_A1
};

enum vkg_extend {
	VKVG_EXTEND_NONE,			/*!< nothing will be outputed outside the pattern original bounds */
	VKVG_EXTEND_REPEAT,			/*!< pattern will be repeated to fill all the target bounds */
	VKVG_EXTEND_REFLECT,		/*!< pattern will be repeated but mirrored on each repeat */
	VKVG_EXTEND_PAD				/*!< the last pixels making the borders of the pattern will be extended to the whole target */
};

enum vkg_filter {
	VKVG_FILTER_FAST,
	VKVG_FILTER_GOOD,
	VKVG_FILTER_BEST,
	VKVG_FILTER_NEAREST,
	VKVG_FILTER_BILINEAR,
	VKVG_FILTER_GAUSSIAN,
};

enum vkg_pattern_type {
	VKVG_PATTERN_TYPE_SOLID,			/*!< single color pattern */
	VKVG_PATTERN_TYPE_SURFACE,			/*!< vkvg surface pattern */
	VKVG_PATTERN_TYPE_LINEAR,			/*!< linear gradient pattern */
	VKVG_PATTERN_TYPE_RADIAL,			/*!< radial gradient pattern */
	VKVG_PATTERN_TYPE_MESH,				/*!< not implemented */
	VKVG_PATTERN_TYPE_RASTER_SOURCE,	/*!< not implemented */
};

enum vkg_clip_state {
	vkg_clip_state_none			= 0x00,
	vkg_clip_state_clear		= 0x01,
	vkg_clip_state_clip			= 0x02,
	vkg_clip_state_clip_saved	= 0x06,
};

enum vkg_line_cap {
	VKVG_LINE_CAP_BUTT,		/*!< normal line endings, this is the default. */
	VKVG_LINE_CAP_ROUND,	/*!< rounded line caps */
	VKVG_LINE_CAP_SQUARE	/*!< extend the caps with squared terminations having border equal to current line width. */
};

enum vkg_line_join {
	VKVG_LINE_JOIN_MITER,	/*!< normal joins with sharp angles, this is the default. */
	VKVG_LINE_JOIN_ROUND,	/*!< joins are rounded on the exterior border of the line. */
	VKVG_LINE_JOIN_BEVEL	/*!< beveled line joins. */
};

enum vkg_fill_rule {
	VKVG_FILL_RULE_EVEN_ODD,	/*!< even-odd fill rule */
	VKVG_FILL_RULE_NON_ZERO		/*!< non zero fill rule */
};

enum vkg_operator {
	VKVG_OPERATOR_CLEAR,

	VKVG_OPERATOR_SOURCE,
	VKVG_OPERATOR_OVER,
/*	VKVG_OPERATOR_IN,
	VKVG_OPERATOR_OUT,
	VKVG_OPERATOR_ATOP,

	VKVG_OPERATOR_DEST,
	VKVG_OPERATOR_DEST_OVER,
	VKVG_OPERATOR_DEST_IN,
	VKVG_OPERATOR_DEST_OUT,
	VKVG_OPERATOR_DEST_ATOP,

	VKVG_OPERATOR_XOR,
	VKVG_OPERATOR_ADD,
	VKVG_OPERATOR_SATURATE,

	VKVG_OPERATOR_MULTIPLY,
	VKVG_OPERATOR_SCREEN,
	VKVG_OPERATOR_OVERLAY,
	VKVG_OPERATOR_DARKEN,
	VKVG_OPERATOR_LIGHTEN,
	VKVG_OPERATOR_COLOR_DODGE,
	VKVG_OPERATOR_COLOR_BURN,
	VKVG_OPERATOR_HARD_LIGHT,
	VKVG_OPERATOR_SOFT_LIGHT,
	*/VKVG_OPERATOR_DIFFERENCE,/*
	VKVG_OPERATOR_EXCLUSION,
	VKVG_OPERATOR_HSL_HUE,
	VKVG_OPERATOR_HSL_SATURATION,
	VKVG_OPERATOR_HSL_COLOR,
	VKVG_OPERATOR_HSL_LUMINOSITY,*/
	VKVG_OPERATOR_MAX,
};

#define VKVG_IDENTITY_MATRIX (vkg_matrix){1,0,0,1,0,0}/*!< The identity matrix*/
struct vkg_matrix {
	float xx; float yx;
	float xy; float yy;
	float x0; float y0;
};

struct vkg_record {
	uint16_t		cmd;
	size_t			dataOffset;
};

struct vkg_recording {
	vkg_record*		commands;
	uint32_t		commandsCount;
	uint32_t		commandsReservedCount;
	size_t			bufferSize;
	size_t			bufferReservedSize;
	char*			buffer;
};

#define VKVG_PTS_SIZE				1024
#define VKVG_VBO_SIZE				(VKVG_PTS_SIZE * 4)
#define VKVG_IBO_SIZE				(VKVG_VBO_SIZE * 6)
#define VKVG_PATHES_SIZE			16
#define VKVG_ARRAY_THRESHOLD		8

#define VKVG_IBO_16					0
#define VKVG_IBO_32					1

#define VKVG_CUR_IBO_TYPE			VKVG_IBO_32//change this only

#if VKVG_CUR_IBO_TYPE == VKVG_IBO_16
	#define VKVG_IBO_MAX			UINT16_MAX
	#define VKVG_IBO_INDEX_TYPE		uint16_t
	#define VKVG_VK_INDEX_TYPE		VK_INDEX_TYPE_UINT16
#else
	#define VKVG_IBO_MAX			UINT32_MAX
	#define VKVG_IBO_INDEX_TYPE		uint32_t
	#define VKVG_VK_INDEX_TYPE		VK_INDEX_TYPE_UINT32
#endif

struct vkg_color {
	float r;					/*!< the red component */
	float g;					/*!< the green component */
	float b;					/*!< the blue component */
	float a;					/*!< the alpha component */
};

struct vkg_font_extents {
	float ascent;			/*!< the distance that the font extends above the baseline. */
	float descent;			/*!< the distance that the font extends below the baseline.*/
	float height;			/*!< the recommended vertical distance between baselines. */
	float max_x_advance;	/*!< the maximum distance in the X direction that the origin is advanced for any glyph in the font.*/
	float max_y_advance;	/*!< the maximum distance in the Y direction that the origin is advanced for any glyph in the font. This will be zero for normal fonts used for horizontal writing.*/
};

struct vkg_text_extents {
	float x_bearing;		/*!< the horizontal distance from the origin to the leftmost part of the glyphs as drawn. Positive if the glyphs lie entirely to the right of the origin. */
	float y_bearing;		/*!< the vertical distance from the origin to the topmost part of the glyphs as drawn. Positive only if the glyphs lie completely below the origin; will usually be negative.*/
	float width;			/*!< width of the glyphs as drawn*/
	float height;			/*!< height of the glyphs as drawn*/
	float x_advance;		/*!< distance to advance in the X direction after drawing these glyphs*/
	float y_advance;		/*!< distance to advance in the Y direction after drawing these glyphs. Will typically be zero except for vertical text layout as found in East-Asian languages.*/
};

struct vkg_glyph_info {
	int32_t  x_advance;
	int32_t  y_advance;
	int32_t  x_offset;
	int32_t  y_offset;
	/* private */
	uint32_t codepoint;//should be named glyphIndex, but for harfbuzz compatibility...
};

#if VKVG_DBG_STATS
struct vkg_debug_stats {
	uint32_t	sizePoints;		/**< maximum point array size					*/
	uint32_t	sizePathes;		/**< maximum path array size					*/
	uint32_t	sizeVertices;	/**< maximum size of host vertice cache			*/
	uint32_t	sizeIndices;	/**< maximum size of host index cache			*/
	uint32_t	sizeVBO;		/**< maximum size of vulkan vertex buffer		*/
	uint32_t	sizeIBO;		/**< maximum size of vulkan index buffer		*/
};

vkg_debug_stats vkg_device_get_stats (VkgDevice dev);
vkg_debug_stats vkg_device_reset_stats (VkgDevice dev);
#endif


/// must merge with glm facility in math. there isnt much to add here
struct VkgMatrix:mx {
	ptr(VkgMatrix, mx, vkg_matrix);
	///
	VkeStatus 	invert();
	void 		init_identity();
	void 		init(float xx, float yx, float xy, float yy, float x0, float y0);
	///
	static VkgMatrix translated(float tx, float ty);
	static VkgMatrix scaled(float sx, float sy);
	static VkgMatrix rotated(float radians);
	///
	void translate			(float tx, float ty);
	void scale				(float sx, float sy);
	void rotate				(float radians);
	void multiply			(VkgMatrix a, VkgMatrix b);
	void transform_distance	(float *dx, float *dy);
	void transform_point	(float *x, float *y);
	void get_scale			(float *sx, float *sy);
};

struct VkgDevice:mx {
	ptr(VkgDevice, mx, struct vkg_device);
	///
	VkgDevice(VkeDevice, VkSampleCountFlagBits, bool);
	// deprecating these:
	//VkgDevice (VkeInstance inst, VkeGPU phy, VkeDevice vkdev, uint32_t qFamIdx, uint32_t qIndex);
	//VkgDevice (VkeInstance inst, VkeGPU phy, VkeDevice vkdev, uint32_t qFamIdx, uint32_t qIndex,
	//		   VkeSampleCountFlagBits samples, bool deferredResolve);
	
	void 		set_context_cache_size (uint32_t maxCount);

	VkeStatus 	device_status ();
	void 		set_dpi (vec2i *p);
	vec2i 		get_dpi ();
	VkeStatus 	get_required_device_extensions(VkPhysicalDevice phy, const char** pExtensions, uint32_t* pExtCount);
	const void* get_device_requirements (VkPhysicalDeviceFeatures* pEnabledFeatures);
};

struct VkgSurface:mx {
	ptr(VkgSurface, mx, struct vkg_surface);
	/// constructors
	VkgSurface 	(VkgDevice dev, uint32_t width, uint32_t height);
	VkgSurface 	(VkgDevice dev, const char* 	filePath);
	VkgSurface 	(VkgDevice dev, void* 			vkhImg);
	VkgSurface 	(VkgDevice dev, unsigned char* 	img, uint32_t width, uint32_t height);
	/// methods
	VkeStatus 	status 			();
	void 		clear 			();
	VkImage		get_vk_image 	();
	VkFormat 	get_vk_format 	();
	uint32_t 	get_width 		();
	uint32_t 	get_height 		();
	VkeStatus 	write_to_png 	(const char* path);
	VkeStatus 	write_to_memory (unsigned char* const bitmap);
	void 		resolve 		();
};

struct stroke_context;
struct dash_context;

struct VkgPattern:mx {
	ptr(VkgPattern, mx, struct vkg_pattern);

	static VkgPattern 	create_for_surface(VkgSurface surf);
	static VkgPattern 	create_linear(float x0, float y0, float x1, float y1);
	static VkgPattern 	create_radial(float cx0, float cy0, float radius0, float cx1, float cy1, float radius1);

	VkeStatus 			set_linear			(float  x0, float  y0, float  x1, float  y1);
	VkeStatus 			get_linear_points	(float* x0, float* y0, float* x1, float* y1);
	VkeStatus 			edit_radial			(float cx0, float cy0, float radius0, float cx1, float cy1, float radius1);
	VkeStatus 			get_color_stop_count(uint32_t* count);
	VkeStatus 			get_color_stop_rgba (uint32_t index, float* offset, float* r, float* g, float* b, float* a);
	VkeStatus 			add_color_stop		(float offset, float r, float g, float b, float a);
	void 				set_extend			(vkg_extend extend);
	void 				set_filter			(vkg_filter filter);
	vkg_extend 			get_extend 			();
	vkg_filter 			get_filter 			();
	vkg_pattern_type 	get_type 			();
	void 				set_matrix 			(VkgMatrix m);
	void 				get_matrix 			(VkgMatrix m);
};

struct VkgVertex {
	vec2f    		pos;
	uint32_t		color;
	vec3f			uv;
};

struct vkg_context_save;

struct VkgContext:mx {
	ptr(VkgContext, mx, struct vkg_context);

	VkgContext(VkgSurface surf);

	VkeStatus 		status 						();
	const char* 	status_to_string 			(VkeStatus status);
	void 			flush 						();
	void 			new_path 					();
	void 			close_path 					();
	void 			new_sub_path 				();
	void 			path_extents 				(float *x1, float *y1, float *x2, float *y2);
	void 			get_current_point 			(float* x, float* y);
	void 			line_to 					(float x, float y);
	void 			rel_line_to 				(float dx, float dy);
	void 			move_to 					(float x, float y);
	void 			rel_move_to 				(float x, float y);
	void 			arc 						(float xc, float yc, float radius, float a1, float a2);
	void 			arc_negative 				(float xc, float yc, float radius, float a1, float a2);
	void 			curve_to 					(float x1, float y1, float x2, float y2, float x3, float y3);
	void 			rel_curve_to 				(float x1, float y1, float x2, float y2, float x3, float y3);
	void 			quadratic_to 				(float x1, float y1, float x2, float y2);
	void 			rel_quadratic_to 			(float x1, float y1, float x2, float y2);
	VkeStatus 		rectangle					(float x, float y, float w, float h);
	VkeStatus 		rounded_rectangle 			(float x, float y, float w, float h, float radius);
	void 	  		rounded_rectangle2 			(float x, float y, float w, float h, float rx, float ry);
	void 			ellipse 					(float radiusX, float radiusY, float x, float y, float rotationAngle);
	void 			elliptic_arc_to 			(float x, float y, bool large_arc_flag, bool sweep_flag, float rx, float ry, float phi);
	void 			rel_elliptic_arc_to 		(float x, float y, bool large_arc_flag, bool sweep_flag, float rx, float ry, float phi);
	void 			stroke 						();
	void 			stroke_preserve 			();
	void 			fill 						();
	void 			fill_preserve 				();
	void 			paint 						();
	void 			clear 						();//use vkClearAttachment to speed up clearing surf
	void 			reset_clip	 				();
	void 			clip 						();
	void 			clip_preserve 				();
	void 			set_opacity 				(float opacity);
	void 			set_source_color 			(uint32_t c);
	void 			set_source_rgba 			(float r, float g, float b, float a);
	void 			set_source_rgb 				(float r, float g, float b);
	void 			set_line_width 				(float width);
	void 			set_miter_limit 			(float limit);
	float 			get_miter_limit 			();
	void 			set_line_cap 				(vkg_line_cap cap);
	void 			set_line_join 				(vkg_line_join join);
	void 			set_source_surface 			(VkgSurface surf, float x, float y);
	void 			set_source 					(VkgPattern pat);
	void 			set_operator 				(vkg_operator op);
	void 			set_fill_rule 				(vkg_fill_rule fr);
	void 			set_dash 					(const float* dashes, uint32_t num_dashes, float offset);
	void 			get_dash 					(const float *dashes, uint32_t* num_dashes, float* offset);
	float 			get_line_width 				();
	vkg_line_cap   	get_line_cap 				();
	vkg_line_join   get_line_join 				();
	vkg_operator   	get_operator 				();
	vkg_fill_rule   get_fill_rule 				();
	VkgPattern 		get_source 					();
	VkgSurface 		get_target 					();
	bool 			has_current_point 			();
	void 			save 						();
	void 			restore 					();
	void 			translate 					(float dx, float dy);
	void 			scale 						(float sx, float sy);
	void 			rotate 						(float radians);
	void 			transform 					(const vkg_matrix* matrix);
	void 			set_matrix 					(const vkg_matrix* matrix);
	void 			get_matrix 					(vkg_matrix * const matrix);
	void 			identity_matrix 			();
	void 			select_font_face 			(const char* name);
	void 			load_font_from_path 		(const char* path, const char *name);
	void 			load_font_from_memory 		(unsigned char* fontBuffer, long fontBufferByteSize, const char* name); // there can be only one. dick.
	void 			set_font_size 				(uint32_t size);
	void 			show_text 					(const char* utf8);
	void 			text_extents 				(const char* utf8, vkg_text_extents* extents);
	void 			font_extents 				(vkg_font_extents* extents);
	bool 			check_point_array			();

	/// these are internally implemented and protected.  its good to have them in the header.
	protected:
	void 			check_vertex_cache_size		();
	void 			ensure_vertex_cache_size	(uint32_t addedVerticesCount);
	void 			resize_vertex_cache			(uint32_t newSize);
	void 			check_index_cache_size		();
	void 			ensure_index_cache_size		(uint32_t addedIndicesCount);
	void 			resize_index_cache			(uint32_t newSize);
	bool 			check_pathes_array			();
	bool 			current_path_is_empty		();
	void 			finish_path					();
	void 			clear_path					();
	void 			remove_last_point			();
	bool 			path_is_closed				(uint32_t ptrPath);
	void 			set_curve_start				();
	void 			set_curve_end				();
	bool 			path_has_curves				(uint32_t ptrPath);
	float 			normalizeAngle				(float a);
	float 			get_arc_step				(float radius);
	vec2f*			get_current_position    	();
	void 			add_point					(float x, float y);
	void 			resetMinMax					();
	void 			path_extents				(bool transformed, float *x1, float *y1, float *x2, float *y2);
	void 			draw_stoke_cap				(stroke_context* str, vec2f p0, vec2f n, bool isStart);
	void 			draw_segment				(stroke_context* str, dash_context* dc, bool isCurve);
	float 			draw_dashed_segment			(stroke_context *str, dash_context* dc, bool isCurve);
	bool 			build_vb_step				(stroke_context *str, bool isCurve);
	void 			poly_fill					(vec4f *bounds);
	void 			fill_non_zero				();
	void 			draw_full_screen_quad		(vec4f *scissor);
	void 			create_gradient_buff		();
	void 			create_vertices_buff		();
	void 			add_vertex					(VkgVertex &v);
	void 			add_vertexf					(float x, float y);
	void 			set_vertex					(uint32_t idx, VkgVertex v);
	void 			add_triangle_indices		(VKVG_IBO_INDEX_TYPE i0, VKVG_IBO_INDEX_TYPE i1, VKVG_IBO_INDEX_TYPE i2);
	void 			add_tri_indices_for_rect	(VKVG_IBO_INDEX_TYPE i);
	void 			vao_add_rectangle			(float x, float y, float width, float height);
	void 			bind_draw_pipeline			();
	void 			create_cmd_buff				();
	void 			check_vao_size				();
	void 			flush_cmd_buff				();
	void 			ensure_renderpass_is_started();
	void 			emit_draw_cmd_undrawn_vertices();
	void 			flush_cmd_until_vx_base		();
	bool 			wait_ctx_flush_end			();
	bool 			wait_and_submit_cmd			();
	void 			update_push_constants		();
	void 			update_cur_pattern			(VkgPattern pat);
	void 			set_mat_inv_and_vkCmdPush 	();
	void 			start_cmd_for_render_pass 	();
	void 			createDescriptorPool		();
	void 			init_descriptor_sets		();
	void 			update_descriptor_set		(VkeImage img, VkDescriptorSet ds);
	void 			update_gradient_desc_set	();
	void 			free_ctx_save				(vkg_context_save* sav);
	void 			release_context_resources	();
	void 			add_indice 					(VKVG_IBO_INDEX_TYPE i);
	void 			add_indice_for_fan 			(VKVG_IBO_INDEX_TYPE i);
	void			resize_vbo 					(size_t new_size);
	void			resize_ibo 					(size_t new_size);
	void add_vertexf_unchecked(float x, float y);
	void add_indice_for_strip(VKVG_IBO_INDEX_TYPE i, bool odd);
	void add_triangle_indices_unchecked (VKVG_IBO_INDEX_TYPE i0, VKVG_IBO_INDEX_TYPE i1, VKVG_IBO_INDEX_TYPE i2);
	void clear_attachment();
	void flush_vertices_caches_until_vertex_base ();
	void flush_vertices_caches();
	void end_render_pass ();
};

struct VkgText:mx {
	ptr(VkgText, mx, struct vkg_text_run);

	VkgText(VkgContext ctx, const char* text); /// was vkg_text_run_create
	VkgText(VkgContext ctx, const char* text, uint32_t length);

	void 	 show_text_run 		(VkgContext ctx, VkgText textRun);
	void     get_extents 		(VkgText textRun, vkg_text_extents* extents);
	uint32_t get_glyph_count 	();
	void     get_glyph_position (uint32_t index, vkg_glyph_info* pGlyphInfo);
};


} /// namespace ion
