#pragma once

 //disable warning on iostream functions on windows
#define _CRT_SECURE_NO_WARNINGS

#include <assert.h>
#include <float.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>  // needed before stdarg.h on Windows
#include <stdarg.h>
#include <string.h>

#define _USE_MATH_DEFINES
#include <math.h>

#include <vk_mem_alloc.h>
//#include <io/io.h>

//#include <tinycthread.h>
#include <vkg/cross_os.hh>

#include <vke/vke.hh>
#include <vkg/vkg.hh>

#ifdef VKVG_USE_FREETYPE
	#include <ft2build.h>
	#include FT_FREETYPE_H
	#if defined(VKVG_LCD_FONT_FILTER) && defined(FT_CONFIG_OPTION_SUBPIXEL_RENDERING)
		#include <freetype/ftlcdfil.h>
	#endif
	#define FT_CHECK_RESULT(f)																				\
	{																										\
		FT_Error res = (f);																					\
		if (res != 0)																				\
		{																									\
			fprintf(stderr,"Fatal : FreeType error is %d in %s at line %d\n", res,	__FILE__, __LINE__); \
			assert(res == 0);																		\
		}																									\
	}
#else
	#include "stb_truetype.h"
#endif
#ifdef VKVG_USE_HARFBUZZ
	#include <harfbuzz/hb.h>
	#include <harfbuzz/hb-ft.h>
#else
#endif

#ifdef VKVG_USE_FONTCONFIG
	#include <fontconfig/fontconfig.h>
#endif

#include <vkg/internal.hh>

namespace ion {

#ifndef M_PIF
	#define M_PIF				3.14159265358979323846f /* float pi */
	#define M_PIF_2				1.57079632679489661923f
	#define M_2_PIF				0.63661977236758134308f	 // 2/pi
#endif

/*#ifndef M_2_PI
	#define M_2_PI		0.63661977236758134308	// 2/pi
#endif*/

#define PATH_CLOSED_BIT		0x80000000				/* most significant bit of path elmts is closed/open path state */
#define PATH_HAS_CURVES_BIT 0x40000000				/* 2rd most significant bit of path elmts is curved status
													 * for main path, this indicate that curve datas are present.
													 * For segments, this indicate that the segment is curved or not */
#define PATH_IS_CONVEX_BIT	0x20000000				/* simple rectangle or circle. */
#define PATH_ELT_MASK		0x1FFFFFFF				/* Bit mask for fetching path element value */

#define ROUNDF(f, c) (((float)((int)((f) * (c))) / (c)))
#define ROUND_DOWN(v,p) (floorf(v * p) / p)
#define EQUF(a, b) (fabsf(a-(b))<=FLT_EPSILON)

#ifndef MAX
	#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
	#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

//width of the stencil buffer will determine the number of context saving/restore layers
//the two first bits of the stencil are the FILL and the CLIP bits, all other bits are
//used to store clipping bit on context saving. 8 bit stencil will allow 6 save/restore layer
#define FB_COLOR_FORMAT VK_FORMAT_B8G8R8A8_UNORM
#define VKVG_SURFACE_IMGS_REQUIREMENTS (VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT|VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|\
	VK_FORMAT_FEATURE_TRANSFER_DST_BIT|VK_FORMAT_FEATURE_TRANSFER_SRC_BIT|VK_FORMAT_FEATURE_BLIT_SRC_BIT)
#define VKVG_PNG_WRITE_IMG_REQUIREMENTS (VK_FORMAT_FEATURE_TRANSFER_SRC_BIT|VK_FORMAT_FEATURE_TRANSFER_DST_BIT|VK_FORMAT_FEATURE_BLIT_DST_BIT)
//30 seconds fence timeout
#define VKVG_FENCE_TIMEOUT 30000000000
//#define VKVG_FENCE_TIMEOUT 10000


