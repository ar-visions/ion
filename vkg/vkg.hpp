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

#include <vke/vke.hh>
#include <math/math.hpp>

#include <vulkan/vulkan.h>
#include <math.h>
#include <stdbool.h>


#define FONT_PAGE_SIZE			1024
#define FONT_CACHE_INIT_LAYERS	1
#define FONT_FILE_NAME_MAX_SIZE 1024
#define FONT_NAME_MAX_SIZE		128

enum vkvg_direction {
	VKVG_HORIZONTAL	= 0,
	VKVG_VERTICAL	= 1
};

enum vkvg_format {
	VKVG_FORMAT_ARGB32,
	VKVG_FORMAT_RGB24,
	VKVG_FORMAT_A8,
	VKVG_FORMAT_A1
};
/**
 * @brief pattern border policy
 *
 * when painting a pattern on a surface, if the input bounds are smaller than the target bounds,
 * the extend defines how the pattern will be rendered outside its original bounds.
 */
enum vkvg_extend {
	VKVG_EXTEND_NONE,			/*!< nothing will be outputed outside the pattern original bounds */
	VKVG_EXTEND_REPEAT,			/*!< pattern will be repeated to fill all the target bounds */
	VKVG_EXTEND_REFLECT,		/*!< pattern will be repeated but mirrored on each repeat */
	VKVG_EXTEND_PAD				/*!< the last pixels making the borders of the pattern will be extended to the whole target */
};


enum vkvg_filter {
	VKVG_FILTER_FAST,
	VKVG_FILTER_GOOD,
	VKVG_FILTER_BEST,
	VKVG_FILTER_NEAREST,
	VKVG_FILTER_BILINEAR,
	VKVG_FILTER_GAUSSIAN,
};


enum vkvg_pattern_type {
	VKVG_PATTERN_TYPE_SOLID,			/*!< single color pattern */
	VKVG_PATTERN_TYPE_SURFACE,			/*!< vkvg surface pattern */
	VKVG_PATTERN_TYPE_LINEAR,			/*!< linear gradient pattern */
	VKVG_PATTERN_TYPE_RADIAL,			/*!< radial gradient pattern */
	VKVG_PATTERN_TYPE_MESH,				/*!< not implemented */
	VKVG_PATTERN_TYPE_RASTER_SOURCE,	/*!< not implemented */
};

struct vkvg_record {
	uint16_t	cmd;
	size_t		dataOffset;
};

