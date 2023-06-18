#ifndef VKVG_INTERNAL_H
#define VKVG_INTERNAL_H

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
#include <io/io.h>

#include <tinycthread.h>
#include "cross_os.h"

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

#include <vkg/vkvg_internal.h>

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
}_vkvg_font_t;

/* Font identification structure */
typedef struct {
	char**				names;		/* Resolved Input names to this font by fontConfig or custom name set by @ref vkvg_load_from_path*/
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
	_vkvg_font_t*		sizes;		/* loaded font size array */
}_vkvg_font_identity_t;

// Font cache global structure, entry point for all font related operations.
typedef struct _font_cache_t {
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

	_vkvg_font_identity_t*	fonts;	/* Loaded fonts structure array */
	int32_t			fontsCount;		/* Loaded fonts array count*/
} _font_cache;

struct _vkvg_device;
struct _vkvg_context;
void _fonts_cache_create				(struct _vkvg_device *dev);
void _font_cache_free				    (struct _vkvg_device *dev);
_vkvg_font_identity_t *_font_cache_add_font_identity (struct _vkvg_context *ctx, const char* fontFile, const char *name);
bool _font_cache_load_font_file_in_memory (_vkvg_font_identity_t* fontId);
void _font_cache_show_text				(struct _vkvg_context *ctx, const char* text);
void _font_cache_text_extents			(struct _vkvg_context *ctx, const char* text, int length, vkvg_text_extents_t *extents);
void _font_cache_font_extents			(struct _vkvg_context *ctx, vkvg_font_extents_t* extents);
void _font_cache_create_text_run		(struct _vkvg_context *ctx, const char* text, int length, VkvgText textRun);
void _font_cache_destroy_text_run		(VkvgText textRun);
void _font_cache_show_text_run			(struct _vkvg_context *ctx, VkvgText tr);
void _font_cache_update_context_descset (struct _vkvg_context *ctx);

/*

typedef struct {
	VkeStatus status;
} _vkvg_no_mem_struct;

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

typedef struct _cached_ctx_t {
	thrd_t				thread;
	VkvgContext			ctx;
	struct _cached_ctx*	pNext;
} _cached_ctx;

bool _device_try_get_phyinfo			(VkePhyInfo* phys, uint32_t phyCount, VkPhysicalDeviceType gpuType, VkePhyInfo* phy);
bool _device_init_function_pointers		(VkvgDevice dev);
void _device_create_empty_texture		(VkvgDevice dev, VkFormat format, VkImageTiling tiling);
void _device_get_best_image_tiling		(VkvgDevice dev, VkFormat format, VkImageTiling* pTiling);
void _device_check_best_image_tiling	(VkvgDevice dev, VkFormat format);
void _device_create_pipeline_cache		(VkvgDevice dev);
VkRenderPass _device_createRenderPassMS	(VkvgDevice dev, VkAttachmentLoadOp loadOp, VkAttachmentLoadOp stencilLoadOp);
VkRenderPass _device_createRenderPassNoResolve(VkvgDevice dev, VkAttachmentLoadOp loadOp, VkAttachmentLoadOp stencilLoadOp);
void _device_setupPipelines				(VkvgDevice dev);
void _device_createDescriptorSetLayout 	(VkvgDevice dev);
void _device_wait_idle					(VkvgDevice dev);
void _device_wait_and_reset_device_fence(VkvgDevice dev);
void _device_submit_cmd					(VkvgDevice dev, VkCommandBuffer* cmd, VkFence fence);

bool _device_try_get_cached_context		(VkvgDevice dev, VkvgContext* pCtx);
void _device_store_context				(VkvgContext ctx);

#define FULLSCREEN_BIT	0x10000000
#define SRCTYPE_MASK	0x000000FF

#define CreateRgba(r, g, b, a) ((a << 24) | (r << 16) | (g << 8) | b)
#ifdef VKVG_PREMULT_ALPHA
	#define CreateRgbaf(r, g, b, a) (((int)(a * 255.0f) << 24) | ((int)(b * a * 255.0f) << 16) | ((int)(g * a * 255.0f) << 8) | (int)(r * a * 255.0f))
#else
	#define CreateRgbaf(r, g, b, a) (((int)(a * 255.0f) << 24) | ((int)(b * 255.0f) << 16) | ((int)(g * 255.0f) << 8) | (int)(r * 255.0f))
#endif

typedef struct _vkvg_context_save_t {
	struct _vkvg_context_save_t* pNext;
	float					lineWidth;
	float					miterLimit;
	uint32_t				dashCount;		//value count in dash array, 0 if dash not set.
	float					dashOffset;		//an offset for dash
	float*					dashes;			//an array of alternate lengths of on and off stroke.
	vkvg_operator_t			curOperator;
	vkvg_line_cap_t			lineCap;
	vkvg_line_join_t		lineJoint;
	vkvg_fill_rule_t		curFillRule;
	long					selectedCharSize; /* Font size*/
	char					selectedFontName[FONT_NAME_MAX_SIZE];
	_vkvg_font_identity_t	selectedFont;	   //hold current face and size before cache addition
	_vkvg_font_identity_t*	currentFont;	   //font ready for lookup
	vkvg_direction_t		textDirection;
	push_constants			pushConsts;
	uint32_t				curColor;
	VkvgPattern				pattern;
	vkvg_clip_state_t		clippingState;
} vkvg_context_save;