//texture coordinates of one character in font cache array texture.
typedef struct {
	vec4f		bounds;				/* normalized float bounds of character bitmap in font cache texture. */
	vec2f		bmpDiff;			/* Difference in pixel between char bitmap top left corner and char glyph*/
	uint8_t		pageIdx;			/* Page index in font cache texture array */
#ifdef VKVG_USE_FREETYPE
	FT_Vector	advance;			/* horizontal or vertical advance */
#else
	vec2f		advance;
#endif
}_char_ref;

// Current location in font cache texture array for new character addition. Each font holds such structure to locate
// where to upload new chars.
typedef struct {
	uint8_t		pageIdx;			/* Current page number in font cache */
	int			penX;				/* Current X in cache for next char addition */
	int			penY;				/* Current Y in cache for next char addition */
	int			height;				/* Height of current line pointed by this structure */
}_tex_ref_t;

// Loaded font structure, one per size, holds informations for glyphes upload in cache and the lookup table of characters.
typedef struct {
#ifdef VKVG_USE_FREETYPE
	FT_F26Dot6		charSize;		/* Font size*/
	FT_Face			face;			/* FreeType face*/
#else
	uint32_t		charSize;		/* Font size in pixel */
	float			scale;			/* scale factor for the given size */
	int				ascent;			/* unscalled stb font metrics */
	int				descent;
	int				lineGap;
#endif

#ifdef VKVG_USE_HARFBUZZ
	hb_font_t*		hb_font;		/* HarfBuzz font instance*/
#endif
	_char_ref**		charLookup;		/* Lookup table of characteres in cache, if not found, upload is queued*/

	_tex_ref_t		curLine;		/* tex coord where to add new char bmp's */
}_vkg_font_t;

/* Font identification structure */
struct vkg_font_identity {
	char**				names;		/* Resolved Input names to this font by fontConfig or custom name set by @ref vkg_load_from_path*/
	uint32_t			namesCount;	/* Count of resolved names by fontConfig */
	unsigned char*		fontBuffer;	/* stb_truetype in memory buffer */
	long				fontBufSize;/* */
	char*				fontFile;	/* Font file full path*/
#ifndef VKVG_USE_FREETYPE
	stbtt_fontinfo		stbInfo;	/* stb_truetype structure */
	int					ascent;		/* unscalled stb font metrics */
	int					descent;
	int					lineGap;
#endif
	uint32_t			sizeCount;	/* available font size loaded */
	_vkg_font_t*		sizes;		/* loaded font size array */
};

// Font cache global structure, entry point for all font related operations.
struct font_cache_t {
#ifdef VKVG_USE_FREETYPE
	FT_Library		library;		/* FreeType library*/
#else
#endif
#ifdef VKVG_USE_FONTCONFIG
	FcConfig*		config;			/* Font config, used to find font files by font names*/
#endif

	int				stagingX;		/* x pen in host buffer */
	uint8_t*		hostBuff;		/* host memory where bitmaps are first loaded */

	VkCommandBuffer cmd;			/* vulkan command buffer for font textures upload */
	VkeBuffer		buff;			/* stagin buffer */
	VkeImage		texture;		/* 2d array texture used by contexts to draw characteres */
	VkFormat		texFormat;		/* Format of the fonts texture array */
	uint8_t			texPixelSize;	/* Size in byte of a single pixel in a font texture */
	uint8_t			texLength;		/* layer count of 2d array texture, starts with FONT_CACHE_INIT_LAYERS count and increased when needed */
	int*			pensY;			/* array of current y pen positions for each texture in cache 2d array */
	VkFence			uploadFence;	/* Signaled when upload is finished */

	vkg_font_identity*	fonts;	/* Loaded fonts structure array */
	int32_t			fontsCount;		/* Loaded fonts array count*/
} _font_cache;