struct vkvg_recording {
	vkvg_record*	commands;
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


enum vkvg_line_cap {
	VKVG_LINE_CAP_BUTT,		/*!< normal line endings, this is the default. */
	VKVG_LINE_CAP_ROUND,	/*!< rounded line caps */
	VKVG_LINE_CAP_SQUARE	/*!< extend the caps with squared terminations having border equal to current line width. */
};

enum vkvg_line_join {
	VKVG_LINE_JOIN_MITER,	/*!< normal joins with sharp angles, this is the default. */
	VKVG_LINE_JOIN_ROUND,	/*!< joins are rounded on the exterior border of the line. */
	VKVG_LINE_JOIN_BEVEL	/*!< beveled line joins. */
};

enum vkvg_fill_rule {
	VKVG_FILL_RULE_EVEN_ODD,	/*!< even-odd fill rule */
	VKVG_FILL_RULE_NON_ZERO		/*!< non zero fill rule */
};

/// VKVG objects passed to user are not managed with obi

struct vkvg_color {
	float r;					/*!< the red component */
	float g;					/*!< the green component */
	float b;					/*!< the blue component */
	float a;					/*!< the alpha component */
};

/**
  * @brief font metrics
  *
  * structure defining global font metrics for a particular font. It can be retrieve by calling @ref vkvg_font_extents
  * on a valid context.
  */
struct vkvg_font_extents {
	float ascent;			/*!< the distance that the font extends above the baseline. */
	float descent;			/*!< the distance that the font extends below the baseline.*/
	float height;			/*!< the recommended vertical distance between baselines. */
	float max_x_advance;	/*!< the maximum distance in the X direction that the origin is advanced for any glyph in the font.*/
	float max_y_advance;	/*!< the maximum distance in the Y direction that the origin is advanced for any glyph in the font. This will be zero for normal fonts used for horizontal writing.*/
};
/**
  * @brief text metrics
  *
  * structure defining metrics for a single or a string of glyphs. To measure text, call @ref vkvg_text_extents
  * on a valid context.
  */
struct vkvg_text_extents {
	float x_bearing;		/*!< the horizontal distance from the origin to the leftmost part of the glyphs as drawn. Positive if the glyphs lie entirely to the right of the origin. */
	float y_bearing;		/*!< the vertical distance from the origin to the topmost part of the glyphs as drawn. Positive only if the glyphs lie completely below the origin; will usually be negative.*/
	float width;			/*!< width of the glyphs as drawn*/
	float height;			/*!< height of the glyphs as drawn*/
	float x_advance;		/*!< distance to advance in the X direction after drawing these glyphs*/
	float y_advance;		/*!< distance to advance in the Y direction after drawing these glyphs. Will typically be zero except for vertical text layout as found in East-Asian languages.*/
};

/**
  * @brief glyphs position in a @ref VkvgText
  *
  * structure defining glyph position as computed for rendering a text run.
  * the codepoint field is for internal use only.
  */
struct vkvg_glyph_info {
	int32_t  x_advance;
	int32_t  y_advance;
	int32_t  x_offset;
	int32_t  y_offset;
	/* private */
	uint32_t codepoint;//should be named glyphIndex, but for harfbuzz compatibility...
};

#if VKVG_DBG_STATS
struct vkvg_debug_stats {
	uint32_t	sizePoints;		/**< maximum point array size					*/
	uint32_t	sizePathes;		/**< maximum path array size					*/
	uint32_t	sizeVertices;	/**< maximum size of host vertice cache			*/
	uint32_t	sizeIndices;	/**< maximum size of host index cache			*/
	uint32_t	sizeVBO;		/**< maximum size of vulkan vertex buffer		*/
	uint32_t	sizeIBO;		/**< maximum size of vulkan index buffer		*/
} ;

vkvg_debug_stats_t vkvg_device_get_stats (VkvgDevice dev);
vkvg_debug_stats_t vkvg_device_reset_stats (VkvgDevice dev);
#endif

#define VKVG_IDENTITY_MATRIX (vkvg_matrix_t){1,0,0,1,0,0}/*!< The identity matrix*/
struct vkvg_matrix {
	float xx; float yx;
	float xy; float yy;
	float x0; float y0;
};
/**
 * @brief Set matrix to identity
 *
 * Initialize members of the supplied #vkvg_matrix_t to make an identity matrix of it.
 * @param matrix a valid #vkvg_matrix_t pointer.
 */
vke_public
void vkvg_matrix_init_identity (vkvg_matrix_t *matrix);

vke_public
void vkvg_matrix_init (vkvg_matrix_t *matrix,
		   float xx, float yx,
		   float xy, float yy,
		   float x0, float y0);

vke_public
void vkvg_matrix_init_translate (vkvg_matrix_t *matrix, float tx, float ty);

vke_public
void vkvg_matrix_init_scale (vkvg_matrix_t *matrix, float sx, float sy);

vke_public
void vkvg_matrix_init_rotate (vkvg_matrix_t *matrix, float radians);

vke_public
void vkvg_matrix_translate (vkvg_matrix_t *matrix, float tx, float ty);

vke_public
void vkvg_matrix_scale (vkvg_matrix_t *matrix, float sx, float sy);

vke_public
void vkvg_matrix_rotate (vkvg_matrix_t *matrix, float radians);

vke_public
void vkvg_matrix_multiply (vkvg_matrix_t *result, const vkvg_matrix_t *a, const vkvg_matrix_t *b);

vke_public
void vkvg_matrix_transform_distance (const vkvg_matrix_t *matrix, float *dx, float *dy);

vke_public
void vkvg_matrix_transform_point (const vkvg_matrix_t *matrix, float *x, float *y);

vke_public
VkeStatus vkvg_matrix_invert (vkvg_matrix_t *matrix);
vke_public
void vkvg_matrix_get_scale (const vkvg_matrix_t *matrix, float *sx, float *sy);

vke_public
void vkvg_device_set_thread_aware (VkvgDevice dev, uint32_t thread_awayre);

vke_public
void vkvg_device_set_context_cache_size (VkvgDevice dev, uint32_t maxCount);
VkvgDevice vkvg_device_create(VkeDevice, VkSampleCountFlagBits, bool);

vke_public
VkvgDevice vkvg_device_create_from_vk (VkInstance inst, VkPhysicalDevice phy, VkDevice vkdev, uint32_t qFamIdx, uint32_t qIndex);

vke_public
VkvgDevice vkvg_device_create_from_vk_multisample (VkInstance inst, VkPhysicalDevice phy, VkDevice vkdev, uint32_t qFamIdx, uint32_t qIndex, VkSampleCountFlags samples, bool deferredResolve);

vke_public
VkeStatus vkvg_device_status (VkvgDevice dev);

vke_public
VkvgDevice vkvg_device_reference (VkvgDevice dev);

vke_public
void vkvg_device_set_dpy (VkvgDevice dev, int hdpy, int vdpy);

vke_public
void vkvg_device_get_dpy (VkvgDevice dev, int* hdpy, int* vdpy);

vke_public
VkeStatus vkvg_get_required_device_extensions(VkPhysicalDevice phy, const char** pExtensions, uint32_t* pExtCount);

vke_public
const void* vkvg_get_device_requirements (VkPhysicalDeviceFeatures* pEnabledFeatures);

vke_public
VkvgSurface vkvg_surface_create (VkvgDevice dev, uint32_t width, uint32_t height);

vke_public
VkvgSurface vkvg_surface_create_from_image (VkvgDevice dev, const char* filePath);

vke_public
VkvgSurface vkvg_surface_create_for_VkhImage (VkvgDevice dev, void* vkhImg);

vke_public
VkvgSurface vkvg_surface_create_from_bitmap (VkvgDevice dev, unsigned char* img, uint32_t width, uint32_t height);

vke_public
VkvgSurface vkvg_surface_reference (VkvgSurface surf);

vke_public
uint32_t vkvg_surface_get_reference_count (VkvgSurface surf);

vke_public
VkeStatus vkvg_surface_status (VkvgSurface surf);

vke_public
void vkvg_surface_clear (VkvgSurface surf);

vke_public
VkImage	vkvg_surface_get_vk_image (VkvgSurface surf);

vke_public
VkFormat vkvg_surface_get_vk_format (VkvgSurface surf);

vke_public
uint32_t vkvg_surface_get_width (VkvgSurface surf);

vke_public
uint32_t vkvg_surface_get_height (VkvgSurface surf);

vke_public
VkeStatus vkvg_surface_write_to_png (VkvgSurface surf, const char* path);

vke_public
VkeStatus vkvg_surface_write_to_memory (VkvgSurface surf, unsigned char* const bitmap);

vke_public
void vkvg_surface_resolve (VkvgSurface surf);

enum vkvg_operator {
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


VkvgContext vkvg_create(VkvgSurface surf);

vke_public
VkeStatus vkvg_status (VkvgContext ctx);

vke_public
const char* vkvg_status_to_string (VkeStatus status);

vke_public
VkvgContext vkvg_reference (VkvgContext ctx);

vke_public
void vkvg_flush (VkvgContext ctx);

vke_public
void vkvg_new_path (VkvgContext ctx);

vke_public
void vkvg_close_path (VkvgContext ctx);

vke_public
void vkvg_new_sub_path (VkvgContext ctx);

vke_public
void vkvg_path_extents (VkvgContext ctx, float *x1, float *y1, float *x2, float *y2);

vke_public
void vkvg_get_current_point (VkvgContext ctx, float* x, float* y);

vke_public
void vkvg_line_to (VkvgContext ctx, float x, float y);

vke_public
void vkvg_rel_line_to (VkvgContext ctx, float dx, float dy);

vke_public
void vkvg_move_to (VkvgContext ctx, float x, float y);

vke_public
void vkvg_rel_move_to (VkvgContext ctx, float x, float y);

vke_public
void vkvg_arc (VkvgContext ctx, float xc, float yc, float radius, float a1, float a2);

vke_public
void vkvg_arc_negative (VkvgContext ctx, float xc, float yc, float radius, float a1, float a2);

vke_public
void vkvg_curve_to (VkvgContext ctx, float x1, float y1, float x2, float y2, float x3, float y3);

vke_public
void vkvg_rel_curve_to (VkvgContext ctx, float x1, float y1, float x2, float y2, float x3, float y3);

vke_public
void vkvg_quadratic_to (VkvgContext ctx, float x1, float y1, float x2, float y2);

vke_public
void vkvg_rel_quadratic_to (VkvgContext ctx, float x1, float y1, float x2, float y2);

vke_public
VkeStatus vkvg_rectangle(VkvgContext ctx, float x, float y, float w, float h);

vke_public
VkeStatus vkvg_rounded_rectangle (VkvgContext ctx, float x, float y, float w, float h, float radius);

vke_public
void vkvg_rounded_rectangle2 (VkvgContext ctx, float x, float y, float w, float h, float rx, float ry);

vke_public
void vkvg_ellipse (VkvgContext ctx, float radiusX, float radiusY, float x, float y, float rotationAngle);

vke_public
void vkvg_elliptic_arc_to (VkvgContext ctx, float x, float y, bool large_arc_flag, bool sweep_flag, float rx, float ry, float phi);

vke_public
void vkvg_rel_elliptic_arc_to (VkvgContext ctx, float x, float y, bool large_arc_flag, bool sweep_flag, float rx, float ry, float phi);

vke_public
void vkvg_stroke (VkvgContext ctx);

vke_public
void vkvg_stroke_preserve (VkvgContext ctx);

vke_public
void vkvg_fill (VkvgContext ctx);

vke_public
void vkvg_fill_preserve (VkvgContext ctx);

vke_public
void vkvg_paint (VkvgContext ctx);

vke_public
void vkvg_clear (VkvgContext ctx);//use vkClearAttachment to speed up clearing surf

vke_public
void vkvg_reset_clip (VkvgContext ctx);

vke_public
void vkvg_clip (VkvgContext ctx);

vke_public
void vkvg_clip_preserve (VkvgContext ctx);

vke_public
void vkvg_set_opacity (VkvgContext ctx, float opacity);
vke_public

vke_public
void vkvg_set_source_color (VkvgContext ctx, uint32_t c);

vke_public
void vkvg_set_source_rgba (VkvgContext ctx, float r, float g, float b, float a);

vke_public
void vkvg_set_source_rgb (VkvgContext ctx, float r, float g, float b);

vke_public
void vkvg_set_line_width (VkvgContext ctx, float width);

vke_public
void vkvg_set_miter_limit (VkvgContext ctx, float limit);

vke_public
float vkvg_get_miter_limit (VkvgContext ctx);

vke_public
void vkvg_set_line_cap (VkvgContext ctx, vkvg_line_cap_t cap);

vke_public
void vkvg_set_line_join (VkvgContext ctx, vkvg_line_join_t join);

vke_public
void vkvg_set_source_surface (VkvgContext ctx, VkvgSurface surf, float x, float y);

vke_public
void vkvg_set_source (VkvgContext ctx, VkvgPattern pat);

vke_public
void vkvg_set_operator (VkvgContext ctx, vkvg_operator_t op);

vke_public
void vkvg_set_fill_rule (VkvgContext ctx, vkvg_fill_rule_t fr);

vke_public
void vkvg_set_dash (VkvgContext ctx, const float* dashes, uint32_t num_dashes, float offset);

vke_public
void vkvg_get_dash (VkvgContext ctx, const float *dashes, uint32_t* num_dashes, float* offset);

vke_public
float vkvg_get_line_width (VkvgContext ctx);

vke_public
vkvg_line_cap_t vkvg_get_line_cap (VkvgContext ctx);

vke_public
vkvg_line_join_t vkvg_get_line_join (VkvgContext ctx);

vke_public
vkvg_operator_t vkvg_get_operator (VkvgContext ctx);

vke_public
vkvg_fill_rule_t vkvg_get_fill_rule (VkvgContext ctx);

vke_public
VkvgPattern vkvg_get_source (VkvgContext ctx);

vke_public
VkvgSurface vkvg_get_target (VkvgContext ctx);

vke_public
bool vkvg_has_current_point (VkvgContext ctx);

vke_public
void vkvg_save (VkvgContext ctx);

vke_public
void vkvg_restore (VkvgContext ctx);

vke_public
void vkvg_translate (VkvgContext ctx, float dx, float dy);

vke_public
void vkvg_scale (VkvgContext ctx, float sx, float sy);

vke_public
void vkvg_rotate (VkvgContext ctx, float radians);

vke_public
void vkvg_transform (VkvgContext ctx, const vkvg_matrix_t* matrix);

vke_public
void vkvg_set_matrix (VkvgContext ctx, const vkvg_matrix_t* matrix);

vke_public
void vkvg_get_matrix (VkvgContext ctx, vkvg_matrix_t * const matrix);

vke_public
void vkvg_identity_matrix (VkvgContext ctx);

vke_public
void vkvg_select_font_face (VkvgContext ctx, const char* name);

vke_public
void vkvg_load_font_from_path (VkvgContext ctx, const char* path, const char *name);

vke_public
void vkvg_load_font_from_memory (VkvgContext ctx, unsigned char* fontBuffer, long fontBufferByteSize, const char* name);

vke_public
void vkvg_set_font_size (VkvgContext ctx, uint32_t size);

vke_public
void vkvg_show_text (VkvgContext ctx, const char* utf8);

vke_public
void vkvg_text_extents (VkvgContext ctx, const char* utf8, vkvg_text_extents_t* extents);

vke_public
void vkvg_font_extents (VkvgContext ctx, vkvg_font_extents_t* extents);

vke_public
VkvgText vkvg_text_run_create (VkvgContext ctx, const char* text);

vke_public
VkvgText vkvg_text_run_create_with_length (VkvgContext ctx, const char* text, uint32_t length);

vke_public
void vkvg_show_text_run (VkvgContext ctx, VkvgText textRun);

vke_public
void vkvg_text_run_get_extents (VkvgText textRun, vkvg_text_extents_t* extents);

vke_public
uint32_t vkvg_text_run_get_glyph_count (VkvgText textRun);

vke_public
void vkvg_text_run_get_glyph_position (VkvgText textRun,
									   uint32_t index,
									   vkvg_glyph_info_t* pGlyphInfo);

vke_public
VkeStatus vkvg_pattern_status (VkvgPattern pat);

vke_public
uint32_t vkvg_pattern_get_reference_count (VkvgPattern pat);

vke_public
VkvgPattern vkvg_pattern_create_for_surface (VkvgSurface surf);

vke_public
VkvgPattern vkvg_pattern_create_linear (float x0, float y0, float x1, float y1);

vke_public
VkeStatus vkvg_pattern_edit_linear(VkvgPattern pat, float x0, float y0, float x1, float y1);

vke_public
VkeStatus vkvg_pattern_get_linear_points(VkvgPattern pat, float* x0, float* y0, float* x1, float* y1);

vke_public
VkvgPattern vkvg_pattern_create_radial (float cx0, float cy0, float radius0,
										float cx1, float cy1, float radius1);

vke_public
VkeStatus vkvg_pattern_edit_radial(VkvgPattern pat,
								float cx0, float cy0, float radius0,
								float cx1, float cy1, float radius1);

vke_public
VkeStatus vkvg_pattern_get_color_stop_count (VkvgPattern pat, uint32_t* count);

vke_public
VkeStatus vkvg_pattern_get_color_stop_rgba (VkvgPattern pat, uint32_t index,
												float* offset, float* r, float* g, float* b, float* a);

vke_public
VkeStatus vkvg_pattern_add_color_stop(VkvgPattern pat, float offset, float r, float g, float b, float a);

vke_public
void vkvg_pattern_set_extend (VkvgPattern pat, vkvg_extend_t extend);

vke_public
void vkvg_pattern_set_filter (VkvgPattern pat, vkvg_filter_t filter);

vke_public
vkvg_extend_t vkvg_pattern_get_extend (VkvgPattern pat);

vkvg_filter_t vkvg_pattern_get_filter (VkvgPattern pat);

vkvg_pattern_type_t vkvg_pattern_get_type (VkvgPattern pat);
void vkvg_pattern_set_matrix (VkvgPattern pat, const vkvg_matrix_t* matrix);
void vkvg_pattern_get_matrix (VkvgPattern pat, vkvg_matrix_t* matrix);

/** @}*/

/********* EXPERIMENTAL **************/
vke_public
void vkvg_set_source_color_name (VkvgContext ctx, const char* color);

#ifdef VKVG_RECORDING
typedef struct _vkvg_recording_t* VkvgRecording;

void			 vkvg_start_recording		(VkvgContext ctx);
VkvgRecording vkvg_stop_recording		(VkvgContext ctx);
void			 vkvg_replay				(VkvgContext ctx, VkvgRecording rec);
void			 vkvg_replay_command		(VkvgContext ctx, VkvgRecording rec, uint32_t cmdIndex);
void			 vkvg_recording_get_command (VkvgRecording rec, uint32_t cmdIndex, uint32_t* cmd, void** dataOffset);
uint32_t		 vkvg_recording_get_count   (VkvgRecording rec);
void*		 vkvg_recording_get_data    (VkvgRecording rec);


struct Vertex {
	vec2f    	pos;
	uint32_t	color;
	vec3f		uv;
};

struct push_constants {
	vec4f			source;
	vec2f			size;
	uint32_t		fsq_patternType;
	float			opacity;
	vkvg_matrix_t	mat;
	vkvg_matrix_t	matInv;
};

// Precompute everything necessary to measure and draw one line of text, usefull to draw the same text multiple times.
struct vkvg_text_run {
	struct _vkvg_font_identity_t*	fontId;		/* vkvg font structure pointer */
	struct _vkvg_font_t*			font;		/* vkvg font structure pointer */