io_declare(vkvg_context_save);

typedef struct _ear_clip_point {
	vec2f					pos;
	VKVG_IBO_INDEX_TYPE		idx;
	struct _ear_clip_point*	next;
} ear_clip_point;

typedef struct {
	bool					dashOn;
	uint32_t				curDash;		//current dash index
	float					curDashOffset;	//cur dash offset between defined path point and last dash segment(on/off) start
	float					totDashLength;	//total length of dashes
	vec2f					normal;
} dash_context_t;

typedef struct {
	uint32_t	iL;
	uint32_t	iR;
	uint32_t	cp;//current point
	VKVG_IBO_INDEX_TYPE 	firstIdx;//save first point idx for closed path
	float		hw;				//stroke half width, computed once.
	float		lhMax;			//miter limit * line width
	float		arcStep;		//cached arcStep, prevent compute multiple times for same stroke, 0 if not yet computed
} stroke_context_t;

void _check_vertex_cache_size	(VkvgContext ctx);
void _ensure_vertex_cache_size	(VkvgContext ctx, uint32_t addedVerticesCount);
void _resize_vertex_cache		(VkvgContext ctx, uint32_t newSize);

void _check_index_cache_size	(VkvgContext ctx);
void _ensure_index_cache_size	(VkvgContext ctx, uint32_t addedIndicesCount);
void _resize_index_cache		(VkvgContext ctx, uint32_t newSize);

bool _check_pathes_array		(VkvgContext ctx);

bool _current_path_is_empty		(VkvgContext ctx);
void _finish_path				(VkvgContext ctx);
void _clear_path				(VkvgContext ctx);
void _remove_last_point			(VkvgContext ctx);
bool _path_is_closed			(VkvgContext ctx, uint32_t ptrPath);
void _set_curve_start			(VkvgContext ctx);
void _set_curve_end				(VkvgContext ctx);
bool _path_has_curves			(VkvgContext ctx, uint32_t ptrPath);

float _normalizeAngle			(float a);
float _get_arc_step				(VkvgContext ctx, float radius);

vec2f *_get_current_position    (VkvgContext ctx);
void _add_point					(VkvgContext ctx, float x, float y);

void _resetMinMax				(VkvgContext ctx);
void _vkvg_path_extents			(VkvgContext ctx, bool transformed, float *x1, float *y1, float *x2, float *y2);
void _draw_stoke_cap			(VkvgContext ctx, stroke_context_t* str, vec2f p0, vec2f n, bool isStart);
void _draw_segment				(VkvgContext ctx, stroke_context_t* str, dash_context_t* dc, bool isCurve);
float _draw_dashed_segment		(VkvgContext ctx, stroke_context_t *str, dash_context_t* dc, bool isCurve);
bool _build_vb_step				(VkvgContext ctx, stroke_context_t *str, bool isCurve);