struct _vkg_device;
struct _vkg_context;
void _fonts_cache_create				(struct _vkg_device *dev);
void _font_cache_free				    (struct _vkg_device *dev);
vkg_font_identity *_font_cache_add_font_identity (struct _vkg_context *ctx, const char* fontFile, const char *name);
bool _font_cache_load_font_file_in_memory (vkg_font_identity* fontId);
void _font_cache_show_text				(struct _vkg_context *ctx, const char* text);
void _font_cache_text_extents			(struct _vkg_context *ctx, const char* text, int length, vkg_text_extents *extents);
void _font_cache_font_extents			(struct _vkg_context *ctx, vkg_font_extents* extents);
VkgText _font_cache_create_text_run		(struct _vkg_context *ctx, const char* text, int length, VkgText textRun);
void _font_cache_destroy_text_run		(VkgText textRun);
void _font_cache_show_text_run			(struct _vkg_context *ctx, VkgText tr);
void _font_cache_update_context_descset (struct _vkg_context *ctx);

/*

typedef struct {
	VkeStatus status;
} _vkg_no_mem_struct;

*/

static VkeStatus _no_mem_status = VKE_STATUS_NO_MEMORY;

#define STENCIL_FILL_BIT	0x1
#define STENCIL_CLIP_BIT	0x2
#define STENCIL_ALL_BIT		0x3

#define VKVG_MAX_CACHED_CONTEXT_COUNT 2

extern PFN_vkCmdBindPipeline			CmdBindPipeline;
extern PFN_vkCmdBindDescriptorSets		CmdBindDescriptorSets;
extern PFN_vkCmdBindIndexBuffer			CmdBindIndexBuffer;
extern PFN_vkCmdBindVertexBuffers		CmdBindVertexBuffers;

extern PFN_vkCmdDrawIndexed				CmdDrawIndexed;
extern PFN_vkCmdDraw					CmdDraw;

extern PFN_vkCmdSetStencilCompareMask	CmdSetStencilCompareMask;
extern PFN_vkCmdSetStencilReference		CmdSetStencilReference;
extern PFN_vkCmdSetStencilWriteMask		CmdSetStencilWriteMask;
extern PFN_vkCmdBeginRenderPass			CmdBeginRenderPass;
extern PFN_vkCmdEndRenderPass			CmdEndRenderPass;
extern PFN_vkCmdSetViewport				CmdSetViewport;
extern PFN_vkCmdSetScissor				CmdSetScissor;

extern PFN_vkCmdPushConstants			CmdPushConstants;
extern PFN_vkWaitForFences				WaitForFences;
extern PFN_vkResetFences				ResetFences;
extern PFN_vkResetCommandBuffer			ResetCommandBuffer;

struct cached_ctx_t {
	std::thread 	   *thread;
	//thrd_t				thread;
	VkgContext			ctx;
	struct _cached_ctx*	pNext;
} _cached_ctx;

bool _device_init_function_pointers		(VkgDevice dev);
void _device_create_empty_texture		(VkgDevice dev, VkFormat format, VkImageTiling tiling);
void _device_get_best_image_tiling		(VkgDevice dev, VkFormat format, VkImageTiling* pTiling);
void _device_check_best_image_tiling	(VkgDevice dev, VkFormat format);
void _device_create_pipeline_cache		(VkgDevice dev);
VkRenderPass _device_createRenderPassMS	(VkgDevice dev, VkAttachmentLoadOp loadOp, VkAttachmentLoadOp stencilLoadOp);
VkRenderPass _device_createRenderPassNoResolve(VkgDevice dev, VkAttachmentLoadOp loadOp, VkAttachmentLoadOp stencilLoadOp);
void _device_setupPipelines				(VkgDevice dev);
void _device_createDescriptorSetLayout 	(VkgDevice dev);
void _device_wait_idle					(VkgDevice dev);
void _device_wait_and_reset_device_fence(VkgDevice dev);
void _device_submit_cmd					(VkgDevice dev, VkCommandBuffer* cmd, VkFence fence);

bool _device_try_get_cached_context		(VkgDevice dev, VkgContext* pCtx);
void _device_store_context				(VkgContext ctx);

#define FULLSCREEN_BIT	0x10000000
#define SRCTYPE_MASK	0x000000FF