	VkvgDevice				dev;		/* vkvg device associated with this text run */
	vkvg_text_extents_t		extents;	/* store computed text extends */
	
	const char*				text;		/* utf8 char array of text*/
	unsigned int			glyph_count;/* Total glyph count */

#ifdef VKVG_USE_HARFBUZZ
	struct hb_buffer_t*			hbBuf;		/* HarfBuzz buffer of text */
	struct hb_glyph_position_t*	glyphs;		/* HarfBuzz computed glyph positions array */
#else
	vkvg_glyph_info_t*		glyphs;		/* computed glyph positions array */
#endif
};

struct _vkvg_font_identity_t;
struct _vkvg_context_save;

struct vkvg_pattern {
	VkeStatus		status;
	vkvg_pattern_type_t type;
	vkvg_extend_t		extend;
	vkvg_filter_t		filter;
	vkvg_matrix_t		matrix;
	bool				hasMatrix;
	void*				data;
};

struct vkvg_gradient {
	vkvg_color_t	colors[16];
	vec4d			stops[16];
	vec4d			cp[2];
	uint32_t		count;
};

enum vkvg_clip_state {
	vkvg_clip_state_none		= 0x00,
	vkvg_clip_state_clear		= 0x01,
	vkvg_clip_state_clip		= 0x02,
	vkvg_clip_state_clip_saved	= 0x06,
};

struct _font_cache;

/// this approach is better because anyone who wants to read 
/// includes vulkan and reads. they can also use these things 
/// as handles if they need

// Vke can typedef the VkeName> for VkeName
// 

struct vkvg_device {
	/// data was cross over from vke but i believe its better to reference it.
	VkeDevice               vke;