void _poly_fill					(VkvgContext ctx, vec4f *bounds);
void _fill_non_zero				(VkvgContext ctx);
void _draw_full_screen_quad		(VkvgContext ctx, vec4f *scissor);

void _create_gradient_buff		(VkvgContext ctx);
void _create_vertices_buff		(VkvgContext ctx);
void _add_vertex				(VkvgContext ctx, Vertex v);
void _add_vertexf				(VkvgContext ctx, float x, float y);
void _set_vertex				(VkvgContext ctx, uint32_t idx, Vertex v);
void _add_triangle_indices		(VkvgContext ctx, VKVG_IBO_INDEX_TYPE i0, VKVG_IBO_INDEX_TYPE i1, VKVG_IBO_INDEX_TYPE i2);
void _add_tri_indices_for_rect	(VkvgContext ctx, VKVG_IBO_INDEX_TYPE i);

void _vao_add_rectangle			(VkvgContext ctx, float x, float y, float width, float height);

void _bind_draw_pipeline		(VkvgContext ctx);
void _create_cmd_buff			(VkvgContext ctx);
void _check_vao_size			(VkvgContext ctx);
void _flush_cmd_buff			(VkvgContext ctx);
void _ensure_renderpass_is_started		(VkvgContext ctx);
void _emit_draw_cmd_undrawn_vertices	(VkvgContext ctx);
void _flush_cmd_until_vx_base	(VkvgContext ctx);
bool _wait_ctx_flush_end		(VkvgContext ctx);
bool _wait_and_submit_cmd		(VkvgContext ctx);
void _update_push_constants		(VkvgContext ctx);
void _update_cur_pattern		(VkvgContext ctx, VkvgPattern pat);
void _set_mat_inv_and_vkCmdPush (VkvgContext ctx);
void _start_cmd_for_render_pass (VkvgContext ctx);

void _createDescriptorPool		(VkvgContext ctx);
void _init_descriptor_sets		(VkvgContext ctx);
void _update_descriptor_set		(VkvgContext ctx, VkeImage img, VkDescriptorSet ds);
void _update_gradient_desc_set	(VkvgContext ctx);
void _free_ctx_save				(vkvg_context_save* sav);
void _release_context_ressources(VkvgContext ctx);

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

void _recursive_bezier(VkvgContext ctx, float distanceTolerance,
					   float x1, float y1, float x2, float y2,
					   float x3, float y3, float x4, float y4,
					   unsigned level);

void _bezier (VkvgContext ctx,
			  float x1, float y1, float x2, float y2,
			  float x3, float y3, float x4, float y4);

void _line_to		(VkvgContext ctx, float x, float y);

void _elliptic_arc	(VkvgContext ctx, float x1, float y1, float x2, float y2,
					 bool largeArc, bool counterClockWise, float _rx, float _ry, float phi);

void _select_font_face			(VkvgContext ctx, const char* name);




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

void				_start_recording	(VkvgContext ctx);
vkvg_recording*	    _stop_recording		(VkvgContext ctx);
void				_destroy_recording	(vkvg_recording* rec);
void				_replay_command		(VkvgContext ctx, VkvgRecording rec, uint32_t index);
void				_record				(vkvg_recording* rec,...);

#define RECORD(ctx,...) {\
	if (ctx->recording)	{\
		_record (ctx->recording,__VA_ARGS__);\
		return;\
	}\
}
#define RECORD2(ctx,...) {\
	if (ctx->recording)	{\
		_record (ctx->recording,__VA_ARGS__);\
		return 0;\
	}\
}

void _explicit_ms_resolve (VkvgSurface surf);
void _clear_surface (VkvgSurface surf, VkImageAspectFlags aspect);
void _create_surface_main_image (VkvgSurface surf);
void _create_surface_secondary_images (VkvgSurface surf);
void _create_framebuffer (VkvgSurface surf);
void _create_surface_images (VkvgSurface surf);
VkvgSurface _create_surface (VkvgDevice dev, VkFormat format);

void _surface_submit_cmd (VkvgSurface surf);
//bool _surface_wait_cmd (VkvgSurface surf);


#endif