#define CreateRgba(r, g, b, a) ((a << 24) | (r << 16) | (g << 8) | b)
#ifdef VKVG_PREMULT_ALPHA
	#define CreateRgbaf(r, g, b, a) (((int)(a * 255.0f) << 24) | ((int)(b * a * 255.0f) << 16) | ((int)(g * a * 255.0f) << 8) | (int)(r * a * 255.0f))
#else
	#define CreateRgbaf(r, g, b, a) (((int)(a * 255.0f) << 24) | ((int)(b * 255.0f) << 16) | ((int)(g * 255.0f) << 8) | (int)(r * 255.0f))
#endif

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
	vkg_matrix	mat;
	vkg_matrix	matInv;
};

struct vkg_context_save {
	struct vkg_context_save* pNext;
	float					lineWidth;
	float					miterLimit;
	uint32_t				dashCount;		//value count in dash array, 0 if dash not set.
	float					dashOffset;		//an offset for dash
	float*					dashes;			//an array of alternate lengths of on and off stroke.
	vkg_operator			curOperator;
	vkg_line_cap			lineCap;
	vkg_line_join			lineJoint;
	vkg_fill_rule			curFillRule;
	long					selectedCharSize; /* Font size*/
	char					selectedFontName[FONT_NAME_MAX_SIZE];
	vkg_font_identity		selectedFont;	   //hold current face and size before cache addition
	vkg_font_identity*		currentFont;	   //font ready for lookup
	vkg_direction			textDirection;
	push_constants			pushConsts;
	uint32_t				curColor;
	VkgPattern				pattern;
	vkg_clip_state			clippingState;
};

struct ear_clip_point {
	vec2f					pos;
	VKVG_IBO_INDEX_TYPE		idx;
	struct _ear_clip_point*	next;
};

struct dash_context {
	bool					dashOn;
	uint32_t				curDash;		//current dash index
	float					curDashOffset;	//cur dash offset between defined path point and last dash segment(on/off) start
	float					totDashLength;	//total length of dashes
	vec2f					normal;
};

struct stroke_context {
	uint32_t	iL;
	uint32_t	iR;
	uint32_t	cp;//current point
	VKVG_IBO_INDEX_TYPE 	firstIdx;//save first point idx for closed path
	float		hw;				//stroke half width, computed once.
	float		lhMax;			//miter limit * line width
	float		arcStep;		//cached arcStep, prevent compute multiple times for same stroke, 0 if not yet computed
};

void _check_vertex_cache_size	(VkgContext ctx);
void _ensure_vertex_cache_size	(VkgContext ctx, uint32_t addedVerticesCount);
void _resize_vertex_cache		(VkgContext ctx, uint32_t newSize);

void _check_index_cache_size	(VkgContext ctx);
void _ensure_index_cache_size	(VkgContext ctx, uint32_t addedIndicesCount);
void _resize_index_cache		(VkgContext ctx, uint32_t newSize);

bool _check_pathes_array		(VkgContext ctx);

bool _current_path_is_empty		(VkgContext ctx);
void _finish_path				(VkgContext ctx);
void _clear_path				(VkgContext ctx);
void _remove_last_point			(VkgContext ctx);
bool _path_is_closed			(VkgContext ctx, uint32_t ptrPath);
void _set_curve_start			(VkgContext ctx);
void _set_curve_end				(VkgContext ctx);
bool path_has_curves			(VkgContext ctx, uint32_t ptrPath);

float _normalizeAngle			(float a);
float _get_arc_step				(VkgContext ctx, float radius);

vec2f *_get_current_position    	(VkgContext ctx);
void _add_point						(VkgContext ctx, float x, float y);