	VkeImageTiling>		supportedTiling;		/**< Supported image tiling for surface, 0xFF=no support */
	VkeFormat>			stencilFormat;			/**< Supported vulkan image format for stencil */
	VkeImageAspectFlags>	stencilAspectFlag;		/**< stencil only or depth stencil, could be solved by VK_KHR_separate_depth_stencil_layouts*/
	VkeFormat>			pngStagFormat;			/**< Supported vulkan image format png write staging img */
	VkeImageTiling>		pngStagTiling;			/**< tiling for the blit operation */

	VkeQueue				gQueue;					/**< Vulkan Queue with Graphic flag */

	VkeRenderPass>		renderPass;				/**< Vulkan render pass, common for all surfaces */
	VkeRenderPass>		renderPass_ClearStencil;/**< Vulkan render pass for first draw with context, stencil has to be cleared */
	VkeRenderPass>		renderPass_ClearAll;	/**< Vulkan render pass for new surface, clear all attacments*/

	uint32_t				references;				/**< Reference count, prevent destroying device if still in use */
	VkeCommandPool>			cmdPool;				/**< Global command pool for processing on surfaces without context */
	VkeCommandBuffer>			cmd;					/**< Global command buffer */
	VkeFence>					fence;					/**< this fence is kept signaled when idle, wait and reset are called before each recording. */

