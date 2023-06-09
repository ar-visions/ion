﻿
#include <vkg/internal.hh>

#include <locale.h>
#include <string.h>
#include <wchar.h>

#ifndef VKVG_USE_FREETYPE
	#define STB_TRUETYPE_IMPLEMENTATION
	#include "stb_truetype.h"
#endif

static int defaultFontCharSize = 12<<6;

void _font_cache_free(vkg_device *dev) {
	_font_cache* cache = (_font_cache*)dev->fontCache;
	free (cache->hostBuff);

	for (int i = 0; i < cache->fontsCount; ++i) {
		vkg_font_identity* f = &cache->fonts[i];
		for (uint32_t j = 0; j < f->sizeCount; j++) {
			vkg_font* s = &f->sizes[j];
#ifdef VKVG_USE_FREETYPE
			for (int g = 0; g < s->face->num_glyphs; ++g) {
				if (s->charLookup[g]!=NULL)
					free(s->charLookup[g]);
			}
			FT_Done_Face (s->face);
#else
			for (int g = 0; g < f->stbInfo.numGlyphs; ++g) {
				if (s->charLookup[g]!=NULL)
					free(s->charLookup[g]);
			}
#endif

#ifdef VKVG_USE_HARFBUZZ
			hb_font_destroy(s->hb_font);
#endif
			free(s->charLookup);
		}
		free (f->sizes);
		free(f->fontFile);
		for (uint32_t j = 0; j < f->namesCount; j++)
			free (f->names[j]);
		if (f->namesCount > 0)
			free (f->names);
		free (f->fontBuffer);
	}

	free(cache->fonts);
	free(cache->pensY);

	vke_buffer_reset	(&cache->buff);
	io_drop(vke_image, cache->texture);
	//vkFreeCommandBuffers(dev->vke->vkdev,dev->cmdPool, 1, &cache->cmd);
	vkDestroyFence		(dev->vke->vkdev,cache->uploadFence,NULL);
#ifdef VKVG_USE_FREETYPE
	FT_Done_FreeType(cache->library);
#endif
#ifdef VKVG_USE_FONTCONFIG
	FcConfigDestroy(cache->config);
	FcFini();
#endif
	_font_cache_drop(dev->fontCache);
}