void _resetMinMax					(VkgContext ctx);
void _vkg_path_extents				(VkgContext ctx, bool transformed, float *x1, float *y1, float *x2, float *y2);
void _draw_stoke_cap				(VkgContext ctx, stroke_context* str, vec2f p0, vec2f n, bool isStart);
void _draw_segment					(VkgContext ctx, stroke_context* str, dash_context_t* dc, bool isCurve);
float draw_dashed_segment			(VkgContext ctx, stroke_context *str, dash_context_t* dc, bool isCurve);
bool build_vb_step					(VkgContext ctx, stroke_context *str, bool isCurve);

void _poly_fill						(VkgContext ctx, vec4f *bounds);
void _fill_non_zero					(VkgContext ctx);
void _draw_full_screen_quad			(VkgContext ctx, vec4f *scissor);

void _create_gradient_buff			(VkgContext ctx);
void _create_vertices_buff			(VkgContext ctx);
void add_vertex						(VkgContext ctx, Vertex v);
void add_vertexf					(VkgContext ctx, float x, float y);
void _set_vertex					(VkgContext ctx, uint32_t idx, Vertex v);
void add_triangle_indices			(VkgContext ctx, VKVG_IBO_INDEX_TYPE i0, VKVG_IBO_INDEX_TYPE i1, VKVG_IBO_INDEX_TYPE i2);
void add_tri_indices_for_rect		(VkgContext ctx, VKVG_IBO_INDEX_TYPE i);

void _vao_add_rectangle				(VkgContext ctx, float x, float y, float width, float height);
void _bind_draw_pipeline			(VkgContext ctx);
void _create_cmd_buff				(VkgContext ctx);
void _check_vao_size				(VkgContext ctx);
void _flush_cmd_buff				(VkgContext ctx);
void _ensure_renderpass_is_started	(VkgContext ctx);
void _emit_draw_cmd_undrawn_vertices(VkgContext ctx);
void _flush_cmd_until_vx_base		(VkgContext ctx);
bool _wait_ctx_flush_end			(VkgContext ctx);
bool _wait_and_submit_cmd			(VkgContext ctx);
void _update_push_constants			(VkgContext ctx);
void _update_cur_pattern			(VkgContext ctx, VkgPattern pat);
void _set_mat_inv_and_vkCmdPush 	(VkgContext ctx);
void _start_cmd_for_render_pass 	(VkgContext ctx);
void _createDescriptorPool			(VkgContext ctx);
void _init_descriptor_sets			(VkgContext ctx);
void _update_descriptor_set			(VkgContext ctx, VkeImage img, VkDescriptorSet ds);
void _update_gradient_desc_set		(VkgContext ctx);
void _free_ctx_save					(vkg_context_save* sav);
void _release_context_ressources	(VkgContext ctx);

static inline float vec2f_zcross (vec2f v1, vec2f v2) {
	return v1[X] * v2[Y] - v1[Y] * v2[X];
}

static inline float ecp_zcross (ear_clip_point* p0, ear_clip_point* p1, ear_clip_point* p2) {
	vec2f d_10;
	vec2f d_20;
	vec2f_sub(d_10, p1->pos, p0->pos);
	vec2f_sub(d_20, p2->pos, p0->pos);
	return vec2f_zcross (d_10, d_20);
}

void _recursive_bezier(VkgContext ctx, float distanceTolerance,
					   float x1, float y1, float x2, float y2,
					   float x3, float y3, float x4, float y4,
					   unsigned level);

void _bezier (VkgContext ctx,
			  float x1, float y1, float x2, float y2,
			  float x3, float y3, float x4, float y4);

void _line_to		(VkgContext ctx, float x, float y);

void _elliptic_arc	(VkgContext ctx, float x1, float y1, float x2, float y2,
					 bool largeArc, bool counterClockWise, float _rx, float _ry, float phi);

void _select_font_face			(VkgContext ctx, const char* name);




#define VKVG_CMD_SAVE				0x0001
#define VKVG_CMD_RESTORE			0x0002