	VkePipeline>				pipe_OVER;				/**< default operator */
	VkePipeline>				pipe_SUB;
	VkePipeline>				pipe_CLEAR;				/**< clear operator */

	VkePipeline>				pipelinePolyFill;		/**< even-odd polygon filling first step */
	VkePipeline>				pipelineClipping;		/**< draw on stencil to update clipping regions */

	VkePipelineCache>			pipelineCache;			/**< speed up startup by caching configured pipelines on disk */
	VkePipelineLayout>		pipelineLayout;			/**< layout common to all pipelines */
	VkeDescriptorSetLayout>	dslFont;				/**< font cache descriptors layout */
	VkeDescriptorSetLayout>	dslSrc;					/**< context source surface descriptors layout */
	VkeDescriptorSetLayout>	dslGrad;				/**< context gradient descriptors layout */

	VkeImage				emptyImg;				/**< prevent unbound descriptor to trigger Validation error 61 */
	VkeSampleCountFlags>  samples;				/**< samples count common to all surfaces */
	bool					deferredResolve;		/**< if true, resolve only on context destruction and set as source */
	VkeStatus			status;					/**< Current status of device, affected by last operation */

	struct _font_cache_t*   fontCache;				/**< Store everything relative to common font caching system */

	VkvgContext				lastCtx;				/**< last element of double linked list of context, used to trigger font caching system update on all contexts*/