void _fonts_cache_create (VkgDevice dev) {
	_font_cache *cache = io_new(_font_cache);

#ifdef VKVG_USE_FONTCONFIG
	cache->config = FcInitLoadConfigAndFonts ();
	if (!cache->config) {
		vke_log(VKE_LOG_DEBUG, "Font config initialisation failed, consider using 'FONTCONFIG_PATH' and 'FONTCONFIG_FILE' environmane\
					   variables to point to 'fonts.conf' needed for FontConfig startup");
		assert(cache->config);
	}
#endif

#ifdef VKVG_USE_FREETYPE
	FT_CHECK_RESULT(FT_Init_FreeType(&cache->library));
#endif

#if defined(VKVG_LCD_FONT_FILTER) && defined(FT_CONFIG_OPTION_SUBPIXEL_RENDERING)
	FT_CHECK_RESULT(FT_Library_SetLcdFilter (cache->library, FT_LCD_FILTER_LIGHT));
	cache->texFormat = FB_COLOR_FORMAT;
	cache->texPixelSize = 4;
#else
	cache->texFormat = VK_FORMAT_R8_UNORM;
	cache->texPixelSize = 1;
#endif

	cache->texLength = FONT_CACHE_INIT_LAYERS;
	cache->texture = VkeImage::tex2d_array_create (dev->vke, cache->texFormat, FONT_PAGE_SIZE, FONT_PAGE_SIZE,
							cache->texLength ,VKE_MEMORY_USAGE_GPU_ONLY,
							VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	vke_image_create_descriptor (cache->texture, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_ASPECT_COLOR_BIT,
								 VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);

	cache->uploadFence = vke_fence_create(dev->vke);

	const uint32_t buffLength = FONT_PAGE_SIZE*FONT_PAGE_SIZE*cache->texPixelSize;

	vke_buffer_init (dev->vke,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VKE_MEMORY_USAGE_CPU_TO_GPU,
		buffLength, &cache->buff, true);

	cache->cmd = vke_cmd_buff_create(dev->vke,dev->cmdPool,VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	//Set texture cache initial layout to shaderReadOnly to prevent error msg if cache is not fill
	const VkImageSubresourceRange subres = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,cache->texLength};
	cache->cmd.begin(VkeCommandBufferUsage::one_time_submit);
	vke_image_set_layout_subres(cache->cmd, cache->texture, subres,
								VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
								VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	VK_CHECK_RESULT(vkEndCommandBuffer(cache->cmd));
	_device_submit_cmd (dev, &cache->cmd, cache->uploadFence);

	cache->hostBuff = (uint8_t*)malloc(buffLength);
	cache->pensY = (int*)calloc(cache->texLength, sizeof(int));

	dev->fontCache = cache;
}
///increase layer count of 2d texture array used as font cache.
void _increase_font_tex_array (VkgDevice dev){
	vke_log(VKE_LOG_INFO, "_increase_font_tex_array\n");

	_font_cache* cache = dev->fontCache;

	vkWaitForFences(dev->vke->vkdev, 1, &cache->uploadFence, VK_TRUE, UINT64_MAX);
	ResetFences(dev->vke->vkdev, 1, &cache->uploadFence);

	vkResetCommandBuffer(cache->cmd, 0);

	uint8_t newSize = cache->texLength + FONT_CACHE_INIT_LAYERS;
	VkeImage newImg = vke_tex2d_array_create ((VkeDevice)dev, cache->texFormat, FONT_PAGE_SIZE, FONT_PAGE_SIZE,
											  newSize ,VKE_MEMORY_USAGE_GPU_ONLY,
											  VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	vke_image_create_descriptor (newImg, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_ASPECT_COLOR_BIT,
							   VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST,VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);

	VkImageSubresourceRange subresNew	= {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,newSize};
	VkImageSubresourceRange subres		= {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,cache->texLength};

	cache->cmd.begin(VkeCommandBufferUsage::one_time_submit);

	vke_image_set_layout_subres(cache->cmd, newImg, subresNew,
								VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	vke_image_set_layout_subres(cache->cmd, cache->texture, subres,
								VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
								VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	VkImageCopy cregion = { .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, cache->texLength},
							.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, cache->texLength},
							.extent = {FONT_PAGE_SIZE,FONT_PAGE_SIZE,1}};

	vkCmdCopyImage (cache->cmd, vke_image_get_vkimage (cache->texture), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
								vke_image_get_vkimage (newImg), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cregion);

	vke_image_set_layout_subres(cache->cmd, newImg, subresNew,
								VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
								VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	vke_image_set_layout_subres(cache->cmd, cache->texture, subres,
								VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
								VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	VK_CHECK_RESULT(vkEndCommandBuffer(cache->cmd));

	_device_submit_cmd  (dev, &cache->cmd, cache->uploadFence);
	vkWaitForFences		(dev->vke->vkdev, 1, &cache->uploadFence, VK_TRUE, UINT64_MAX);

	cache->pensY = (int*)realloc(cache->pensY, newSize * sizeof(int));
	void* tmp = memset (&cache->pensY[cache->texLength],0,FONT_CACHE_INIT_LAYERS*sizeof(int));

	vke_image_drop	(cache->texture);

	cache->texLength   = newSize;
	cache->texture	   = newImg;

	_device_wait_idle(dev);
}
//flush font stagging buffer to cache texture array
//Trigger stagging buffer to be uploaded in font cache. Groupping upload improve performances.
void _flush_chars_to_tex (VkgDevice dev, vkg_font* f) {

	_font_cache* cache = dev->fontCache;
	if (cache->stagingX == 0)//no char in stagging buff to flush
		return;

	vke_log(VKE_LOG_INFO, "_flush_chars_to_tex pen(%d, %d)\n",f->curLine.penX, f->curLine.penY);
	vkWaitForFences		(dev->vke->vkdev,1,&cache->uploadFence,VK_TRUE,UINT64_MAX);
	ResetFences (dev->vke->vkdev, 1, &cache->uploadFence);

	vkResetCommandBuffer(cache->cmd,0);

	memcpy(vke_buffer_get_mapped_pointer (&cache->buff), cache->hostBuff, (uint64_t)f->curLine.height * FONT_PAGE_SIZE * cache->texPixelSize);

	cache->cmd.begin(VkeCommandBufferUsage::one_time_submit);

	VkImageSubresourceRange subres		= {VK_IMAGE_ASPECT_COLOR_BIT,0,1,f->curLine.pageIdx,1};
	vke_image_set_layout_subres(cache->cmd, cache->texture, subres,
								VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	VkBufferImageCopy bufferCopyRegion = { .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,f->curLine.pageIdx,1},
										   .bufferRowLength = FONT_PAGE_SIZE,
										   .bufferImageHeight = f->curLine.height,
										   .imageOffset = {f->curLine.penX,f->curLine.penY,0},
										   .imageExtent = {FONT_PAGE_SIZE-f->curLine.penX,f->curLine.height,1}};

	vkCmdCopyBufferToImage(cache->cmd, cache->buff.buffer,
						   vke_image_get_vkimage (cache->texture), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);

	vke_image_set_layout_subres(cache->cmd, cache->texture, subres,
								VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
								VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	VK_CHECK_RESULT(vkEndCommandBuffer(cache->cmd));

	_device_submit_cmd (dev, &cache->cmd, cache->uploadFence);

	f->curLine.penX += cache->stagingX;
	cache->stagingX = 0;
	memset(cache->hostBuff, 0, (uint64_t)FONT_PAGE_SIZE * FONT_PAGE_SIZE * cache->texPixelSize);
}
///Start a new line in font cache, increase texture layer count if needed.
void _init_next_line_in_tex_cache (VkgDevice dev, vkg_font* f){
	_font_cache* cache = dev->fontCache;
	int i;
	for (i = 0; i < cache->texLength; ++i) {
		if (cache->pensY[i] + f->curLine.height >= FONT_PAGE_SIZE)
			continue;
		f->curLine.pageIdx = (unsigned char)i;
		f->curLine.penX = 0;
		f->curLine.penY = cache->pensY[i];
		cache->pensY[i] += f->curLine.height;
		return;
	}
	_flush_chars_to_tex			(dev, f);
	_increase_font_tex_array	(dev);
	_init_next_line_in_tex_cache(dev, f);
}

void _font_cache_update_context_descset (VkgContext ctx) {
	if (ctx->fontCacheImg)
		vke_image_drop(ctx->fontCacheImg);

	io_sync(ctx->dev);

	ctx->fontCacheImg = ctx->dev->fontCache->texture;
	vke_image_reference (ctx->fontCacheImg);

	update_descriptor_set (ctx, ctx->fontCacheImg, ctx->dsFont);

	io_unsync(ctx->dev);
}
//create a new char entry and put glyph in stagging buffer, ready for upload.
_char_ref* _prepare_char (VkgDevice dev, VkgText tr, uint32_t gindex){
	vkg_font* f = tr->font;
#ifdef VKVG_USE_FREETYPE
	#if defined(VKVG_LCD_FONT_FILTER) && defined(FT_CONFIG_OPTION_SUBPIXEL_RENDERING)
		FT_CHECK_RESULT(FT_Load_Glyph(f->face, gindex, FT_LOAD_TARGET_NORMAL));
		FT_CHECK_RESULT(FT_Render_Glyph(f->face->glyph, FT_RENDER_MODE_LCD));
	#else
		FT_CHECK_RESULT(FT_Load_Glyph(f->face, gindex, FT_LOAD_RENDER));
	#endif

	FT_GlyphSlot	slot			= f->face->glyph;
	FT_Bitmap		bmp				= slot->bitmap;
	uint32_t		bmpByteWidth	= bmp.width;
	uint32_t		bmpPixelWidth	= bmp.width;
	uint32_t		bmpRows			= bmp.rows;
	unsigned char*  buffer			= bmp.buffer;

#if defined(VKVG_LCD_FONT_FILTER) && defined(FT_CONFIG_OPTION_SUBPIXEL_RENDERING)
	bmpPixelWidth /= 3;
#endif
#else
	stbtt_fontinfo*	pStbInfo = &tr->fontId->stbInfo;
	int c_x1, c_y1, c_x2, c_y2;
	stbtt_GetGlyphBitmapBox (pStbInfo, gindex, f->scale, f->scale, &c_x1, &c_y1, &c_x2, &c_y2);
	uint32_t bmpByteWidth	= c_x2 - c_x1;
	uint32_t bmpPixelWidth	= bmpByteWidth;
	uint32_t bmpRows		= c_y2 - c_y1;
#endif
	uint8_t* data = dev->fontCache->hostBuff;

	if (dev->fontCache->stagingX + f->curLine.penX + bmpPixelWidth > FONT_PAGE_SIZE){
		_flush_chars_to_tex (dev, f);
		_init_next_line_in_tex_cache (dev, f);
	}

	_char_ref* cr = (_char_ref*)malloc(sizeof(_char_ref));
	int penX = dev->fontCache->stagingX;

#ifdef VKVG_USE_FREETYPE
	for(uint32_t y=0; y < bmpRows; y++) {
		for(uint32_t x=0; x < bmpPixelWidth; x++) {
#if defined(VKVG_LCD_FONT_FILTER) && defined(FT_CONFIG_OPTION_SUBPIXEL_RENDERING)
			unsigned char r = buffer[y * bmp.pitch + x * 3];
			unsigned char g = buffer[y * bmp.pitch + x * 3 + 1];
			unsigned char b = buffer[y * bmp.pitch + x * 3 + 2];

			data[(penX + x + y * FONT_PAGE_SIZE) * 4] = b;
			data[(penX + x + y * FONT_PAGE_SIZE) * 4 + 1] = g;
			data[(penX + x + y * FONT_PAGE_SIZE) * 4 + 2] = r;
			data[(penX + x + y * FONT_PAGE_SIZE) * 4 + 3] = (r+g+b)/3;
#else
			data[penX + x + y * FONT_PAGE_SIZE ] = buffer[x + y * bmpPixelWidth];
#endif
		}
	}
	cr->bmpDiff.x	= (int16_t)slot->bitmap_left;
	cr->bmpDiff.y	= (int16_t)slot->bitmap_top;
	cr->advance		= slot->advance;
#else
	int advance;
	int lsb;
	stbtt_GetGlyphHMetrics(pStbInfo, gindex, &advance, &lsb);
	stbtt_MakeGlyphBitmap (pStbInfo, data + penX, bmpPixelWidth, bmpRows, FONT_PAGE_SIZE, f->scale, f->scale, gindex);
	cr->bmpDiff.x	= (int16_t)c_x1;
	cr->bmpDiff.y	= (int16_t)-c_y1;
	cr->advance		= (vec2f) {(uint32_t)roundf (f->scale * advance) << 6, 0};
#endif
	vec4f uvBounds = {
		{(float)(penX + f->curLine.penX) / (float)FONT_PAGE_SIZE},
		{(float)f->curLine.penY / (float)FONT_PAGE_SIZE},
		{(float)bmpPixelWidth},
		{(float)bmpRows}};
	cr->bounds		= uvBounds;
	cr->pageIdx		= f->curLine.pageIdx;

	f->charLookup[gindex] = cr;
	dev->fontCache->stagingX += bmpPixelWidth;
	return cr;
}
void _font_add_name (vkg_font_identity* font, const char* name) {
	if (++font->namesCount == 1)
		font->names = (char**) malloc (sizeof(char*));
	else
		font->names = (char**) realloc (font->names, font->namesCount * sizeof(char*));
	font->names[font->namesCount-1] = (char*)calloc(strlen(name)+1, sizeof (char));
	strcpy (font->names[font->namesCount-1], name);
}
bool _font_cache_load_font_file_in_memory (vkg_font_identity* fontId) {
	FILE* fontFile = fopen(fontId->fontFile, "rb");
	if (!fontFile)
		return false;
	fseek(fontFile, 0, SEEK_END);
	fontId->fontBufSize = ftell(fontFile); /* how long is the file ? */
	fseek(fontFile, 0, SEEK_SET); /* reset */
	fontId->fontBuffer = malloc(fontId->fontBufSize);
	fread(fontId->fontBuffer, fontId->fontBufSize, 1, fontFile);
	fclose(fontFile);
	return true;
}
vkg_font_identity* _font_cache_add_font_identity (VkgContext ctx, const char* fontFilePath, const char* name){
	_font_cache*	cache = (_font_cache*)ctx->dev->fontCache;
	if (++cache->fontsCount == 1)
		cache->fonts = (vkg_font_identity*) malloc (cache->fontsCount * sizeof(vkg_font_identity));
	else
		cache->fonts = (vkg_font_identity*) realloc (cache->fonts, cache->fontsCount * sizeof(vkg_font_identity));
	vkg_font_identity nf = {0};

	if (fontFilePath) {
		int fflength = strlen (fontFilePath) + 1;
		nf.fontFile = (char*)malloc (fflength * sizeof(char));
		strcpy (nf.fontFile, fontFilePath);
	}

	_font_add_name (&nf, name);

	cache->fonts[cache->fontsCount-1] = nf;
	return &cache->fonts[cache->fontsCount-1];
}
//select current font for context
vkg_font* _find_or_create_font_size (VkgContext ctx) {
	vkg_font_identity* font = ctx->currentFont;

	for (uint32_t i = 0; i < font->sizeCount; ++i) {
		if (font->sizes[i].charSize == ctx->selectedCharSize)
			return &font->sizes[i];
	}
	//if not found, create a new font size structure
	if (++font->sizeCount == 1)
		font->sizes = (vkg_font*) malloc (sizeof(vkg_font));
	else
		font->sizes = (vkg_font*) realloc (font->sizes, font->sizeCount * sizeof(vkg_font));
	vkg_font newSize = {.charSize = ctx->selectedCharSize};

	VkgDevice dev = ctx->dev;
#ifdef VKVG_USE_FREETYPE
	_font_cache*	cache = (_font_cache*)ctx->dev->fontCache;
	FT_CHECK_RESULT(FT_New_Memory_Face (cache->library, font->fontBuffer, font->fontBufSize, 0, &newSize.face));
	FT_CHECK_RESULT(FT_Set_Char_Size(newSize.face, 0, newSize.charSize, dev->vke->phyinfo->hdpi, dev->vke->phyinfo->vdpi ));

	newSize.charLookup = (_char_ref**)calloc (newSize.face->num_glyphs, sizeof(_char_ref*));

	if (FT_IS_SCALABLE(newSize.face))
		newSize.curLine.height = newSize.face->size->metrics.height >> 6;
	else
		newSize.curLine.height = newSize.face->height >> 6;
#else
	int result = stbtt_InitFont(&font->stbInfo, font->fontBuffer, 0);
	assert(result && "stbtt_initFont failed");
	if (!result) {
		ctx->status = VKE_STATUS_INVALID_FONT;
		return NULL;
	}
	stbtt_GetFontVMetrics(&font->stbInfo, &font->ascent, &font->descent, &font->lineGap);
	newSize.charLookup	= (_char_ref**)calloc (font->stbInfo.numGlyphs, sizeof(_char_ref*));
	//newSize.scale		= stbtt_ScaleForPixelHeight(&font->stbInfo, newSize.charSize);
	newSize.scale		= stbtt_ScaleForMappingEmToPixels(&font->stbInfo, newSize.charSize);
	newSize.curLine.height = roundf (newSize.scale * (font->ascent - font->descent + font->lineGap));
	newSize.ascent		= roundf (newSize.scale * font->ascent);
	newSize.descent		= roundf (newSize.scale * font->descent);
	newSize.lineGap		= roundf (newSize.scale * font->lineGap);
#endif

#ifdef VKVG_USE_HARFBUZZ
	newSize.hb_font = hb_ft_font_create(newSize.face, NULL);
#endif

	_init_next_line_in_tex_cache (dev, &newSize);

	font->sizes[font->sizeCount-1] = newSize;
	return &font->sizes[font->sizeCount-1];
}

//try find font already resolved with fontconfig by font name
bool _tryFindFontByName (VkgContext ctx, vkg_font_identity** font){
	_font_cache*	cache = ctx->dev->fontCache;
	for (int i = 0; i < cache->fontsCount; ++i) {
		for (uint32_t j = 0; j < cache->fonts[i].namesCount; j++) {
			if (strcmp (cache->fonts[i].names[j], ctx->selectedFontName) == 0) {
				*font = &cache->fonts[i];
				return true;
			}
		}
	}
	return false;
}

#ifdef VKVG_USE_FONTCONFIG
bool _tryResolveFontNameWithFontConfig (VkgContext ctx, vkg_font_identity** resolvedFont) {
	_font_cache*	cache = (_font_cache*)ctx->dev->fontCache;
	char* fontFile = NULL;

	FcPattern* pat = FcNameParse((const FcChar8*)ctx->selectedFontName);
	FcConfigSubstitute(cache->config, pat, FcMatchPattern);
	FcDefaultSubstitute(pat);
	FcResult result;
	FcPattern* font = FcFontMatch(cache->config, pat, &result);
	if (font)
		FcPatternGetString(font, FC_FILE, 0, (FcChar8 **)&fontFile);
	*resolvedFont = NULL;
	if (fontFile) {
		//try find font in cache by path
		for (int i = 0; i < cache->fontsCount; ++i) {
			if (cache->fonts[i].fontFile && strcmp (cache->fonts[i].fontFile, fontFile) == 0) {
				_font_add_name (&cache->fonts[i], ctx->selectedFontName);
				*resolvedFont = &cache->fonts[i];
				break;
			}
		}
		if (!*resolvedFont) {
			//if not found, create a new vkg_font
			vkg_font_identity* fid = _font_cache_add_font_identity(ctx, fontFile, ctx->selectedFontName);
			_font_cache_load_font_file_in_memory (fid);
			*resolvedFont = &cache->fonts[cache->fontsCount-1];
		}
	}

	FcPatternDestroy(pat);
	FcPatternDestroy(font);

	return (fontFile != NULL);
}
#endif


//try to find corresponding font in cache (defined by context selectedFont) and create a new font entry if not found.
void _update_current_font (VkgContext ctx) {
	if (ctx->currentFont == NULL) {
		io_sync(ctx->dev);
		if (ctx->selectedFontName[0] == 0)
			_select_font_face (ctx, "sans");

		if (!_tryFindFontByName (ctx, &ctx->currentFont)) {
#ifdef VKVG_USE_FONTCONFIG
			_tryResolveFontNameWithFontConfig (ctx, &ctx->currentFont);
#else
			vke_log(VKE_LOG_ERR, "Unresolved font: %s\n", ctx->selectedFontName);
			io_unsync(ctx->dev);
			ctx->status = VKE_STATUS_INVALID_FONT;
			return;
#endif
		}

		ctx->currentFontSize = _find_or_create_font_size (ctx);
		io_unsync(ctx->dev);
	}	
}

#ifdef VKVG_USE_HARFBUZZ
//Get harfBuzz buffer for provided text.
hb_buffer * _get_hb_buffer (vkg_font* font, const char* text, int length) {
	hb_buffer *buf = hb_buffer_create();

	hb_script_t script = HB_SCRIPT_LATIN;
	hb_unicode_funcs_t* ucfunc = hb_unicode_funcs_get_default ();
	wchar_t firstChar = 0;
	if (mbstowcs (&firstChar, text, 1))
		script = hb_unicode_script (ucfunc, firstChar);

	hb_direction_t dir = hb_script_get_horizontal_direction(script);
	hb_buffer_set_direction (buf, dir);
	hb_buffer_set_script	(buf, script);
	//hb_buffer_set_language	(buf, hb_language_from_string (lng, (int)strlen(lng)));
	hb_buffer_add_utf8		(buf, text, length, 0, length);

	hb_shape (font->hb_font, buf, NULL, 0);

	return buf;
}
#endif

//retrieve global font extends of context's current font as defined by FreeType
void _font_cache_font_extents (VkgContext ctx, vkg_font_extents *extents) {
	_update_current_font (ctx);

	if (ctx->status)
		return;

	//TODO: ensure correct metrics are returned (scalled/unscalled, etc..)
	vkg_font* font = ctx->currentFontSize;
#ifdef VKVG_USE_FREETYPE
	FT_BBox*			bbox	= &font->face->bbox;
	FT_Size_Metrics*	metrics = &font->face->size->metrics;

	extents->ascent			= (float)(FT_MulFix(font->face->ascender, metrics->y_scale) >> 6);//metrics->ascender >> 6;
	extents->descent		=-(float)(FT_MulFix(font->face->descender, metrics->y_scale) >> 6);//metrics->descender >> 6;
	extents->height			= (float)(FT_MulFix(font->face->height, metrics->y_scale) >> 6);//metrics->height >> 6;
	extents->max_x_advance	= (float)(bbox->xMax >> 6);
	extents->max_y_advance	= (float)(bbox->yMax >> 6);
#else
	extents->ascent			= roundf (font->scale * ctx->currentFont->ascent);
	extents->descent		=-roundf (font->scale * ctx->currentFont->descent);
	extents->height			= roundf (font->scale * (ctx->currentFont->ascent - ctx->currentFont->descent + ctx->currentFont->lineGap));
	extents->max_x_advance	= 0;//TODO
	extents->max_y_advance	= 0;
#endif
}
//compute text extends for provided string.
void _font_cache_text_extents (VkgContext ctx, const char* text, int length, vkg_text_extents *extents) {
	if (text == NULL) {
		memset(extents, 0, sizeof(vkg_text_extents));
		return;
	}

	vkg_text_run tr = {0};
	_font_cache_create_text_run (ctx, text, length, &tr);

	if (ctx->status)
		return;

	*extents = tr.extents;

	_font_cache_destroy_text_run (&tr);
}
//text is expected as utf8 encoded
//if length is < 0, text must be null terminated, else it contains glyph count
void _font_cache_create_text_run (VkgContext ctx, const char* text, int length, VkgText textRun) {

	_update_current_font (ctx);

	if (ctx->status)
		return;

	textRun->fontId = ctx->currentFont;
	textRun->font = ctx->currentFontSize;
	textRun->dev = ctx->dev;

	io_sync(ctx->dev->fontCache);

#ifdef VKVG_USE_HARFBUZZ
	textRun->hbBuf = _get_hb_buffer (ctx->currentFontSize, text,  length);
	textRun->glyphs = hb_buffer_get_glyph_positions	 (textRun->hbBuf, &textRun->glyph_count);
#else

	size_t wsize;
	if (length < 0)
		wsize = mbstowcs(NULL, text, 0);
	else
		wsize = (size_t)length;
	wchar_t *tmp = (wchar_t*)malloc((wsize+1) * sizeof (wchar_t));
	textRun->glyph_count = mbstowcs (tmp, text, wsize);
	textRun->glyphs = (vkg_glyph_info_t*)malloc(textRun->glyph_count * sizeof (vkg_glyph_info_t));
	for (unsigned int i=0; i<textRun->glyph_count; i++) {
#ifdef VKVG_USE_FREETYPE
		uint32_t gindex = FT_Get_Char_Index (textRun->font->face, tmp[i]);
#else
		uint32_t gindex = stbtt_FindGlyphIndex (&textRun->fontId->stbInfo, tmp[i]);
#endif
		_char_ref* cr = textRun->font->charLookup[gindex];
		if (cr==NULL)
			cr = _prepare_char (textRun->dev, textRun, gindex);
		textRun->glyphs[i].codepoint = gindex;
		textRun->glyphs[i].x_advance = cr->advance.x;
		textRun->glyphs[i].y_advance = cr->advance.y;
		textRun->glyphs[i].x_offset	 = 0;
		textRun->glyphs[i].y_offset	 = 0;
		/*textRun->glyphs[i].x_offset	 = cr->bmpDiff.x;
		textRun->glyphs[i].y_offset	 = cr->bmpDiff.y;*/
	}
	free (tmp);
#endif
	
	io_unsync(ctx->dev->fontCache);

	unsigned int string_width_in_pixels = 0;
	for (uint32_t i=0; i < textRun->glyph_count; ++i)
		string_width_in_pixels += textRun->glyphs[i].x_advance >> 6;
#ifdef VKVG_USE_FREETYPE
	FT_Size_Metrics* metrics = &ctx->currentFontSize->face->size->metrics;
	textRun->extents.height = (float)(FT_MulFix(ctx->currentFontSize->face->height, metrics->y_scale) >> 6);// (metrics->ascender + metrics->descender) >> 6;
#else
	textRun->extents.height = textRun->font->ascent - textRun->font->descent + textRun->font->lineGap;
#endif
	textRun->extents.x_advance = (float)string_width_in_pixels;
	if (textRun->glyph_count > 0) {
		textRun->extents.y_advance = (float)(textRun->glyphs[textRun->glyph_count-1].y_advance >> 6);
		textRun->extents.x_bearing = -(float)(textRun->glyphs[0].x_offset >> 6);
		textRun->extents.y_bearing = -(float)(textRun->glyphs[0].y_offset >> 6);
	}

	textRun->extents.width	= textRun->extents.x_advance;
}
void _font_cache_destroy_text_run (VkgText textRun) {
#ifdef VKVG_USE_HARFBUZZ
	hb_buffer_destroy(textRun->hbBuf);
#else
	if (textRun->glyph_count > 0)
		free (textRun->glyphs);
#endif
}
#ifdef DEBUG
void _show_texture (vkg_context* ctx){
	Vertex vs[] = {
		{{0,0},							  0,  {0,0,0}},
		{{0,FONT_PAGE_SIZE},			  0,  {0,1,0}},
		{{FONT_PAGE_SIZE,0},			  0,  {1,0,0}},
		{{FONT_PAGE_SIZE,FONT_PAGE_SIZE}, 0,  {1,1,0}}
	};

	VKVG_IBO_INDEX_TYPE firstIdx = (VKVG_IBO_INDEX_TYPE)(ctx->vertCount - ctx->curVertOffset);
	Vertex* pVert = &ctx->vertexCache[ctx->vertCount];
	memcpy (pVert,vs,4*sizeof(Vertex));
	ctx->vertCount+=4;

	check_vertex_cache_size(ctx);

	add_tri_indices_for_rect(ctx, firstIdx);
}
#endif
void _font_cache_show_text_run (VkgContext ctx, VkgText tr) {
	unsigned int glyph_count;
#ifdef VKVG_USE_HARFBUZZ
	hb_glyph_info_t* glyph_info = hb_buffer_get_glyph_infos (tr->hbBuf, &glyph_count);
#else
	vkg_glyph_info_t* glyph_info = tr->glyphs;
	glyph_count = tr->glyph_count;
#endif

	Vertex v = {{0},ctx->curColor,{0,0,-1}};
	vec2f pen = {0,0};

	if (!current_path_is_empty(ctx))
		pen = get_current_position(ctx);

	LOCK_FONTCACHE (ctx->dev)

	for (uint32_t i=0; i < glyph_count; ++i) {
		_char_ref* cr = tr->font->charLookup[glyph_info[i].codepoint];

#ifdef VKVG_USE_HARFBUZZ
		if (cr==NULL)
			cr = _prepare_char(tr->dev, tr, glyph_info[i].codepoint);
#endif

		float uvWidth	= cr->bounds.width  / (float)FONT_PAGE_SIZE;
		float uvHeight	= cr->bounds.height / (float)FONT_PAGE_SIZE;
		vec2f p0 = {pen.x + cr->bmpDiff.x + (tr->glyphs[i].x_offset >> 6),
				   pen.y - cr->bmpDiff.y + (tr->glyphs[i].y_offset >> 6)};
		v.pos = p0;

		VKVG_IBO_INDEX_TYPE firstIdx = (VKVG_IBO_INDEX_TYPE)(ctx->vertCount - ctx->curVertOffset);


		v.uv.x = cr->bounds.x;
		v.uv.y = cr->bounds.y;
		v.uv.z = cr->pageIdx;
		add_vertex(ctx,v);

		v.pos.y += cr->bounds.height;
		v.uv.y += uvHeight;
		add_vertex(ctx,v);

		v.pos.x += cr->bounds.width;
		v.pos.y = p0.y;
		v.uv.x += uvWidth;
		v.uv.y = cr->bounds.y;
		add_vertex(ctx,v);

		v.pos.y += cr->bounds.height;
		v.uv.y += uvHeight;
		add_vertex(ctx,v);

		add_tri_indices_for_rect (ctx, firstIdx);

		pen.x += (tr->glyphs[i].x_advance >> 6);
		pen.y -= (tr->glyphs[i].y_advance >> 6);
	}

	//equivalent to a moveto
	finish_path(ctx);
	add_point (ctx, pen.x, pen.y);
	_flush_chars_to_tex(tr->dev, tr->font);	
	UNLOCK_FONTCACHE (ctx->dev)

	if (ctx->fontCacheImg != ctx->dev->fontCache->texture) {
		vkg_flush (ctx);
		_font_cache_update_context_descset (ctx);
	}
}

void _font_cache_show_text (VkgContext ctx, const char* text){

	vkg_text_run tr = {0};
	_font_cache_create_text_run (ctx, text, -1, &tr);

	if (ctx->status)
		return;

	_font_cache_show_text_run (ctx, &tr);

	_font_cache_destroy_text_run (&tr);

	//_show_texture(ctx); return;
}


/*void testfonts(){
	FT_Library		library;
	FT_Face			face;
	FT_GlyphSlot	slot;

	assert(!FT_Init_FreeType(&library));
	assert(!FT_New_Face(library, "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 0, &face));
	assert(!FT_Set_Char_Size(face, 0, ptSize, device_hdpi, device_vdpi ));

	//_build_face_tex(face);

	hb_font_t *hb_font = hb_ft_font_create(face, NULL);
	hb_buffer *buf = hb_buffer_create();

	const char *text = "Ленивый рыжий кот";
	const char *lng	 = "en";
	//"كسول الزنجبيل القط","懶惰的姜貓",


	hb_buffer_set_direction (buf, HB_DIRECTION_LTR);
	hb_buffer_set_script	(buf, HB_SCRIPT_LATIN);
	hb_buffer_set_language	(buf, hb_language_from_string(lng,strlen(lng)));
	hb_buffer_add_utf8		(buf, text, strlen(text), 0, strlen(text));

	hb_unicode_funcs_t * unifc = hb_unicode_funcs_get_default();
	hb_script_t sc = hb_buffer_get_script(buf);

	sc = hb_unicode_script(unifc,0x0260);

	FT_CharMap* cm = face->charmap;

	//hb_script_to_iso15924_tag()


	FT_Done_Face	( face );
	FT_Done_FreeType( library );
}*/