#define VKVG_CMD_PATH_COMMANDS		0x0100
#define VKVG_CMD_DRAW_COMMANDS		0x0200
#define VKVG_CMD_RELATIVE_COMMANDS	(0x0400|VKVG_CMD_PATH_COMMANDS)
#define VKVG_CMD_PATHPROPS_COMMANDS	(0x1000|VKVG_CMD_PATH_COMMANDS)
#define VKVG_CMD_PRESERVE_COMMANDS	(0x0400|VKVG_CMD_DRAW_COMMANDS)
#define VKVG_CMD_PATTERN_COMMANDS	0x0800
#define VKVG_CMD_TRANSFORM_COMMANDS	0x2000
#define VKVG_CMD_TEXT_COMMANDS		0x4000

#define VKVG_CMD_NEW_PATH			(0x0001|VKVG_CMD_PATH_COMMANDS)
#define VKVG_CMD_NEW_SUB_PATH		(0x0002|VKVG_CMD_PATH_COMMANDS)
#define VKVG_CMD_CLOSE_PATH			(0x0003|VKVG_CMD_PATH_COMMANDS)
#define VKVG_CMD_MOVE_TO			(0x0004|VKVG_CMD_PATH_COMMANDS)
#define VKVG_CMD_LINE_TO			(0x0005|VKVG_CMD_PATH_COMMANDS)
#define VKVG_CMD_RECTANGLE			(0x0006|VKVG_CMD_PATH_COMMANDS)
#define VKVG_CMD_ARC				(0x0007|VKVG_CMD_PATH_COMMANDS)
#define VKVG_CMD_ARC_NEG			(0x0008|VKVG_CMD_PATH_COMMANDS)
//#define VKVG_CMD_ELLIPSE			(0x0009|VKVG_CMD_PATH_COMMANDS)
#define VKVG_CMD_CURVE_TO			(0x000A|VKVG_CMD_PATH_COMMANDS)
#define VKVG_CMD_QUADRATIC_TO		(0x000B|VKVG_CMD_PATH_COMMANDS)
#define VKVG_CMD_ELLIPTICAL_ARC_TO	(0x000C|VKVG_CMD_PATH_COMMANDS)

#define VKVG_CMD_SET_LINE_WIDTH		(0x0001|VKVG_CMD_PATHPROPS_COMMANDS)
#define VKVG_CMD_SET_MITER_LIMIT	(0x0002|VKVG_CMD_PATHPROPS_COMMANDS)
#define VKVG_CMD_SET_LINE_JOIN		(0x0003|VKVG_CMD_PATHPROPS_COMMANDS)
#define VKVG_CMD_SET_LINE_CAP		(0x0004|VKVG_CMD_PATHPROPS_COMMANDS)
#define VKVG_CMD_SET_OPERATOR		(0x0005|VKVG_CMD_PATHPROPS_COMMANDS)
#define VKVG_CMD_SET_FILL_RULE		(0x0006|VKVG_CMD_PATHPROPS_COMMANDS)
#define VKVG_CMD_SET_DASH			(0x0007|VKVG_CMD_PATHPROPS_COMMANDS)

#define VKVG_CMD_TRANSLATE			(0x0001|VKVG_CMD_TRANSFORM_COMMANDS)
#define VKVG_CMD_ROTATE				(0x0002|VKVG_CMD_TRANSFORM_COMMANDS)
#define VKVG_CMD_SCALE				(0x0003|VKVG_CMD_TRANSFORM_COMMANDS)
#define VKVG_CMD_TRANSFORM			(0x0004|VKVG_CMD_TRANSFORM_COMMANDS)
#define VKVG_CMD_IDENTITY_MATRIX	(0x0005|VKVG_CMD_TRANSFORM_COMMANDS)

#define VKVG_CMD_SET_MATRIX			(0x0006|VKVG_CMD_TRANSFORM_COMMANDS)

#define VKVG_CMD_SET_FONT_SIZE		(0x0001|VKVG_CMD_TEXT_COMMANDS)
#define VKVG_CMD_SET_FONT_FACE		(0x0002|VKVG_CMD_TEXT_COMMANDS)
#define VKVG_CMD_SET_FONT_PATH		(0x0003|VKVG_CMD_TEXT_COMMANDS)
#define VKVG_CMD_SHOW_TEXT			(0x0004|VKVG_CMD_TEXT_COMMANDS)