	int32_t					cachedContextMaxCount;	/**< Maximum context cache element count.*/
	int32_t					cachedContextCount;		/**< Current context cache element count.*/
	struct _cached_ctx_t*   cachedContextLast;		/**< Last element of single linked list of saved context for fast reuse.*/

#ifdef VKVG_WIRED_DEBUG
	VkPipeline				pipelineWired;
	VkPipeline				pipelineLineList;
#endif
#if VKVG_DBG_STATS
	vkvg_debug_stats_t		debug_stats;			/**< debug statistics on memory usage and vulkan ressources */
#endif
}; /// keeping with this pattern, no *_t (replacing others)

using VkvgDevice = sp<vkvg_device>;

struct vkvg_context {
	VkeStatus		status;
	uint32_t			references;		//reference count

	VkvgDevice			dev;
	VkvgSurface			pSurf;			//surface bound to context, set on creation of ctx
#ifdef VKVG_ENABLE_VK_TIMELINE_SEMAPHORE
	uint64_t			timelineStep;	//context cmd last submission timeline id.
#else
	VkFence				flushFence;		//context fence
#endif
	//VkDescriptorImageInfo sourceDescriptor;	//Store view/sampler in context

	VkCommandPool		cmdPool;		//local pools ensure thread safety
	VkCommandBuffer		cmdBuffers[2];	//double cmd buff for context operations
	VkCommandBuffer		cmd;			//current recording buffer
	bool				cmdStarted;		//prevent flushing empty renderpass
	bool				pushCstDirty;	//prevent pushing to gpu if not requested
	VkDescriptorPool	descriptorPool;	//one pool per thread
	VkDescriptorSet		dsFont;			//fonts glyphs texture atlas descriptor (local for thread safety)
	VkDescriptorSet		dsSrc;			//source ds
	VkDescriptorSet		dsGrad;			//gradient uniform buffer

	VkeImage			fontCacheImg;	//current font cache, may not be the last one, updated only if new glyphs are
										//uploaded by the current context
	VkRect2D			bounds;
	uint32_t			curColor;

#if VKVG_FILL_NZ_GLUTESS
	void (*vertex_cb)(VKVG_IBO_INDEX_TYPE, VkvgContext);//tesselator vertex callback
	VKVG_IBO_INDEX_TYPE tesselator_fan_start;
	uint32_t			tesselator_idx_counter;
	
#endif

#if VKVG_RECORDING
	vkvg_recording*		recording;
#endif

	VkeBuffer			uboGrad;		//uniform buff obj holdings gradient infos

	//vk buffers, holds data until flush
	VkeBuffer			indices;		//index buffer with persistent map memory
	uint32_t			sizeIBO;		//size of vk ibo
	uint32_t			sizeIndices;	//reserved size
	uint32_t			indCount;		//current indice count

	uint32_t			curIndStart;	//last index recorded in cmd buff
	VKVG_IBO_INDEX_TYPE	curVertOffset;	//vertex offset in draw indexed command

	VkeBuffer			vertices;		//vertex buffer with persistent mapped memory
	uint32_t			sizeVBO;		//size of vk vbo size
	uint32_t			sizeVertices;	//reserved size
	uint32_t			vertCount;		//effective vertices count