#define VKVG_CMD_REL_MOVE_TO			(VKVG_CMD_MOVE_TO			|VKVG_CMD_RELATIVE_COMMANDS)
#define VKVG_CMD_REL_LINE_TO			(VKVG_CMD_LINE_TO			|VKVG_CMD_RELATIVE_COMMANDS)
#define VKVG_CMD_REL_CURVE_TO			(VKVG_CMD_CURVE_TO			|VKVG_CMD_RELATIVE_COMMANDS)
#define VKVG_CMD_REL_QUADRATIC_TO		(VKVG_CMD_QUADRATIC_TO		|VKVG_CMD_RELATIVE_COMMANDS)
#define VKVG_CMD_REL_ELLIPTICAL_ARC_TO	(VKVG_CMD_ELLIPTICAL_ARC_TO	|VKVG_CMD_RELATIVE_COMMANDS)

#define VKVG_CMD_PAINT				(0x0001|VKVG_CMD_DRAW_COMMANDS)
#define VKVG_CMD_FILL				(0x0002|VKVG_CMD_DRAW_COMMANDS)
#define VKVG_CMD_STROKE				(0x0003|VKVG_CMD_DRAW_COMMANDS)
#define VKVG_CMD_CLIP				(0x0004|VKVG_CMD_DRAW_COMMANDS)
#define VKVG_CMD_RESET_CLIP			(0x0005|VKVG_CMD_DRAW_COMMANDS)
#define VKVG_CMD_CLEAR				(0x0006|VKVG_CMD_DRAW_COMMANDS)

#define VKVG_CMD_FILL_PRESERVE		(VKVG_CMD_FILL	|VKVG_CMD_PRESERVE_COMMANDS)
#define VKVG_CMD_STROKE_PRESERVE	(VKVG_CMD_STROKE	|VKVG_CMD_PRESERVE_COMMANDS)
#define VKVG_CMD_CLIP_PRESERVE		(VKVG_CMD_CLIP	|VKVG_CMD_PRESERVE_COMMANDS)

#define VKVG_CMD_SET_SOURCE_RGB		(0x0001|VKVG_CMD_PATTERN_COMMANDS)
#define VKVG_CMD_SET_SOURCE_RGBA	(0x0002|VKVG_CMD_PATTERN_COMMANDS)
#define VKVG_CMD_SET_SOURCE_COLOR	(0x0003|VKVG_CMD_PATTERN_COMMANDS)
#define VKVG_CMD_SET_SOURCE			(0x0004|VKVG_CMD_PATTERN_COMMANDS)
#define VKVG_CMD_SET_SOURCE_SURFACE	(0x0005|VKVG_CMD_PATTERN_COMMANDS)

void				_start_recording	(VkgContext ctx);
vkg_recording*	    _stop_recording		(VkgContext ctx);
void				_destroy_recording	(vkg_recording* rec);
void				_replay_command		(VkgContext ctx, VkgRecording rec, uint32_t index);
void				_record				(vkg_recording* rec,...);

#define RECORD(data,...) {\
	if (data->recording)	{\
		_record (data->recording,__VA_ARGS__);\
		return;\
	}\
}
#define RECORD2(data,...) {\
	if (data->recording)	{\
		_record (data->recording,__VA_ARGS__);\
		return 0;\
	}\
}

void _explicit_ms_resolve (VkgSurface surf);
void _clear_surface (VkgSurface surf, VkImageAspectFlags aspect);
void _create_surface_main_image (VkgSurface surf);
void _create_surface_secondary_images (VkgSurface surf);
void _create_framebuffer (VkgSurface surf);
void _create_surface_images (VkgSurface surf);
VkgSurface _create_surface (VkgDevice dev, VkFormat format);