	Vertex*				vertexCache;
	VKVG_IBO_INDEX_TYPE* indexCache;

	//pathes, exists until stroke of fill
	vec2f*				points;			//points array
	uint32_t			sizePoints;		//reserved size
	uint32_t			pointCount;		//effective points count

	//pathes array is a list of point count per segment
	uint32_t			pathPtr;		//pointer in the path array
	uint32_t*			pathes;
	uint32_t			sizePathes;

	uint32_t			segmentPtr;		//current segment count in current path having curves
	uint32_t			subpathCount;	//store count of subpath, not straight forward to retrieve from segmented path array
	bool				simpleConvex;	//true if path is single rect or concave closed curve.

	float				lineWidth;
	float				miterLimit;
	uint32_t			dashCount;		//value count in dash array, 0 if dash not set.
	float				dashOffset;		//an offset for dash
	float*				dashes;			//an array of alternate lengths of on and off stroke.

	vkvg_operator_t		curOperator;
	vkvg_line_cap_t		lineCap;
	vkvg_line_join_t	lineJoin;
	vkvg_fill_rule_t	curFillRule;

	long				selectedCharSize; /* Font size*/
	char				selectedFontName[FONT_NAME_MAX_SIZE];
	//_vkvg_font_t		  selectedFont;		//hold current face and size before cache addition
	struct _vkvg_font_identity_t* currentFont;		//font pointing to cached fonts identity
	struct _vkvg_font_t*		  currentFontSize;	//font structure by size ready for lookup
	vkvg_direction_t	textDirection;

	push_constants		pushConsts;
	VkvgPattern			pattern;

	struct _vkvg_context_save* pSavedCtxs;		//last ctx saved ptr
	uint8_t				curSavBit;			//current stencil bit used to save context, 6 bits used by stencil for save/restore
	VkeImage*			savedStencils;		//additional image for saving contexes once more than 6 save/restore are reached
	vkvg_clip_state_t	curClipState;		//current clipping status relative to the previous saved one or clear state if none.

	VkClearRect			clearRect;
	VkRenderPassBeginInfo renderPassBeginInfo;
};


struct vkvg_surface {
	VkeStatus	status;					/**< Current status of surface, affected by last operation */
	VkvgDevice		dev;
	uint32_t		width;
	uint32_t		height;
	VkFormat		format;
	VkFramebuffer	fb;
	VkeImage		img;
	VkeImage		imgMS;
	VkeImage		stencil;
	VkCommandPool	cmdPool;				//local pools ensure thread safety
	VkCommandBuffer cmd;					//surface local command buffer.
	bool			newSurf;
#ifdef VKVG_ENABLE_VK_TIMELINE_SEMAPHORE
	VkSemaphore		timeline;				/**< Timeline semaphore */
	uint64_t		timelineStep;
#else
	VkFence			flushFence;				//unsignaled idle.
#endif
};

using VgTextRun     = sp<vkg_text_run>;
using VgContext     = sp<vkg_context>;
using VgSurface     = sp<vkg_surface>;
using VgDevice      = sp<vkg_device>;
using VgPattern     = sp<vkg_pattern>;
using VgGradient    = sp<vkg_gradient>;
using VgRecording   = sp<vkg_recording>;
using VgRecord      = sp<vkg_record>;

/*************************************/
#endif