void _surface_submit_cmd (VkgSurface surf);
//bool _surface_wait_cmd (VkgSurface surf);










struct vkg_text_run {
	struct vkg_font_identity*	fontId;		/* vkvg font structure pointer */
	struct _vkg_font_t*			font;		/* vkvg font structure pointer */

	VkgDevice				dev;		/* vkvg device associated with this text run */
	vkg_text_extents		extents;	/* store computed text extends */
	
	const char*				text;		/* utf8 char array of text*/
	unsigned int			glyph_count;/* Total glyph count */

#ifdef VKVG_USE_HARFBUZZ
	struct hb_buffer_t*			hbBuf;		/* HarfBuzz buffer of text */
	struct hb_glyph_position_t*	glyphs;		/* HarfBuzz computed glyph positions array */
#else
	vkg_glyph_info_t*		glyphs;		/* computed glyph positions array */
#endif
};

struct vkg_pattern {
	VkeStatus			status;
	VkgPattern::type_t 	type;
	vkg_extend_t		extend;
	vkg_filter_t		filter;
	vkg_matrix		matrix;
	bool				hasMatrix;
	void*				data;
};

struct vkg_gradient {
	vkg_color_t			colors[16];
	vec4d				stops[16];
	vec4d				cp[2];
	uint32_t			count;
};

struct vkg_device {
	/// data was cross over from vke but i believe its better to reference it.
	VkeDevice           vke;
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

	VkgContext				lastCtx;				/**< last element of double linked list of context, used to trigger font caching system update on all contexts*/

	int32_t					cachedContextMaxCount;	/**< Maximum context cache element count.*/
	int32_t					cachedContextCount;		/**< Current context cache element count.*/
	struct _cached_ctx_t*   cachedContextLast;		/**< Last element of single linked list of saved context for fast reuse.*/

#ifdef VKVG_WIRED_DEBUG
	VkPipeline				pipelineWired;
	VkPipeline				pipelineLineList;
#endif
#if VKVG_DBG_STATS
	vkg_debug_stats		debug_stats;			/**< debug statistics on memory usage and vulkan ressources */
#endif
}; /// keeping with this pattern, no *_t (replacing others)

struct vkg_context {
	VkeStatus		status;
	uint32_t			references;		//reference count

	VkgDevice			dev;
	VkgSurface			pSurf;			//surface bound to context, set on creation of ctx
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
	void (*vertex_cb)(VKVG_IBO_INDEX_TYPE, VkgContext);//tesselator vertex callback
	VKVG_IBO_INDEX_TYPE tesselator_fan_start;
	uint32_t			tesselator_idx_counter;
	
#endif

#if VKVG_RECORDING
	vkg_recording*		recording;
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

	vkg_operator		curOperator;
	vkg_line_cap		lineCap;
	vkg_line_join	lineJoin;
	vkg_fill_rule	curFillRule;

	long				selectedCharSize; /* Font size*/
	char				selectedFontName[FONT_NAME_MAX_SIZE];
	//_vkg_font_t		  selectedFont;		//hold current face and size before cache addition
	struct vkg_font_identity* currentFont;		//font pointing to cached fonts identity
	struct _vkg_font_t*		  currentFontSize;	//font structure by size ready for lookup
	vkg_direction	textDirection;

	push_constants		pushConsts;
	VkgPattern			pattern;

	struct _vkg_context_save* pSavedCtxs;		//last ctx saved ptr
	uint8_t				curSavBit;			//current stencil bit used to save context, 6 bits used by stencil for save/restore
	VkeImage*			savedStencils;		//additional image for saving contexes once more than 6 save/restore are reached
	vkg_clip_state	curClipState;		//current clipping status relative to the previous saved one or clear state if none.

	VkClearRect			clearRect;
	VkRenderPassBeginInfo renderPassBeginInfo;
};

struct vkg_surface {
	VkeStatus	status;					/**< Current status of surface, affected by last operation */
	VkgDevice		dev;
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

}
