
#include <vkg/internal.hh>

//credits for bezier algorithms to:
//		Anti-Grain Geometry (AGG) - Version 2.5
//		A high quality rendering engine for C++
//		Copyright (C) 2002-2006 Maxim Shemanarev
//		Contact: mcseem@antigrain.com
//				 mcseemagg@yahoo.com
//				 http://antigrain.com

#include <vke/vke.hh>

#ifdef VKVG_FILL_NZ_GLUTESS
#include <glutess/glutess.h>
#endif

void VkgContext::resize_vertex_cache (uint32_t newSize) {
	Vertex* tmp = (Vertex*) realloc (data->vertexCache, (size_t)newSize * sizeof(Vertex));
	vke_log(VKE_LOG_DBG_ARRAYS, "resize vertex cache (vx count=%u): old size: %u -> new size: %u size(byte): %zu Ptr: %p -> %p\n",
		data->vertCount, data->sizeVertices, newSize, (size_t)newSize * sizeof(Vertex), data->vertexCache, tmp);
	if (tmp == NULL){
		data->status = VKE_STATUS_NO_MEMORY;
		vke_log(VKE_LOG_ERR, "resize vertex cache failed: vert count: %u byte size: %zu\n", newSize, newSize * sizeof(Vertex));
		return;
	}
	data->vertexCache = tmp;
	data->sizeVertices = newSize;
}

void VkgContext::resize_index_cache (uint32_t newSize) {
	VKVG_IBO_INDEX_TYPE* tmp = (VKVG_IBO_INDEX_TYPE*) realloc (data->indexCache, (size_t)newSize * sizeof(VKVG_IBO_INDEX_TYPE));
	vke_log(VKE_LOG_DBG_ARRAYS, "resize IBO: new size: %lu Ptr: %p -> %p\n", (size_t)newSize * sizeof(VKVG_IBO_INDEX_TYPE), data->indexCache, tmp);
	if (tmp == NULL){
		data->status = VKE_STATUS_NO_MEMORY;
		vke_log(VKE_LOG_ERR, "resize IBO failed: idx count: %u size(byte): %zu\n", newSize, (size_t)newSize * sizeof(VKVG_IBO_INDEX_TYPE));
		return;
	}
	data->indexCache = tmp;
	data->sizeIndices = newSize;
}

void VkgContext::ensure_vertex_cache_size (uint32_t addedVerticesCount) {
	if (data->sizeVertices - data->vertCount > VKVG_ARRAY_THRESHOLD + addedVerticesCount)
		return;
	uint32_t newSize = data->sizeVertices + addedVerticesCount;
	uint32_t modulo = addedVerticesCount % VKVG_VBO_SIZE;
	if (modulo > 0)
		newSize += VKVG_VBO_SIZE - modulo;
	_resize_vertex_cache (newSize);
}

void VkgContext::check_vertex_cache_size () {
	assert(ctx->sizeVertices > ctx->vertCount);
	if (ctx->sizeVertices - VKVG_ARRAY_THRESHOLD > ctx->vertCount)
		return;
	_resize_vertex_cache (ctx->sizeVertices + VKVG_VBO_SIZE);
}

void VkgContext::ensure_index_cache_size (uint32_t addedIndicesCount) {
	assert(ctx->sizeIndices > ctx->indCount);
	if (ctx->sizeIndices - VKVG_ARRAY_THRESHOLD > ctx->indCount + addedIndicesCount)
		return;
	uint32_t newSize = ctx->sizeIndices + addedIndicesCount;
	uint32_t modulo = addedIndicesCount % VKVG_IBO_SIZE;
	if (modulo > 0)
		newSize += VKVG_IBO_SIZE - modulo;
	_resize_index_cache (newSize);
}

void VkgContext::check_index_cache_size () {
	if (ctx->sizeIndices - VKVG_ARRAY_THRESHOLD > ctx->indCount)
		return;
	_resize_index_cache (ctx->sizeIndices + VKVG_IBO_SIZE);
}

//check host path array size, return true if error. pathPtr is already incremented
bool VkgContext::check_pathes_array (){
	if (ctx->sizePathes - ctx->pathPtr - ctx->segmentPtr > VKVG_ARRAY_THRESHOLD)
		return false;
	ctx->sizePathes += VKVG_PATHES_SIZE;
	uint32_t* tmp = (uint32_t*) realloc (ctx->pathes, (size_t)ctx->sizePathes * sizeof(uint32_t));
	vke_log(VKE_LOG_DBG_ARRAYS, "resize PATH: new size: %u Ptr: %p -> %p\n", ctx->sizePathes, ctx->pathes, tmp);
	if (tmp == NULL){
		ctx->status = VKE_STATUS_NO_MEMORY;
		vke_log(VKE_LOG_ERR, "resize PATH failed: new size(byte): %zu\n", ctx->sizePathes * sizeof(uint32_t));
		_clear_path(ctx);
		return true;
	}
	ctx->pathes = tmp;
	return false;
}

//check host point array size, return true if error
bool VkgContext::check_point_array (){
	if (ctx->sizePoints - VKVG_ARRAY_THRESHOLD > ctx->pointCount)
		return false;
	ctx->sizePoints += VKVG_PTS_SIZE;
	vec2f* tmp = (vec2f*) realloc (ctx->points, (size_t)ctx->sizePoints * sizeof(vec2f));
	vke_log(VKE_LOG_DBG_ARRAYS, "resize Points: new size(point): %u Ptr: %p -> %p\n", ctx->sizePoints, ctx->points, tmp);
	if (tmp == NULL){
		ctx->status = VKE_STATUS_NO_MEMORY;
		vke_log(VKE_LOG_ERR, "resize PATH failed: new size(byte): %zu\n", ctx->sizePoints * sizeof(vec2f));
		_clear_path (ctx);
		return true;
	}
	ctx->points = tmp;
	return false;
}

bool VkgContext::current_path_is_empty () {
	return ctx->pathes [ctx->pathPtr] == 0;
}

//this function expect that current point exists
vec2f* VkgContext::get_current_position () {
	return &ctx->points[ctx->pointCount-1];
}

//set curve start point and set path has curve bit
void VkgContext::set_curve_start () {
	if (ctx->segmentPtr > 0) {
		//check if current segment has points (straight)
		if ((ctx->pathes [ctx->pathPtr + ctx->segmentPtr]&PATH_ELT_MASK) > 0)
			ctx->segmentPtr++;
	} else {
		//not yet segmented path, first segment length is copied
		if (ctx->pathes [ctx->pathPtr] > 0){//create first straight segment first
			ctx->pathes [ctx->pathPtr + 1] = ctx->pathes [ctx->pathPtr];
			ctx->segmentPtr = 2;
		}else
			ctx->segmentPtr = 1;
	}
	_check_pathes_array(ctx);
	ctx->pathes [ctx->pathPtr + ctx->segmentPtr] = 0;
}

//compute segment length and set is curved bit
void VkgContext::set_curve_end () {
	//ctx->pathes [ctx->pathPtr + ctx->segmentPtr] = ctx->pathes [ctx->pathPtr] - ctx->pathes [ctx->pathPtr + ctx->segmentPtr];
	ctx->pathes [ctx->pathPtr + ctx->segmentPtr] |= PATH_HAS_CURVES_BIT;
	ctx->segmentPtr++;
	_check_pathes_array(ctx);
	ctx->pathes [ctx->pathPtr + ctx->segmentPtr] = 0;
}

//path start pointed at ptrPath has curve bit
bool VkgContext::path_has_curves (uint32_t ptrPath) {
	return ctx->pathes[ptrPath] & PATH_HAS_CURVES_BIT;
}

void VkgContext::finish_path (){
	if (ctx->pathes [ctx->pathPtr] == 0)//empty
		return;
	if ((ctx->pathes [ctx->pathPtr]&PATH_ELT_MASK) < 2){
		//only current pos is in path
		ctx->pointCount -= ctx->pathes[ctx->pathPtr];//what about the bounds?
		ctx->pathes[ctx->pathPtr] = 0;
		ctx->segmentPtr = 0;
		return;
	}

	vke_log(VKE_LOG_INFO_PATH, "PATH: points count=%10d\n", ctx->pathes[ctx->pathPtr]&PATH_ELT_MASK);

	if (ctx->pathPtr == 0 && ctx->simpleConvex)
		ctx->pathes[0] |= PATH_IS_CONVEX_BIT;

	if (ctx->segmentPtr > 0) {//pathes having curves are segmented
		ctx->pathes[ctx->pathPtr] |= PATH_HAS_CURVES_BIT;
		//curved segment increment segmentPtr on curve end,
		//so if last segment is not a curve and point count > 0
		if ((ctx->pathes[ctx->pathPtr+ctx->segmentPtr]&PATH_HAS_CURVES_BIT)==0 &&
				(ctx->pathes[ctx->pathPtr+ctx->segmentPtr]&PATH_ELT_MASK) > 0)
			ctx->segmentPtr++;//current segment has to be included
		ctx->pathPtr += ctx->segmentPtr;
	}else
		ctx->pathPtr ++;

	if (_check_pathes_array(ctx))
		return;

	ctx->pathes[ctx->pathPtr] = 0;
	ctx->segmentPtr = 0;
	ctx->subpathCount++;
	ctx->simpleConvex = false;
}

//clear path datas in context
void VkgContext::clear_path (){
	ctx->pathPtr = 0;
	ctx->pathes [ctx->pathPtr] = 0;
	ctx->pointCount = 0;
	ctx->segmentPtr = 0;
	ctx->subpathCount = 0;
	ctx->simpleConvex = false;
}

void VkgContext::remove_last_point (){
	ctx->pathes[ctx->pathPtr]--;
	ctx->pointCount--;
	if (ctx->segmentPtr > 0){//if path is segmented
		if (!ctx->pathes [ctx->pathPtr + ctx->segmentPtr])//if current segment is empty
			ctx->segmentPtr--;
		ctx->pathes [ctx->pathPtr + ctx->segmentPtr]--;//decrement last segment point count
		if ((ctx->pathes [ctx->pathPtr + ctx->segmentPtr]&PATH_ELT_MASK) == 0)//if no point left (was only one)
			ctx->pathes [ctx->pathPtr + ctx->segmentPtr] = 0;//reset current segment
		else if (ctx->pathes [ctx->pathPtr + ctx->segmentPtr]&PATH_HAS_CURVES_BIT)//if segment is a curve
			ctx->segmentPtr++;//then segPtr has to be forwarded to new segment
	}
}

bool VkgContext::path_is_closed (uint32_t ptrPath){
	return ctx->pathes[ptrPath] & PATH_CLOSED_BIT;
}

void VkgContext::add_point (float x, float y){
	if (check_point_array(ctx))
		return;
	if (isnan(x) || isnan(y)) {
		vke_log(VKE_LOG_DEBUG, "add_point: (%f, %f)\n", x, y);
		return;
	}
	vec2f v = {x,y};
	/*if (!current_path_is_empty(ctx) && vec2f_len(vec2f_sub(ctx->points[ctx->pointCount-1], v))<1.f)
		return;*/
	vke_log(VKE_LOG_INFO_PTS, "add_point: (%f, %f)\n", x, y);

	ctx->points[ctx->pointCount] = v;
	ctx->pointCount++;//total point count of pathes, (for array bounds check)
	ctx->pathes[ctx->pathPtr]++;//total point count in path
	if (ctx->segmentPtr > 0)
		ctx->pathes[ctx->pathPtr + ctx->segmentPtr]++;//total point count in path's segment
}

float _normalizeAngle(float a)
{
	float res = ROUND_DOWN(fmodf(a, 2.0f * M_PIF), 100);
	if (res < 0.0f)
		res += 2.0f * M_PIF;
	return res;
}

float VkgContext::get_arc_step (float radius) {
	float sx, sy;
	vkg_matrix_get_scale (&ctx->pushConsts.mat, &sx, &sy);
	float r = radius * fabsf(fmaxf(sx,sy));
	if (r < 30.0f)
		return fminf(M_PIF / 3.f, M_PIF / r);
	return fminf(M_PIF / 3.f,M_PIF / (r * 0.4f));
}

void VkgContext::create_gradient_buff (){
	vke_buffer_init (ctx->dev->vke,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VKE_MEMORY_USAGE_CPU_TO_GPU,
		sizeof(vkg_gradient), &ctx->uboGrad, true);
}

void VkgContext::create_vertices_buff (){
	vke_buffer_init (ctx->dev->vke,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VKE_MEMORY_USAGE_CPU_TO_GPU,
		ctx->sizeVBO * sizeof(Vertex), &ctx->vertices, true);
	vke_buffer_init (ctx->dev->vke,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VKE_MEMORY_USAGE_CPU_TO_GPU,
		ctx->sizeIBO * sizeof(VKVG_IBO_INDEX_TYPE), &ctx->indices, true);
}

void VkgContext::resize_vbo (uint32_t new_size) {
	if (!_wait_ctx_flush_end (ctx))//wait previous cmd if not completed
		return;
	vke_log(VKE_LOG_DBG_ARRAYS, "resize VBO: %d -> ", ctx->sizeVBO);
	ctx->sizeVBO = new_size;
	uint32_t mod = ctx->sizeVBO % VKVG_VBO_SIZE;
	if (mod > 0)
		ctx->sizeVBO += VKVG_VBO_SIZE - mod;
	vke_log(VKE_LOG_DBG_ARRAYS, "%d\n", ctx->sizeVBO);
	vke_buffer_resize (&ctx->vertices, ctx->sizeVBO * sizeof(Vertex), true);
}

void VkgContext::resize_ibo (size_t new_size) {
	if (!_wait_ctx_flush_end (ctx))//wait previous cmd if not completed
		return;
	ctx->sizeIBO = new_size;
	uint32_t mod = ctx->sizeIBO % VKVG_IBO_SIZE;
	if (mod > 0)
		ctx->sizeIBO += VKVG_IBO_SIZE - mod;
	vke_log(VKE_LOG_DBG_ARRAYS, "resize IBO: new size: %d\n", ctx->sizeIBO);	
	vke_buffer_resize (&ctx->indices, ctx->sizeIBO * sizeof(VKVG_IBO_INDEX_TYPE), true);
}

void VkgContext::add_vertexf (float x, float y){
	Vertex* pVert = &ctx->vertexCache[ctx->vertCount];
	pVert->pos[X] = x;
	pVert->pos[Y] = y;
	pVert->color = ctx->curColor;
	pVert->uv[Z] = -1;
	vke_log(VKE_LOG_INFO_VBO, "Add Vertexf %10d: pos:(%10.4f, %10.4f) uv:(%10.4f,%10.4f,%10.4f) color:0x%.8x \n", ctx->vertCount, pVert->pos[X], pVert->pos[Y], pVert->uv[X], pVert->uv[Y], pVert->uv[Z], pVert->color);
	ctx->vertCount++;
	_check_vertex_cache_size(ctx);
}

void VkgContext::add_vertexf_unchecked (float x, float y){
	Vertex* pVert = &ctx->vertexCache[ctx->vertCount];
	pVert->pos[X] = x;
	pVert->pos[Y] = y;
	pVert->color = ctx->curColor;
	pVert->uv[Z] = -1;
	vke_log(VKE_LOG_INFO_VBO, "Add Vertexf %10d: pos:(%10.4f, %10.4f) uv:(%10.4f,%10.4f,%10.4f) color:0x%.8x \n", ctx->vertCount, pVert->pos[X], pVert->pos[Y], pVert->uv[X], pVert->uv[Y], pVert->uv[Z], pVert->color);
	ctx->vertCount++;
}

void VkgContext::add_vertex(Vertex v){
	ctx->vertexCache[ctx->vertCount] = v;
	vke_log(VKE_LOG_INFO_VBO, "Add Vertex  %10d: pos:(%10.4f, %10.4f) uv:(%10.4f,%10.4f,%10.4f) color:0x%.8x \n", ctx->vertCount, v.pos[X], v.pos[Y], v.uv[X], v.uv[Y], v.uv[Z], v.color);
	ctx->vertCount++;
	_check_vertex_cache_size(ctx);
}

void VkgContext::set_vertex(uint32_t idx, Vertex v){
	ctx->vertexCache[idx] = v;
}

#ifdef VKVG_FILL_NZ_GLUTESS
void add_indice (VKVG_IBO_INDEX_TYPE i) {
	ctx->indexCache[ctx->indCount++] = i;
	_check_index_cache_size(ctx);
}

void _add_indice_for_fan (VKVG_IBO_INDEX_TYPE i) {
	VKVG_IBO_INDEX_TYPE* inds = &ctx->indexCache[ctx->indCount];
	inds[0] = ctx->tesselator_fan_start;
	inds[1] = ctx->indexCache[ctx->indCount-1];
	inds[2] = i;
	ctx->indCount+=3;
	_check_index_cache_size(ctx);
}

void add_indice_for_strip (VKVG_IBO_INDEX_TYPE i, bool odd) {
	VKVG_IBO_INDEX_TYPE* inds = &ctx->indexCache[ctx->indCount];
	if (odd) {
		inds[0] = ctx->indexCache[ctx->indCount-2];
		inds[1] = i;
		inds[2] = ctx->indexCache[ctx->indCount-1];
	} else {
		inds[0] = ctx->indexCache[ctx->indCount-1];
		inds[1] = ctx->indexCache[ctx->indCount-2];
		inds[2] = i;
	}
	ctx->indCount+=3;
	_check_index_cache_size(ctx);
}
#endif

void VkgContext::add_tri_indices_for_rect (VKVG_IBO_INDEX_TYPE i){
	VKVG_IBO_INDEX_TYPE* inds = &ctx->indexCache[ctx->indCount];
	inds[0] = i;
	inds[1] = i+2;
	inds[2] = i+1;
	inds[3] = i+1;
	inds[4] = i+2;
	inds[5] = i+3;
	ctx->indCount+=6;

	_check_index_cache_size(ctx);
	vke_log(VKE_LOG_INFO_IBO, "Rectangle IDX: %d %d %d | %d %d %d (count=%d)\n", inds[0], inds[1], inds[2], inds[3], inds[4], inds[5], ctx->indCount);
}

void VkgContext::add_triangle_indices(VKVG_IBO_INDEX_TYPE i0, VKVG_IBO_INDEX_TYPE i1, VKVG_IBO_INDEX_TYPE i2){
	VKVG_IBO_INDEX_TYPE* inds = &ctx->indexCache[ctx->indCount];
	inds[0] = i0;
	inds[1] = i1;
	inds[2] = i2;
	ctx->indCount+=3;

	_check_index_cache_size(ctx);
	vke_log(VKE_LOG_INFO_IBO, "Triangle IDX: %d %d %d (indCount=%d)\n", i0,i1,i2,ctx->indCount);
}

void VkgContext::add_triangle_indices_unchecked (VKVG_IBO_INDEX_TYPE i0, VKVG_IBO_INDEX_TYPE i1, VKVG_IBO_INDEX_TYPE i2){
	VKVG_IBO_INDEX_TYPE* inds = &ctx->indexCache[ctx->indCount];
	inds[0] = i0;
	inds[1] = i1;
	inds[2] = i2;
	ctx->indCount+=3;

	vke_log(VKE_LOG_INFO_IBO, "Triangle IDX: %d %d %d (indCount=%d)\n", i0,i1,i2,ctx->indCount);
}

void VkgContext::vao_add_rectangle (float x, float y, float width, float height){
	Vertex v[4] =
	{
		{{x,y},				ctx->curColor, {0,0,-1}},
		{{x,y+height},		ctx->curColor, {0,0,-1}},
		{{x+width,y},		ctx->curColor, {0,0,-1}},
		{{x+width,y+height},ctx->curColor, {0,0,-1}}
	};
	VKVG_IBO_INDEX_TYPE firstIdx = (VKVG_IBO_INDEX_TYPE)(ctx->vertCount - ctx->curVertOffset);
	Vertex* pVert = &ctx->vertexCache[ctx->vertCount];
	memcpy (pVert,v,4*sizeof(Vertex));
	ctx->vertCount+=4;

	_check_vertex_cache_size(ctx);

	add_tri_indices_for_rect(firstIdx);
}

//start render pass if not yet started or update push const if requested
void VkgContext::ensure_renderpass_is_started () {
	vke_log(VKE_LOG_INFO, "ensure_renderpass_is_started\n");
	if (!ctx->cmdStarted)
		_start_cmd_for_render_pass(ctx);
	else if (ctx->pushCstDirty)
		_update_push_constants(ctx);
}

void VkgContext::create_cmd_buff (){
	vke_cmd_buffs_create(ctx->dev->vke, ctx->cmdPool,VK_COMMAND_BUFFER_LEVEL_PRIMARY, 2, ctx->cmdBuffers);
#if defined(DEBUG) && defined(ENABLE_VALIDATION)
	vke_device_set_object_name(ctx->pSurf->dev->vke, VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT, (uint64_t)ctx->cmd, "vkvgCtxCmd");
#endif
}

void VkgContext::clear_attachment () {
}

bool VkgContext::wait_ctx_flush_end () {
	vke_log(VKE_LOG_INFO, "CTX: _wait_flush_fence\n");
#ifdef VKVG_ENABLE_VK_TIMELINE_SEMAPHORE
	if (vke_timeline_wait (ctx->dev->vke, ctx->pSurf->timeline, ctx->timelineStep) == VK_SUCCESS)
		return true;
#else
	if (WaitForFences (ctx->dev->vke->vkdev, 1, &ctx->flushFence, VK_TRUE, VKVG_FENCE_TIMEOUT) == VK_SUCCESS)
		return true;
#endif
	vke_log(VKE_LOG_DEBUG, "CTX: _wait_flush_fence timeout\n");
	ctx->status = VKE_STATUS_TIMEOUT;
	return false;
}

bool VkgContext::wait_and_submit_cmd (){
	if (!ctx->cmdStarted)//current cmd buff is empty, be aware that wait is also canceled!!
		return true;

	vke_log(VKE_LOG_INFO, "CTX: _wait_and_submit_cmd\n");

#ifdef VKVG_ENABLE_VK_TIMELINE_SEMAPHORE
	VkgSurface surf = ctx->pSurf;
	VkgDevice dev = surf->dev;
	//vke_timeline_wait (dev->vke, surf->timeline, ct->timelineStep);
	if (ctx->pattern && ctx->pattern->type == VKVG_PATTERN_TYPE_SURFACE) {
		//add source surface timeline sync.
		VkgSurface source = (VkgSurface)ctx->pattern->data;

		io_sync(surf);
		io_sync(source);
		io_sync(dev);

		vke_cmd_submit_timelined2 (dev->gQueue, &ctx->cmd,
								  (VkSemaphore[2]){surf->timeline,source->timeline},
								  (uint64_t[2]){surf->timelineStep,source->timelineStep},
								  (uint64_t[2]){surf->timelineStep+1,source->timelineStep+1});
		surf->timelineStep++;
		source->timelineStep++;
		ctx->timelineStep = surf->timelineStep;
		mtx_unlock(&dev->mutex);

		io_unsync(source);
		io_unsync(surf);
	} else {
		io_sync(surf);
		io_sync(dev);
		vke_cmd_submit_timelined (dev->gQueue, &ctx->cmd, surf->timeline, surf->timelineStep, surf->timelineStep+1);
		surf->timelineStep++;
		ctx->timelineStep = surf->timelineStep;
		io_unsync(dev);
		io_unsync(surf);
	}
#else

	if (!_wait_ctx_flush_end (ctx))
		return false;
	ResetFences (ctx->dev->vke->vkdev, 1, &ctx->flushFence);
	_device_submit_cmd (ctx->dev, &ctx->cmd, ctx->flushFence);
#endif

	if (ctx->cmd == ctx->cmdBuffers[0])
		ctx->cmd = ctx->cmdBuffers[1];
	else
		ctx->cmd = ctx->cmdBuffers[0];

	ResetCommandBuffer (ctx->cmd, 0);
	ctx->cmdStarted = false;
	return true;
}

/*void _explicit_ms_resolve (){//should init cmd before calling this (unused, using automatic resolve by renderpass)
	vke_image_set_layout (ctx->cmd, ctx->pSurf->imgMS, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	vke_image_set_layout (ctx->cmd, ctx->pSurf->img, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	VkImageResolve re = {
		.extent = {ctx->pSurf->width, ctx->pSurf->height,1},
		.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},
		.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}
	};

	vkCmdResolveImage(ctx->cmd,
					  vke_image_get_vkimage (ctx->pSurf->imgMS), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					  vke_image_get_vkimage (ctx->pSurf->img) ,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					  1,&re);
	vke_image_set_layout (ctx->cmd, ctx->pSurf->imgMS, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ,
						  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}*/

//pre flush vertices because of vbo or ibo too small, all vertices except last draw call are flushed
//this function expects a vertex offset > 0
void VkgContext::flush_vertices_caches_until_vertex_base () {
	_wait_ctx_flush_end (ctx);

	memcpy(vke_buffer_get_mapped_pointer(&ctx->vertices), ctx->vertexCache, ctx->curVertOffset * sizeof (Vertex));
	memcpy(vke_buffer_get_mapped_pointer(&ctx->indices), ctx->indexCache, ctx->curIndStart * sizeof (VKVG_IBO_INDEX_TYPE));

	//copy remaining vertices and indices to caches starts
	//this could be optimized at the cost of additional offsets.
	ctx->vertCount -= ctx->curVertOffset;
	ctx->indCount -= ctx->curIndStart;
	memcpy(ctx->vertexCache, &ctx->vertexCache[ctx->curVertOffset], ctx->vertCount * sizeof (Vertex));
	memcpy(ctx->indexCache, &ctx->indexCache[ctx->curIndStart], ctx->indCount * sizeof (VKVG_IBO_INDEX_TYPE));

	ctx->curVertOffset = 0;
	ctx->curIndStart = 0;
}

//copy vertex and index caches to the vbo and ibo vkbuffers used by gpu for drawing
//current running cmd has to be completed to free usage of those
void VkgContext::flush_vertices_caches () {
	if (!_wait_ctx_flush_end (ctx))
		return;

	memcpy(vke_buffer_get_mapped_pointer(&ctx->vertices), ctx->vertexCache, ctx->vertCount * sizeof (Vertex));
	memcpy(vke_buffer_get_mapped_pointer(&ctx->indices), ctx->indexCache, ctx->indCount * sizeof (VKVG_IBO_INDEX_TYPE));

	ctx->vertCount = ctx->indCount = ctx->curIndStart = ctx->curVertOffset = 0;
}

//this func expect cmdStarted to be true
void VkgContext::end_render_pass () {
	vke_log(VKE_LOG_INFO, "END RENDER PASS: ctx = %p;\n", ctx);
	CmdEndRenderPass	  (ctx->cmd);
#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	vke_cmd_label_end (ctx->cmd);
#endif
	ctx->renderPassBeginInfo.renderPass = ctx->dev->renderPass;
}

void VkgContext::check_vao_size () {
	if (ctx->vertCount > ctx->sizeVBO || ctx->indCount > ctx->sizeIBO){
		//vbo or ibo buffers too small
		if (ctx->cmdStarted)
			//if cmd is started buffers, are already bound, so no resize is possible
			//instead we flush, and clear vbo and ibo caches
			_flush_cmd_until_vx_base (ctx);
		if (ctx->vertCount > ctx->sizeVBO)		
			_resize_vbo(ctx->sizeVertices);
		if (ctx->indCount > ctx->sizeIBO)
			_resize_ibo(ctx->sizeIndices);
	}
}

//stroke and non-zero draw call for solid color flush
void VkgContext::emit_draw_cmd_undrawn_vertices (){
	if (ctx->indCount == ctx->curIndStart)
		return;

	_check_vao_size (ctx);

	ensure_renderpass_is_started (ctx);

#ifdef VKVG_WIRED_DEBUG
	if (vkg_wired_debug&vkg_wired_debug_mode_normal)
		CmdDrawIndexed(ctx->cmd, ctx->indCount - ctx->curIndStart, 1, ctx->curIndStart, (int32_t)ctx->curVertOffset, 0);
	if (vkg_wired_debug&vkg_wired_debug_mode_lines) {
		CmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pSurf->dev->pipelineLineList);
		CmdDrawIndexed(ctx->cmd, ctx->indCount - ctx->curIndStart, 1, ctx->curIndStart, (int32_t)ctx->curVertOffset, 0);
	}
	if (vkg_wired_debug&vkg_wired_debug_mode_points) {
		CmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pSurf->dev->pipelineWired);
		CmdDrawIndexed(ctx->cmd, ctx->indCount - ctx->curIndStart, 1, ctx->curIndStart, (int32_t)ctx->curVertOffset, 0);
	}
	if (vkg_wired_debug&vkg_wired_debug_mode_both)
		CmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pSurf->dev->pipe_OVER);
#else
	CmdDrawIndexed(ctx->cmd, ctx->indCount - ctx->curIndStart, 1, ctx->curIndStart, (int32_t)ctx->curVertOffset, 0);
#endif
	vke_log(VKE_LOG_INFO, "RECORD DRAW CMD: ctx = %p; vertices = %d; indices = %d (vxOff = %d idxStart = %d idxTot = %d )\n",
		ctx, ctx->vertCount - ctx->curVertOffset,
		ctx->indCount - ctx->curIndStart, ctx->curVertOffset, ctx->curIndStart, ctx->indCount);

	ctx->curIndStart = ctx->indCount;
	ctx->curVertOffset = ctx->vertCount;
}
//preflush vertices with drawcommand already emited
void VkgContext::flush_cmd_until_vx_base (){
	_end_render_pass (ctx);
	if (ctx->curVertOffset > 0){
		vke_log(VKE_LOG_INFO, "FLUSH UNTIL VX BASE CTX: ctx = %p; vertices = %d; indices = %d\n", ctx, ctx->vertCount, ctx->indCount);
		_flush_vertices_caches_until_vertex_base (ctx);
	}
	vke_cmd_end (ctx->cmd);
	_wait_and_submit_cmd (ctx);
}
void VkgContext::flush_cmd_buff (){
	emit_draw_cmd_undrawn_vertices (ctx);
	if (!ctx->cmdStarted)
		return;
	_end_render_pass		(ctx);
	vke_log(VKE_LOG_INFO, "FLUSH CTX: ctx = %p; vertices = %d; indices = %d\n", ctx, ctx->vertCount, ctx->indCount);
	_flush_vertices_caches	(ctx);
	vke_cmd_end				(ctx->cmd);

	_wait_and_submit_cmd	(ctx);
}

//bind correct draw pipeline depending on current OPERATOR
void VkgContext::bind_draw_pipeline () {
	switch (ctx->curOperator) {
	case VKVG_OPERATOR_OVER:
		CmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->dev->pipe_OVER);
		break;
	case VKVG_OPERATOR_CLEAR:
		CmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->dev->pipe_CLEAR);
		break;
	case VKVG_OPERATOR_DIFFERENCE:
		CmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->dev->pipe_SUB);
		break;
	default:
		CmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->dev->pipe_OVER);
		break;
	}
}
#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
const float DBG_LAB_COLOR_RP[4]		= {0,0,1,1};
const float DBG_LAB_COLOR_FSQ[4]	= {1,0,0,1};
#endif

void VkgContext::start_cmd_for_render_pass () {
	vke_log(VKE_LOG_INFO, "START RENDER PASS: ctx = %p\n", ctx);
	vke_cmd_begin (ctx->cmd,VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	/// ctx->dev->threadAware seems irrelevant to this op so i am removing
	if (ctx->pSurf->img->layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
		VkeImage imgMs = ctx->pSurf->imgMS;
		if (imgMs != NULL)
			vke_image_set_layout(ctx->cmd, imgMs, VK_IMAGE_ASPECT_COLOR_BIT,
								 VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
								 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		vke_image_set_layout(ctx->cmd, ctx->pSurf->img, VK_IMAGE_ASPECT_COLOR_BIT,
						 VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		vke_image_set_layout (ctx->cmd, ctx->pSurf->stencil, ctx->dev->stencilAspectFlag,
							  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
							  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
	}

#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	vke_cmd_label_start(ctx->cmd, "ctx render pass", DBG_LAB_COLOR_RP);
#endif

	CmdBeginRenderPass (ctx->cmd, &ctx->renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	VkViewport viewport = {0,0,(float)ctx->pSurf->width,(float)ctx->pSurf->height,0,1.f};
	CmdSetViewport(ctx->cmd, 0, 1, &viewport);

	CmdSetScissor(ctx->cmd, 0, 1, &ctx->bounds);

	VkDescriptorSet dss[] = {ctx->dsFont, ctx->dsSrc,ctx->dsGrad};
	CmdBindDescriptorSets(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->dev->pipelineLayout,
							0, 3, dss, 0, NULL);

	VkDeviceSize offsets[1] = { 0 };
	CmdBindVertexBuffers(ctx->cmd, 0, 1, &ctx->vertices.buffer, offsets);
	CmdBindIndexBuffer(ctx->cmd, ctx->indices.buffer, 0, VKVG_VK_INDEX_TYPE);

	_update_push_constants	(ctx);

	bind_draw_pipeline (ctx);
	CmdSetStencilCompareMask(ctx->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_CLIP_BIT);
	ctx->cmdStarted = true;
}
//compute inverse mat used in shader when context matrix has changed
//then trigger push constants command
void VkgContext::set_mat_inv_and_vkCmdPush () {
	ctx->pushConsts.matInv = ctx->pushConsts.mat;
	vkg_matrix_invert (&ctx->pushConsts.matInv);
	ctx->pushCstDirty = true;
}
void VkgContext::update_push_constants () {
	CmdPushConstants(ctx->cmd, ctx->dev->pipelineLayout,
					   VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constants),&ctx->pushConsts);
	ctx->pushCstDirty = false;
}
void VkgContext::update_cur_pattern (VkgPattern pat) {
	VkgPattern lastPat = ctx->pattern;
	ctx->pattern = pat;

	uint32_t newPatternType = VKVG_PATTERN_TYPE_SOLID;

	vke_log(VKE_LOG_INFO, "CTX: _update_cur_pattern: %p -> %p\n", lastPat, pat);

	if (pat == NULL) {//solid color
		if (lastPat == NULL)//solid
			return;//solid to solid transition, no extra action requested
	}else
		newPatternType = pat->type;

	switch (newPatternType)	 {
	case VKVG_PATTERN_TYPE_SOLID:
		_flush_cmd_buff				(ctx);
		if (!_wait_ctx_flush_end (ctx))
			return;
		if (lastPat->type == VKVG_PATTERN_TYPE_SURFACE)//unbind current source surface by replacing it with empty texture
			_update_descriptor_set		(ctx->dev->emptyImg, ctx->dsSrc);
		break;
	case VKVG_PATTERN_TYPE_SURFACE:
	{
		emit_draw_cmd_undrawn_vertices(ctx);

		VkgSurface surf = (VkgSurface)pat->data;

		//flush ctx in two steps to add the src transitioning in the cmd buff
		if (ctx->cmdStarted){//transition of img without appropriate dependencies in subpass must be done outside renderpass.
			_end_render_pass (ctx);
			_flush_vertices_caches (ctx);
		} else {
			vke_cmd_begin (ctx->cmd,VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
			ctx->cmdStarted = true;
		}

		//transition source surface for sampling
		vke_image_set_layout (ctx->cmd, surf->img, VK_IMAGE_ASPECT_COLOR_BIT,
							  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
							  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

		vke_cmd_end				(ctx->cmd);
		_wait_and_submit_cmd	(ctx);
		if (!_wait_ctx_flush_end (ctx))
			return;

		VkSamplerAddressMode addrMode = 0;
		VkFilter filter = VK_FILTER_NEAREST;
		switch (pat->extend) {
		case VKVG_EXTEND_NONE:
			addrMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			break;
		case VKVG_EXTEND_PAD:
			addrMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			break;
		case VKVG_EXTEND_REPEAT:
			addrMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			break;
		case VKVG_EXTEND_REFLECT:
			addrMode = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			break;
		}
		switch (pat->filter) {
		case VKVG_FILTER_BILINEAR:
		case VKVG_FILTER_BEST:
			filter = VK_FILTER_LINEAR;
			break;
		default:
			filter = VK_FILTER_NEAREST;
			break;
		}
		vke_image_create_sampler (surf->img, filter, filter,
									VK_SAMPLER_MIPMAP_MODE_NEAREST, addrMode);

		_update_descriptor_set (surf->img, ctx->dsSrc);

		if (pat->hasMatrix) {

		}

		ctx->pushConsts.source.width	= (float)surf->width;
		ctx->pushConsts.source.height	= (float)surf->height;
		break;
	}
	case VKVG_PATTERN_TYPE_LINEAR:
	case VKVG_PATTERN_TYPE_RADIAL:
		_flush_cmd_buff (ctx);
		if (!_wait_ctx_flush_end (ctx))
			return;

		if (lastPat && lastPat->type == VKVG_PATTERN_TYPE_SURFACE)
			_update_descriptor_set (ctx->dev->emptyImg, ctx->dsSrc);

		vec4 bounds = {{(float)ctx->pSurf->width}, {(float)ctx->pSurf->height}, {0}, {0}};//store img bounds in unused source field
		ctx->pushConsts.source = bounds;

		//transform control point with current ctx matrix
		vkg_gradient grad = *(vkg_gradient*)pat->data;

		if (grad.count < 2) {
			ctx->status = VKE_STATUS_PATTERN_INVALID_GRADIENT;
			return;
		}
		vkg_matrix mat;
		if (pat->hasMatrix) {
			VkgPattern::get_matrix (pat, &mat);
			if (vkg_matrix_invert (&mat) != VKE_STATUS_SUCCESS)
				mat = VKVG_IDENTITY_MATRIX;
		}

		if (pat->hasMatrix)
			vkg_matrix_transform_point (&mat, &grad.cp[0][X], &grad.cp[0][Y]);
		vkg_matrix_transform_point (&ctx->pushConsts.mat, &grad.cp[0][X], &grad.cp[0][Y]);
		if (pat->type == VKVG_PATTERN_TYPE_LINEAR) {
			if (pat->hasMatrix)
				vkg_matrix_transform_point (&mat, &grad.cp[0][Z], &grad.cp[0][W]);
			vkg_matrix_transform_point (&ctx->pushConsts.mat, &grad.cp[0][Z], &grad.cp[0][W]);
		} else {
			if (pat->hasMatrix)
				vkg_matrix_transform_point (&mat, &grad.cp[1][X], &grad.cp[1][Y]);
			vkg_matrix_transform_point (&ctx->pushConsts.mat, &grad.cp[1][X], &grad.cp[1][Y]);

			//radii
			if (pat->hasMatrix) {
				vkg_matrix_transform_distance (&mat, &grad.cp[0][Z], &grad.cp[0][W]);
				vkg_matrix_transform_distance (&mat, &grad.cp[1][Z], &grad.cp[0][W]);
			}
			vkg_matrix_transform_distance (&ctx->pushConsts.mat, &grad.cp[0][Z], &grad.cp[0][W]);
			vkg_matrix_transform_distance (&ctx->pushConsts.mat, &grad.cp[1][Z], &grad.cp[0][W]);
		}

		memcpy (vke_buffer_get_mapped_pointer(&ctx->uboGrad) , &grad, sizeof(vkg_gradient));
		vke_buffer_flush(&ctx->uboGrad);
		break;
	}
	ctx->pushConsts.fsq_patternType = (ctx->pushConsts.fsq_patternType & FULLSCREEN_BIT) + newPatternType;
	ctx->pushCstDirty = true;
	if (lastPat)
		VkgPattern::drop(lastPat);
}
void VkgContext::update_descriptor_set (VkeImage img, VkDescriptorSet ds){
	VkDescriptorImageInfo descSrcTex = vke_image_get_descriptor (img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkWriteDescriptorSet writeDescriptorSet = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = ds,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &descSrcTex
	};
	vkUpdateDescriptorSets(ctx->dev->vke->vkdev, 1, &writeDescriptorSet, 0, NULL);
}

void VkgContext::update_gradient_desc_set (){
	VkDescriptorBufferInfo dbi = {ctx->uboGrad.buffer, 0, VK_WHOLE_SIZE};
	VkWriteDescriptorSet writeDescriptorSet = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = ctx->dsGrad,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &dbi
	};
	vkUpdateDescriptorSets(ctx->dev->vke->vkdev, 1, &writeDescriptorSet, 0, NULL);
}
/*
 * Reset currently bound descriptor which image could be destroyed
 */
/*void _reset_src_descriptor_set (){
	VkgDevice dev = ctx->pSurf->dev;
	//VkDescriptorSet dss[] = {ctx->dsSrc};
	vkFreeDescriptorSets	(dev->vke->vkdev, ctx->descriptorPool, 1, &ctx->dsSrc);

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
															  .descriptorPool = ctx->descriptorPool,
															  .descriptorSetCount = 1,
															  .pSetLayouts = &dev->dslSrc };
	VK_CHECK_RESULT(vkAllocateDescriptorSets(dev->vke->vkdev, &descriptorSetAllocateInfo, &ctx->dsSrc));
}*/

void VkgContext::createDescriptorPool () {
	VkgDevice dev = ctx->dev;
	const VkDescriptorPoolSize descriptorPoolSize[] = {
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 },
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
	};
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = 3,
		.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		.poolSizeCount = 2,
		.pPoolSizes = descriptorPoolSize
	};
	VK_CHECK_RESULT(vkCreateDescriptorPool (dev->vke->vkdev, &descriptorPoolCreateInfo, NULL, &ctx->descriptorPool));
}
void VkgContext::init_descriptor_sets (){
	VkgDevice dev = ctx->dev;
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
															  .descriptorPool = ctx->descriptorPool,
															  .descriptorSetCount = 1,
															  .pSetLayouts = &dev->dslFont
															};
	VK_CHECK_RESULT(vkAllocateDescriptorSets(dev->vke->vkdev, &descriptorSetAllocateInfo, &ctx->dsFont));
	descriptorSetAllocateInfo.pSetLayouts = &dev->dslSrc;
	VK_CHECK_RESULT(vkAllocateDescriptorSets(dev->vke->vkdev, &descriptorSetAllocateInfo, &ctx->dsSrc));
	descriptorSetAllocateInfo.pSetLayouts = &dev->dslGrad;
	VK_CHECK_RESULT(vkAllocateDescriptorSets(dev->vke->vkdev, &descriptorSetAllocateInfo, &ctx->dsGrad));
}
void VkgContext::release_context_ressources () {
	VkDevice dev = ctx->dev->vke->vkdev;
	
#ifndef VKVG_ENABLE_VK_TIMELINE_SEMAPHORE
	vkDestroyFence (dev, ctx->flushFence, NULL);
#endif

	vkFreeCommandBuffers(dev, ctx->cmdPool, 2, ctx->cmdBuffers);
	vkDestroyCommandPool(dev, ctx->cmdPool, NULL);

	VkDescriptorSet dss[] = {ctx->dsFont, ctx->dsSrc, ctx->dsGrad};
	vkFreeDescriptorSets	(dev, ctx->descriptorPool, 3, dss);

	vkDestroyDescriptorPool (dev, ctx->descriptorPool,NULL);

	vke_buffer_reset (&ctx->uboGrad);
	vke_buffer_reset (&ctx->indices);
	vke_buffer_reset (&ctx->vertices);

	free(ctx->vertexCache);
	free(ctx->indexCache);

	vke_image_drop	  (ctx->fontCacheImg);
	//TODO:check this for source counter
	//vke_image_drop	  (ctx->source);

	free(ctx->pathes);
	free(ctx->points);

	free(ctx);
}
//populate vertice buff for stroke
bool VkgContext::build_vb_step(stroke_context* str, bool isCurve){
	Vertex v = {{0},ctx->curColor, {0,0,-1}};
	vec2f p0 = ctx->points[str->cp];
	vec2f v0 = vec2f_sub(p0, ctx->points[str->iL]);
	vec2f v1 = vec2f_sub(ctx->points[str->iR], p0);
	float length_v0 = vec2f_len(v0);
	float length_v1 = vec2f_len(v1);
	if (length_v0 < FLT_EPSILON || length_v1 < FLT_EPSILON) {
		vke_log(VKE_LOG_STROKE, "vb_step discard, length<epsilon: l0:%f l1:%f\n", length_v0, length_v1);
		return false;
	}
	vec2f v0n = vec2f_div_s (v0, length_v0);
	vec2f v1n = vec2f_div_s (v1, length_v1);
	float dot = vec2f_dot (v0n, v1n);
	float det = v0n[X] * v1n[Y] - v0n[Y] * v1n[X];
	if (EQUF(dot,1.0f)) {//colinear
		vke_log(VKE_LOG_STROKE, "vb_step discard, dot==1\n");
		return false;
	}

	if (EQUF(dot,-1.0f)) {//cusp (could draw line butt?)
		vec2f vPerp = vec2f_scale(vec2f_perp (v0n), str->hw);

		VKVG_IBO_INDEX_TYPE idx = (VKVG_IBO_INDEX_TYPE)(ctx->vertCount - ctx->curVertOffset);
	
		vec2f_add(v.pos, p0, vPerp);
		add_vertex(v);

		vec2f_sub(v.pos, p0, vPerp); // seriously whack but i dont believe its dangerous
		add_vertex(v);

		add_triangle_indices(idx, idx+1, idx+2);
		add_triangle_indices(idx, idx+2, idx+3);
		vke_log(VKE_LOG_STROKE, "vb_step cusp, dot==-1\n");
		return true;
	}

	vec2f bisec_n;
	vec2f n;
	vec2f_add (n, v0n, v1n)
	vec2f_norm(bisec_n, n);//bisec/bisec_perp are inverted names

	float alpha = acosf(dot);

	if (det < 0) alpha = -alpha;

	float halfAlpha    = alpha / 2.f;
	float cosHalfAlpha = cosf(halfAlpha);
	float lh 		   = str->hw / cosHalfAlpha;
	vec2f bisec_n_perp = vec2f_perp(bisec_n);

	//limit bisectrice length
	float rlh = lh;//rlh is for inside pos tweeks
	if (dot < 0.f)
		rlh = fminf (rlh, fminf (length_v0, length_v1));
	//---

	vec2f bisec;
	vec2f_scale (bisec, bisec_n_perp, rlh);

	VKVG_IBO_INDEX_TYPE idx = (VKVG_IBO_INDEX_TYPE)(ctx->vertCount - ctx->curVertOffset);

	vec2f rlh_inside_pos, rlh_outside_pos;
	if (rlh < lh) {
		vec2f vnPerp;
		if (length_v0 < length_v1)
			vec2f_perp (vnPerp, v1n);
		else
			vec2f_perp (vnPerp, v0n);
		vec2f vHwPerp;
		vec2f_scale (vHwPerp, vnPerp, str->hw);

		double lbc = cosHalfAlpha * rlh;
		if (det < 0.f) {
			vec2f ms;
			vec2f_scale (ms, vnPerp, -lbc);

			vec2f a;
			vec2f_add    (a, p0, bisec);

			vec2f ms_a;
			vec2f_add    (ms_a, ms, a);
			vec2f_add    (rlh_inside_pos, ms_a, vHwPerp);

			vec2f ms2;
			vec2f_scale (ms2, bisec_n_perp, lh)
			vec2f_sub 	 (rlh_outside_pos, p0, ms2);
		} else {
			vec2f m_lbc;
			vec2f_scale (m_lbc, vnPerp, lbc);

			vec2f s;
			vec2f_sub	 (s, p0, bisec);

			vec2f mlbc_s;
			vec2f_add 	 (mlbc_s, m_lbc, s);
			vec2f_sub 	 (rlh_inside_pos, mlbc_s, vHwPerp);

			vec2f ph;
			vec2f_scale (ph, bisec_n_perp, lh)
			vec2f_add 	 (rlh_outside_pos, p0, ph);
		}
	} else {
		if (det < 0.0) {
			vec2f_add    (rlh_inside_pos,  p0, bisec);
			vec2f_sub    (rlh_outside_pos, p0, bisec);
		} else {
			vec2f_sub    (rlh_inside_pos,  p0, bisec);
			vec2f_add    (rlh_outside_pos, p0, bisec);
		}
	}

	vkg_line_join join = ctx->lineJoin;

	if (isCurve) {
		if (dot < 0.8f)
			join = VKVG_LINE_JOIN_ROUND;
		else
			join = VKVG_LINE_JOIN_MITER;
	}

	if (join == VKVG_LINE_JOIN_MITER){
		if (lh > str->lhMax) {//miter limit
			double x = (lh - str->lhMax) * cosHalfAlpha;
			vec2f bisecPerp = vec2f_scale (bisec_n, x);
			bisec = vec2f_scale (bisec_n_perp, str->lhMax);
			if (det < 0) {
				v.pos = rlh_inside_pos;
				add_vertex(v);

				vec2f p;
				vec2f_sub(p, p0, bisec);

				vec2f_sub(v.pos, p, bisecPerp);
				add_vertex(v);
				vec2f_add(v.pos, p, bisecPerp);
				add_vertex(v);

				add_triangle_indices(idx, idx+2, idx+1);
				add_triangle_indices(idx+2, idx+4, idx);
				add_triangle_indices(idx, idx+3, idx+4);
				return true;
			} else {
				vec2f p;
				vec2f_add(p, p0, bisec);
				vec2f_sub(v.pos, p, bisecPerp);
				add_vertex(v);

				v.pos = rlh_inside_pos;
				add_vertex(v);

				vec2f_add(v.pos, p, bisecPerp);
				add_vertex(v);

				add_triangle_indices(idx, idx+2, idx+1);
				add_triangle_indices(idx+2, idx+3, idx+1);
				add_triangle_indices(idx+1, idx+3, idx+4);
				return false;
			}

		} else {//normal miter
			if (det < 0) {
				v.pos = rlh_inside_pos;
				add_vertex(v);

				v.pos = rlh_outside_pos;
				add_vertex(v);
			} else {
				v.pos = rlh_outside_pos;
				add_vertex(v);

				v.pos = rlh_inside_pos;
				add_vertex(v);
			}
			add_tri_indices_for_rect(idx);
			return false;
		}
	} else {
		vec2f vp = vec2f_perp(v0n);

		if (det<0){
			if (dot < 0 && rlh < lh)
				v.pos = rlh_inside_pos;
			else
				vec2f_add(v.pos, p0, bisec);
			///
			add_vertex	 (v);
			vec2f ms;
			vec2f_scale (ms, vp, str->hw)
			vec2f_sub 	 (v.pos, p0, ms);
		} else {
			vec2f ms;
			vec2f_scale (ms, vp, str->hw);
			vec2f_add 	 (v.pos, p0, ms);
			add_vertex  (v);
			if (dot < 0 && rlh < lh)
				v.pos = rlh_inside_pos;
			else
				v.pos = vec2f_sub (p0, bisec);
		}
		add_vertex(v);

		if (join == VKVG_LINE_JOIN_BEVEL){
			if (det<0){
				add_triangle_indices(idx, idx+2, idx+1);
				add_triangle_indices(idx+2, idx+4, idx+0);
				add_triangle_indices(idx, idx+3, idx+4);
			} else {
				add_triangle_indices(idx, idx+2, idx+1);
				add_triangle_indices(idx+2, idx+3, idx+1);
				add_triangle_indices(idx+1, idx+3, idx+4);
			}
		}else if (join == VKVG_LINE_JOIN_ROUND){
			if (!str->arcStep)
				str->arcStep = _get_arc_step (str->hw);
			float a = acosf(vp[X]);
			if (vp[Y] < 0)
				a = -a;

			if (det<0){
				a+=M_PIF;
				float a1 = a + alpha;
				a-=str->arcStep;
				while (a > a1){
					add_vertexf(cosf(a) * str->hw + p0[X], sinf(a) * str->hw + p0[Y]);
					a-=str->arcStep;
				}
			} else {
				float a1 = a + alpha;
				a+=str->arcStep;
				while (a < a1){
					add_vertexf(cosf(a) * str->hw + p0[X], sinf(a) * str->hw + p0[Y]);
					a+=str->arcStep;
				}
			}
			VKVG_IBO_INDEX_TYPE p0Idx = (VKVG_IBO_INDEX_TYPE)(ctx->vertCount - ctx->curVertOffset);
			add_triangle_indices(idx, idx+2, idx+1);
			if (det < 0){
				for (VKVG_IBO_INDEX_TYPE p = idx+2; p < p0Idx; p++)
					add_triangle_indices(p, p+1, idx);
				add_triangle_indices(p0Idx, p0Idx+2, idx);
				add_triangle_indices(idx, p0Idx+1, p0Idx+2);
			} else {
				for (VKVG_IBO_INDEX_TYPE p = idx+2; p < p0Idx; p++)
					add_triangle_indices(p, p+1, idx+1);
				add_triangle_indices(p0Idx, p0Idx+1, idx+1);
				add_triangle_indices(idx+1, p0Idx+1, p0Idx+2);
			}
		}

		vec2f_scale (vp, vec2f_perp(v1n), str->hw);
		if (det < 0)
			vec2f_sub (v.pos, p0, vp);
		else
			vec2f_add (v.pos, p0, vp);
		add_vertex(v);
	}

/*
#ifdef DEBUG

	debugLinePoints[dlpCount] = p0;
	debugLinePoints[dlpCount+1] = _v2add(p0, _vec2dToVec2(_v2Multd(v0n,10)));
	dlpCount+=2;
	debugLinePoints[dlpCount] = p0;
	debugLinePoints[dlpCount+1] = _v2add(p0, _vec2dToVec2(_v2Multd(v1n,10)));
	dlpCount+=2;
	debugLinePoints[dlpCount] = p0;
	debugLinePoints[dlpCount+1] = pR;
	dlpCount+=2;
#endif*/
	/*if (reducedLH)
		return -det;
	else*/
	return (det < 0);
}

void VkgContext::draw_stoke_cap (stroke_context *str, vec2f p0, vec2f n, bool isStart) {
	Vertex v = {{0},ctx->curColor,{0,0,-1}};

	VKVG_IBO_INDEX_TYPE firstIdx = (VKVG_IBO_INDEX_TYPE)(ctx->vertCount - ctx->curVertOffset);

	if (isStart){
		vec2f vhw;
		vec2f_scale(vhw, n, str->hw);

		if (ctx->lineCap == VKVG_LINE_CAP_SQUARE)
			vec2f_sub(p0, p0, vhw);

		vec2f_perp(vhw, vhw);

		if (ctx->lineCap == VKVG_LINE_CAP_ROUND){
			if (!str->arcStep)
				str->arcStep = _get_arc_step (str->hw);

			float a = acosf(n[X]) + M_PIF_2;
			if (n[Y] < 0)
				a = M_PIF-a;
			float a1 = a + M_PIF;

			a += str->arcStep;
			while (a < a1){
				add_vertexf (cosf(a) * str->hw + p0[X], sinf(a) * str->hw + p0[Y]);
				a += str->arcStep;
			}
			VKVG_IBO_INDEX_TYPE p0Idx = (VKVG_IBO_INDEX_TYPE)(ctx->vertCount - ctx->curVertOffset);
			for (VKVG_IBO_INDEX_TYPE p = firstIdx; p < p0Idx; p++)
				add_triangle_indices (p0Idx+1, p, p+1);
			firstIdx = p0Idx;
		}

		v.pos = vec2f_add (p0, vhw);
		add_vertex (v);
		v.pos = vec2f_sub (p0, vhw);
		add_vertex (v);

		add_tri_indices_for_rect (firstIdx);
	} else {
		vec2f vhw = vec2f_scale (n, str->hw);

		if (ctx->lineCap == VKVG_LINE_CAP_SQUARE)
			p0 = vec2f_add (p0, vhw);

		vhw = vec2f_perp (vhw);

		v.pos = vec2f_add (p0, vhw);
		add_vertex (v);
		v.pos = vec2f_sub (p0, vhw);
		add_vertex (v);

		firstIdx = (VKVG_IBO_INDEX_TYPE)(ctx->vertCount - ctx->curVertOffset);

		if (ctx->lineCap == VKVG_LINE_CAP_ROUND){
			if (!str->arcStep)
				str->arcStep = _get_arc_step (str->hw);

			float a = acosf(n[X])+ M_PIF_2;
			if (n[Y] < 0)
				a = M_PIF-a;
			float a1 = a - M_PIF;

			a -= str->arcStep;
			while ( a > a1){
				add_vertexf (cosf(a) * str->hw + p0[X], sinf(a) * str->hw + p0[Y]);
				a -= str->arcStep;
			}

			VKVG_IBO_INDEX_TYPE p0Idx = (VKVG_IBO_INDEX_TYPE)(ctx->vertCount - ctx->curVertOffset - 1);
			for (VKVG_IBO_INDEX_TYPE p = firstIdx-1 ; p < p0Idx; p++)
				add_triangle_indices (p+1, p, firstIdx-2);
		}
	}
}
float VkgContext::draw_dashed_segment (stroke_context* str, dash_context_t* dc, bool isCurve) {
	//vec2f pL = ctx->points[str->iL];
	vec2f p = ctx->points[str->cp];
	vec2f pR = ctx->points[str->iR];

	if (!dc->dashOn)//we test in fact the next dash start, if dashOn = true => next segment is a void.
		build_vb_step (str, isCurve);

	vec2f d = vec2f_sub (pR, p);
	dc->normal = vec2f_norm (d);
	float segmentLength = vec2f_len(d);

	while (dc->curDashOffset < segmentLength){
		vec2f p0 = vec2f_add (p, vec2f_scale(dc->normal, dc->curDashOffset));

		_draw_stoke_cap (str, p0, dc->normal, dc->dashOn);
		dc->dashOn ^= true;
		dc->curDashOffset += ctx->dashes[dc->curDash];
		if (++dc->curDash == ctx->dashCount)
			dc->curDash = 0;
	}
	dc->curDashOffset -= segmentLength;
	dc->curDashOffset = fmodf(dc->curDashOffset, dc->totDashLength);
	return segmentLength;
}
void VkgContext::draw_segment (stroke_context* str, dash_context_t* dc, bool isCurve) {
	str->iR = str->cp + 1;
	if (ctx->dashCount > 0)
		draw_dashed_segment(str, dc, isCurve);
	else
		build_vb_step (str, isCurve);
	str->iL = str->cp++;
	if (ctx->vertCount - ctx->curVertOffset > VKVG_IBO_MAX / 3) {
		Vertex v0 = ctx->vertexCache[ctx->curVertOffset + str->firstIdx];
		Vertex v1 = ctx->vertexCache[ctx->curVertOffset + str->firstIdx + 1];
		emit_draw_cmd_undrawn_vertices(ctx);
		//repeat first 2 vertices for closed pathes
		str->firstIdx = (VKVG_IBO_INDEX_TYPE)(ctx->vertCount - ctx->curVertOffset);
		add_vertex(v0);
		add_vertex(v1);
		ctx->curVertOffset = ctx->vertCount;//prevent redrawing them at the start of the batch
	}
}

bool ptInTriangle(vec2f p, vec2f p0, vec2f p1, vec2f p2) {
	float dX = p[X]-p2[X];
	float dY = p[Y]-p2[Y];
	float dX21 = p2[X]-p1[X];
	float dY12 = p1[Y]-p2[Y];
	float D = dY12*(p0[X]-p2[X]) + dX21*(p0[Y]-p2[Y]);
	float s = dY12*dX + dX21*dY;
	float t = (p2[Y]-p0[Y])*dX + (p0[X]-p2[X])*dY;
	if (D<0)
		return (s<=0) && (t<=0) && (s+t>=D);
	return (s>=0) && (t>=0) && (s+t<=D);
}

void _free_ctx_save (vkg_context_save* sav){
	if (sav->dashCount > 0)
		free (sav->dashes);
	if (sav->pattern)
		VkgPattern::drop(sav->pattern);
	free (sav);
}


#define M_APPROXIMATION_SCALE	1.0
#define M_ANGLE_TOLERANCE		0.01
#define M_CUSP_LIMIT			0.01
#define CURVE_RECURSION_LIMIT	100
#define CURVE_COLLINEARITY_EPSILON 1.7
#define CURVE_ANGLE_TOLERANCE_EPSILON 0.001
//no floating point arithmetic operation allowed in macro.
#pragma warning(disable:4127)
static void recursive_bezier (
	float distanceTolerance,
		float x1, float y1, float x2, float y2,
		float x3, float y3, float x4, float y4,
		unsigned level) {
	if(level > CURVE_RECURSION_LIMIT)
	{
		return;
	}

	// Calculate all the mid-points of the line segments
	//----------------------
	float x12	= (x1 + x2) / 2;
	float y12	= (y1 + y2) / 2;
	float x23	= (x2 + x3) / 2;
	float y23	= (y2 + y3) / 2;
	float x34	= (x3 + x4) / 2;
	float y34	= (y3 + y4) / 2;
	float x123	= (x12 + x23) / 2;
	float y123	= (y12 + y23) / 2;
	float x234	= (x23 + x34) / 2;
	float y234	= (y23 + y34) / 2;
	float x1234 = (x123 + x234) / 2;
	float y1234 = (y123 + y234) / 2;

	if(level > 0) // Enforce subdivision first time
	{
		// Try to approximate the full cubic curve by a single straight line
		//------------------
		float dx = x4-x1;
		float dy = y4-y1;

		float d2 = fabsf(((x2 - x4) * dy - (y2 - y4) * dx));
		float d3 = fabsf(((x3 - x4) * dy - (y3 - y4) * dx));

		float da1, da2;

		if(d2 > CURVE_COLLINEARITY_EPSILON && d3 > CURVE_COLLINEARITY_EPSILON)
		{
			// Regular care
			//-----------------
			if((d2 + d3)*(d2 + d3) <= (dx*dx + dy*dy) * distanceTolerance)
			{
				// If the curvature doesn't exceed the distance_tolerance value
				// we tend to finish subdivisions.
				//----------------------
				if (M_ANGLE_TOLERANCE < CURVE_ANGLE_TOLERANCE_EPSILON) {
					add_point(x1234, y1234);
					return;
				}

				// Angle & Cusp Condition
				//----------------------
				float a23 = atan2f(y3 - y2, x3 - x2);
				da1 = fabsf(a23 - atan2f(y2 - y1, x2 - x1));
				da2 = fabsf(atan2f(y4 - y3, x4 - x3) - a23);
				if(da1 >= M_PIF) da1 = M_2_PIF - da1;
				if(da2 >= M_PIF) da2 = M_2_PIF - da2;

				if(da1 + da2 < (float)M_ANGLE_TOLERANCE)
				{
					// Finally we can stop the recursion
					//----------------------
					add_point (x1234, y1234);
					return;
				}

				if(M_CUSP_LIMIT != 0.0)
				{
					if(da1 > M_CUSP_LIMIT)
					{
						add_point (x2, y2);
						return;
					}

					if(da2 > M_CUSP_LIMIT)
					{
						add_point (x3, y3);
						return;
					}
				}
			}
		} else {
			if(d2 > CURVE_COLLINEARITY_EPSILON)
			{
				// p1,p3,p4 are collinear, p2 is considerable
				//----------------------
				if(d2 * d2 <= distanceTolerance * (dx*dx + dy*dy))
				{
					if(M_ANGLE_TOLERANCE < CURVE_ANGLE_TOLERANCE_EPSILON)
					{
						add_point (x1234, y1234);
						return;
					}

					// Angle Condition
					//----------------------
					da1 = fabsf(atan2f(y3 - y2, x3 - x2) - atan2f(y2 - y1, x2 - x1));
					if(da1 >= M_PIF) da1 = M_2_PIF - da1;

					if(da1 < M_ANGLE_TOLERANCE)
					{
						add_point (x2, y2);
						add_point (x3, y3);
						return;
					}

					if(M_CUSP_LIMIT != 0.0)
					{
						if(da1 > M_CUSP_LIMIT)
						{
							add_point (x2, y2);
							return;
						}
					}
				}
			} else if(d3 > CURVE_COLLINEARITY_EPSILON) {
				// p1,p2,p4 are collinear, p3 is considerable
				//----------------------
				if(d3 * d3 <= distanceTolerance* (dx*dx + dy*dy))
				{
					if(M_ANGLE_TOLERANCE < CURVE_ANGLE_TOLERANCE_EPSILON)
					{
						add_point (x1234, y1234);
						return;
					}

					// Angle Condition
					//----------------------
					da1 = fabsf(atan2f(y4 - y3, x4 - x3) - atan2f(y3 - y2, x3 - x2));
					if(da1 >= M_PIF) da1 = M_2_PIF - da1;

					if(da1 < M_ANGLE_TOLERANCE)
					{
						add_point (x2, y2);
						add_point (x3, y3);
						return;
					}

					if(M_CUSP_LIMIT != 0.0)
					{
						if(da1 > M_CUSP_LIMIT)
						{
							add_point (x3, y3);
							return;
						}
					}
				}
			}
			else
			{
				// Collinear case
				//-----------------
				dx = x1234 - (x1 + x4) / 2;
				dy = y1234 - (y1 + y4) / 2;
				if(dx*dx + dy*dy <= distanceTolerance)
				{
					add_point (x1234, y1234);
					return;
				}
			}
		}
	}

	// Continue subdivision
	//----------------------
	recursive_bezier (distanceTolerance, x1, y1, x12, y12, x123, y123, x1234, y1234, level + 1);
	recursive_bezier (distanceTolerance,x1234, y1234, x234, y234, x34, y34, x4, y4, level + 1);
}
#pragma warning(default:4127)
void VkgContext::line_to (float x, float y) {
	vec2f p = {x,y};
	if (!current_path_is_empty (ctx)){
		//prevent adding the same point
		if (vec2f_equ (_get_current_position (ctx), p))
			return;
	}
	add_point (x, y);
	ctx->simpleConvex = false;
}
void VkgContext::elliptic_arc (float x1, float y1, float x2, float y2, bool largeArc, bool counterClockWise, float _rx, float _ry, float phi) {
	if (ctx->status)
		return;

	if (_rx==0||_ry==0) {
		if (current_path_is_empty(ctx))
			move_to(x1, y1);
		line_to(x2, y2);
		return;
	}
	float rx = fabsf(_rx);
	float ry = fabsf(_ry);

	mat2 m = {
		{ cosf (phi), sinf (phi)},
		{-sinf (phi), cosf (phi)}
	};
	vec2f p  = {(x1 - x2)/2, (y1 - y2)/2};
	vec2f p1 = mat2_mult_vec2 (m, p);

	//radii corrections
	double lambda = powf (p1[X], 2) / powf (rx, 2) + powf (p1[Y], 2) / powf (ry, 2);
	if (lambda > 1) {
		lambda = sqrtf (lambda);
		rx *= lambda;
		ry *= lambda;
	}

	p = (vec2f){rx * p1[Y] / ry, -ry * p1[X] / rx};

	vec2f cp = vec2f_scale (p, sqrtf (fabsf (
		(powf (rx,2) * powf (ry,2) - powf (rx,2) * powf (p1[Y], 2) - powf (ry,2) * powf (p1[X], 2)) /
		(powf (rx,2) * powf (p1[Y], 2) + powf (ry,2) * powf (p1[X], 2))
	)));

	if (largeArc == counterClockWise)
		vec2f_inv(cp);

	m = (mat2) {
		{cosf (phi),-sinf (phi)},
		{sinf (phi), cosf (phi)}
	};
	p = (vec2f){(x1 + x2)/2, (y1 + y2)/2};
	vec2f c = vec2f_add (mat2_mult_vec2(m, cp) , p);

	vec2f u = vec2f_unit_x;
	vec2f v = {(p1[X]-cp[X])/rx, (p1[Y]-cp[Y])/ry};
	double sa = acosf (vec2f_dot (u, v) / (fabsf(vec2f_len(v)) * fabsf(vec2f_len(u))));
	if (isnan(sa))
		sa=M_PIF;
	if (u[X]*v[Y]-u[Y]*v[X] < 0)
		sa = -sa;

	u = v;
	v = (vec2f) {(-p1[X]-cp[X])/rx, (-p1[Y]-cp[Y])/ry};
	double delta_theta = acosf (vec2f_dot (u, v) / (fabsf(vec2f_len (v)) * fabsf(vec2f_len (u))));
	if (isnan(delta_theta))
		delta_theta=M_PIF;
	if (u[X]*v[Y]-u[Y]*v[X] < 0)
		delta_theta = -delta_theta;

	if (counterClockWise) {
		if (delta_theta < 0)
			delta_theta += M_PIF * 2.0;
	} else if (delta_theta > 0)
		delta_theta -= M_PIF * 2.0;

	m = (mat2) {
		{cosf (phi),-sinf (phi)},
		{sinf (phi), cosf (phi)}
	};

	double theta = sa;
	double ea = sa + delta_theta;

	float step = fmaxf(0.001f, fminf(M_PIF, _get_arc_step(fminf (rx, ry))*0.1f));

	p = (vec2f) {
		rx * cosf (theta),
		ry * sinf (theta)
	};
	vec2f xy = vec2f_add (mat2_mult_vec2 (m, p), c);

	if (current_path_is_empty(ctx)){
		set_curve_start (ctx);
		add_point (xy[X], xy[Y]);
		if (!ctx->pathPtr)
			ctx->simpleConvex = true;
		else
			ctx->simpleConvex = false;
	} else {
		line_to(xy[X], xy[Y]);
		set_curve_start (ctx);
		ctx->simpleConvex = false;
	}

	set_curve_start (ctx);

	if (sa < ea) {
		theta += step;
		while (theta < ea) {
			p = (vec2f) {
				rx * cosf (theta),
				ry * sinf (theta)
			};
			xy = vec2f_add (mat2_mult_vec2 (m, p), c);
			add_point (xy[X], xy[Y]);
			theta += step;
		}
	} else {
		theta -= step;
		while (theta > ea) {
			p = (vec2f) {
				rx * cosf (theta),
				ry * sinf (theta)
			};
			xy = vec2f_add (mat2_mult_vec2 (m, p), c);
			add_point (xy[X], xy[Y]);
			theta -= step;
		}
	}
	p = (vec2f) {
		rx * cosf (ea),
		ry * sinf (ea)
	};
	xy = vec2f_add (mat2_mult_vec2 (m, p), c);
	add_point (xy[X], xy[Y]);
	set_curve_end(ctx);
}


//Even-Odd inside test with stencil buffer implementation.
void VkgContext::poly_fill (vec4* bounds){
	//we anticipate the check for vbo buffer size, ibo is not used in poly_fill
	//the polyfill emit a single vertex for each point in the path.
	if (ctx->sizeVBO - VKVG_ARRAY_THRESHOLD <  ctx->vertCount + ctx->pointCount) {
		if (ctx->cmdStarted) {
			_end_render_pass (ctx);
			if (ctx->vertCount > 0)
				_flush_vertices_caches (ctx);
			vke_cmd_end (ctx->cmd);
			_wait_and_submit_cmd (ctx);
			_wait_ctx_flush_end (ctx);
			if (ctx->sizeVBO - VKVG_ARRAY_THRESHOLD < ctx->pointCount){
				_resize_vbo (ctx->pointCount + VKVG_ARRAY_THRESHOLD);
				_resize_vertex_cache (ctx->sizeVBO);
			}
		} else {
			_resize_vbo (ctx->vertCount + ctx->pointCount + VKVG_ARRAY_THRESHOLD);
			_resize_vertex_cache (ctx->sizeVBO);
		}

		_start_cmd_for_render_pass(ctx);
	} else {
		_ensure_vertex_cache_size (ctx->pointCount);
		ensure_renderpass_is_started (ctx);
	}

	CmdBindPipeline (ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->dev->pipelinePolyFill);

	Vertex v = {{0}, ctx->curColor, {0,0,-1}};
	uint32_t ptrPath = 0;
	uint32_t firstPtIdx = 0;

	while (ptrPath < ctx->pathPtr){
		uint32_t pathPointCount = ctx->pathes[ptrPath] & PATH_ELT_MASK;
		if (pathPointCount > 2) {
			VKVG_IBO_INDEX_TYPE firstVertIdx = (VKVG_IBO_INDEX_TYPE)ctx->vertCount;

			for (uint32_t i = 0; i < pathPointCount; i++) {
				v.pos = ctx->points [i+firstPtIdx];
				ctx->vertexCache[ctx->vertCount++] = v;
				if (!bounds)
					continue;
				//bounds are computed here to scissor the painting operation
				//that speed up fill drastically.
				vkg_matrix_transform_point (&ctx->pushConsts.mat, &v.pos[X], &v.pos[Y]);

				if (v.pos[X] < bounds->xMin)
					bounds->x_min = v.pos[X];
				if (v.pos[X] > bounds->x_max)
					bounds->x_max = v.pos[X];
				if (v.pos[Y] < bounds->y_min)
					bounds->y_min = v.pos[Y];
				if (v.pos[Y] > bounds->y_max)
					bounds->y_max = v.pos[Y];
			}

			vke_log(VKE_LOG_INFO_PATH, "\tpoly fill: point count = %d; 1st vert = %d; vert count = %d\n", pathPointCount, firstVertIdx, ctx->vertCount - firstVertIdx);
			CmdDraw (ctx->cmd, pathPointCount, 1, firstVertIdx , 0);
		}
		firstPtIdx += pathPointCount;

		if (path_has_curves (ptrPath)) {
			//skip segments lengths used in stroke
			ptrPath++;
			uint32_t totPts = 0;
			while (totPts < pathPointCount)
				totPts += (ctx->pathes[ptrPath++] & PATH_ELT_MASK);
		}else
			ptrPath++;
	}
	ctx->curVertOffset = ctx->vertCount;
}
#ifdef VKVG_FILL_NZ_GLUTESS
void fan_vertex2(VKVG_IBO_INDEX_TYPE v, VkgContext ctx) {
	VKVG_IBO_INDEX_TYPE i = (VKVG_IBO_INDEX_TYPE)v;
	switch (ctx->tesselator_idx_counter) {
	case 0:
		add_indice(i);
		ctx->tesselator_fan_start = i;
		ctx->tesselator_idx_counter ++;
		break;
	case 1:
	case 2:
		add_indice(i);
		ctx->tesselator_idx_counter ++;
		break;
	default:
		_add_indice_for_fan (i);
		break;
	}
}

void strip_vertex2(VKVG_IBO_INDEX_TYPE v, VkgContext ctx) {
	VKVG_IBO_INDEX_TYPE i = (VKVG_IBO_INDEX_TYPE)v;
	if (ctx->tesselator_idx_counter < 3) {
		add_indice(i);
	} else
		add_indice_for_strip(i, ctx->tesselator_idx_counter % 2);
	ctx->tesselator_idx_counter ++;
}

void triangle_vertex2 (VKVG_IBO_INDEX_TYPE v, VkgContext ctx) {
	VKVG_IBO_INDEX_TYPE i = (VKVG_IBO_INDEX_TYPE)v;
	add_indice(i);
}
void skip_vertex2 (VKVG_IBO_INDEX_TYPE v, VkgContext ctx) {}
void begin2(GLenum which, void *poly_data)
{
	VkgContext ctx = (VkgContext)poly_data;
	switch (which) {
	case GL_TRIANGLES:
		ctx->vertex_cb = &triangle_vertex2;
		break;
	case GL_TRIANGLE_STRIP:
		ctx->tesselator_idx_counter = 0;
		ctx->vertex_cb = &strip_vertex2;
		break;
	case GL_TRIANGLE_FAN:
		ctx->tesselator_idx_counter = ctx->tesselator_fan_start = 0;
		ctx->vertex_cb = &fan_vertex2;
		break;
	default:
		fprintf(stderr, "ERROR, can't handle %d\n", (int)which);
		ctx->vertex_cb = &skip_vertex2;
	}
}

void combine2(const GLdouble newVertex[3],
			 const void *neighborVertex_s[4],
			 const GLfloat neighborWeight[4], void **outData, void *poly_data)
{
	VkgContext ctx = (VkgContext)poly_data;
	Vertex v = {{newVertex[0],newVertex[1]},ctx->curColor, {0,0,-1}};
	*outData = (void*)((unsigned long)(ctx->vertCount - ctx->curVertOffset));
	add_vertex(v);
}


void vertex2(void *vertex_data, void *poly_data)
{
#ifndef _WIN32
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvoid-pointer-to-int-cast"
#endif
	VKVG_IBO_INDEX_TYPE i = (VKVG_IBO_INDEX_TYPE)vertex_data; // this is a bit suspect LOL.  // todo test.
#ifndef _WIN32
#pragma GCC diagnostic pop
#endif
	VkgContext ctx = (VkgContext)poly_data;
	ctx->vertex_cb(i, ctx);
}

void fill_non_zero (){
	Vertex v = {{0},ctx->curColor, {0,0,-1}};

	uint32_t ptrPath = 0;
	uint32_t firstPtIdx = 0;

	if (ctx->pathPtr == 1 && ctx->pathes[0] & PATH_IS_CONVEX_BIT) {
		//simple concave rectangle or circle
		VKVG_IBO_INDEX_TYPE firstVertIdx = (VKVG_IBO_INDEX_TYPE)(ctx->vertCount - ctx->curVertOffset);
		uint32_t pathPointCount = ctx->pathes[ptrPath] & PATH_ELT_MASK;

		_ensure_vertex_cache_size(pathPointCount);
		_ensure_index_cache_size((pathPointCount-2)*3);

		VKVG_IBO_INDEX_TYPE i = 0;
		while (i < 2){
			v.pos = ctx->points [i++];
			_set_vertex (ctx->vertCount++, v);
		}
		while (i < pathPointCount){
			v.pos = ctx->points [i];
			_set_vertex (ctx->vertCount++, v);
			_add_triangle_indices_unchecked(firstVertIdx, firstVertIdx + i - 1, firstVertIdx + i);
			i++;
		}
		return;
	}


	GLUtesselator *tess = gluNewTess();
	gluTessProperty(tess, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_NONZERO);
	gluTessCallback(tess, GLU_TESS_VERTEX_DATA,  (GLvoid (*) ()) &vertex2);
	gluTessCallback(tess, GLU_TESS_BEGIN_DATA,   (GLvoid (*) ()) &begin2);
	gluTessCallback(tess, GLU_TESS_COMBINE_DATA, (GLvoid (*) ()) &combine2);

	gluTessBeginPolygon(tess, ctx);

	while (ptrPath < ctx->pathPtr){
		uint32_t pathPointCount = ctx->pathes[ptrPath] & PATH_ELT_MASK;

		if (pathPointCount > 2) {
			VKVG_IBO_INDEX_TYPE firstVertIdx = (VKVG_IBO_INDEX_TYPE)(ctx->vertCount - ctx->curVertOffset);
			gluTessBeginContour(tess);

			VKVG_IBO_INDEX_TYPE i = 0;

			while (i < pathPointCount){
				v.pos = ctx->points[i+firstPtIdx];
				double dp[] = {v.pos[X],v.pos[Y],0};
				add_vertex(v);
				gluTessVertex(tess, dp, (void*)((unsigned long)firstVertIdx + i));
				i++;
			}
			gluTessEndContour(tess);

			//limit batch size here to 1/3 of the ibo index type ability
			//if (ctx->vertCount - ctx->curVertOffset > VKVG_IBO_MAX / 3)
			//	emit_draw_cmd_undrawn_vertices(ctx);
		}

		firstPtIdx += pathPointCount;
		if (path_has_curves (ptrPath)) {
			//skip segments lengths used in stroke
			ptrPath++;
			uint32_t totPts = 0;
			while (totPts < pathPointCount)
				totPts += (ctx->pathes[ptrPath++] & PATH_ELT_MASK);
		}else
			ptrPath++;
	}

	gluTessEndPolygon(tess);

	gluDeleteTess(tess);
}
#else
//create fill from current path with ear clipping technic
void VkgContext::fill_non_zero (){
	Vertex v = {{0},ctx->curColor, {0,0,-1}};

	uint32_t ptrPath = 0;
	uint32_t firstPtIdx = 0;

	while (ptrPath < ctx->pathPtr){
		uint32_t pathPointCount = ctx->pathes[ptrPath] & PATH_ELT_MASK;

		if (pathPointCount > 2) {
			VKVG_IBO_INDEX_TYPE firstVertIdx = (VKVG_IBO_INDEX_TYPE)(ctx->vertCount - ctx->curVertOffset);
			ear_clip_point* ecps = (ear_clip_point*)malloc(pathPointCount*sizeof(ear_clip_point));
			uint32_t ecps_count = pathPointCount;
			VKVG_IBO_INDEX_TYPE i = 0;

			//init points link list
			while (i < pathPointCount-1){
				v.pos = ctx->points[i+firstPtIdx];
				ear_clip_point ecp = {v.pos, firstVertIdx+i, &ecps[i+1]};
				ecps[i] = ecp;
				add_vertex(v);
				i++;
			}

			v.pos = ctx->points[i+firstPtIdx];
			ear_clip_point ecp = {v.pos, firstVertIdx+i, ecps};
			ecps[i] = ecp;
			add_vertex(v);

			ear_clip_point* ecp_current = ecps;
			uint32_t tries = 0;

			while (ecps_count > 3) {
				if (tries > ecps_count) {
					break;
				}
				ear_clip_point* v0 = ecp_current->next,
						*v1 = ecp_current, *v2 = ecp_current->next->next;
				if (ecp_zcross (v0, v2, v1)<0){
					ecp_current = ecp_current->next;
					tries++;
					continue;
				}
				ear_clip_point* vP = v2->next;
				bool isEar = true;
				while (vP!=v1){
					if (ptInTriangle (vP->pos, v0->pos, v2->pos, v1->pos)){
						isEar = false;
						break;
					}
					vP = vP->next;
				}
				if (isEar){
					add_triangle_indices (v0->idx, v1->idx, v2->idx);
					v1->next = v2;
					ecps_count --;
					tries = 0;
				} else {
					ecp_current = ecp_current->next;
					tries++;
				}
			}
			if (ecps_count == 3)
				add_triangle_indices(ecp_current->next->idx, ecp_current->idx, ecp_current->next->next->idx);
			free (ecps);

			//limit batch size here to 1/3 of the ibo index type ability
			if (ctx->vertCount - ctx->curVertOffset > VKVG_IBO_MAX / 3)
				emit_draw_cmd_undrawn_vertices(ctx);
		}

		firstPtIdx += pathPointCount;
		if (path_has_curves (ptrPath)) {
			//skip segments lengths used in stroke
			ptrPath++;
			uint32_t totPts = 0;
			while (totPts < pathPointCount)
				totPts += (ctx->pathes[ptrPath++] & PATH_ELT_MASK);
		}else
			ptrPath++;
	}
}
#endif

void VkgContext::path_extents (bool transformed, float *x1, float *y1, float *x2, float *y2) {
	uint32_t ptrPath    = 0;
	uint32_t firstPtIdx = 0;
	float    x_min = FLT_MAX, y_min = FLT_MAX;
	float    x_max = FLT_MIN, y_max = FLT_MIN;
	///
	while (ptrPath < ctx->pathPtr){
		uint32_t pathPointCount = ctx->pathes[ptrPath] & PATH_ELT_MASK;

		for (uint32_t i = firstPtIdx; i < firstPtIdx + pathPointCount; i++){
			vec2f p = ctx->points[i];
			if (transformed) vkg_matrix_transform_point (&ctx->pushConsts.mat, &p[X], &p[Y]);
			if (p[X] < x_min) x_min = p[X];
			if (p[X] > x_max) x_max = p[X];
			if (p[Y] < y_min) y_min = p[Y];
			if (p[Y] > y_max) y_max = p[Y];
		}

		firstPtIdx += pathPointCount;
		if (path_has_curves (ptrPath)) {
			//skip segments lengths used in stroke
			ptrPath++;
			uint32_t totPts = 0;
			while (totPts < pathPointCount)
				totPts += (ctx->pathes[ptrPath++] & PATH_ELT_MASK);
		}else
			ptrPath++;
	}
	///
	*x1 = x_min;
	*x2 = x_max;
	*y1 = y_min;
	*y2 = y_max;
}

void VkgContext::draw_full_screen_quad (vec4* scissor) {
#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	vke_cmd_label_start(ctx->cmd, "draw_full_screen_quad", DBG_LAB_COLOR_FSQ);
#endif
	if (scissor) {
		VkRect2D r = {
			{(int32_t)MAX(scissor->x_min, 0), (int32_t)MAX(scissor->y_min, 0)},
			{(int32_t)MAX(scissor->x_max - (int32_t)scissor->x_min + 1, 1), (int32_t)MAX(scissor->y_max - (int32_t)scissor->y_min + 1, 1)}
		};
		CmdSetScissor(ctx->cmd, 0, 1, &r);
	}

	uint32_t firstVertIdx = ctx->vertCount;
	_ensure_vertex_cache_size (3);

	_add_vertexf_unchecked (-1, -1);
	_add_vertexf_unchecked ( 3, -1);
	_add_vertexf_unchecked (-1,  3);

	ctx->curVertOffset = ctx->vertCount;

	ctx->pushConsts.fsq_patternType |= FULLSCREEN_BIT;
	CmdPushConstants(ctx->cmd, ctx->dev->pipelineLayout,
					   VK_SHADER_STAGE_VERTEX_BIT, 24, 4,&ctx->pushConsts.fsq_patternType);
	CmdDraw (ctx->cmd,3,1,firstVertIdx,0);
	ctx->pushConsts.fsq_patternType &= ~FULLSCREEN_BIT;
	CmdPushConstants(ctx->cmd, ctx->dev->pipelineLayout,
					   VK_SHADER_STAGE_VERTEX_BIT, 24, 4,&ctx->pushConsts.fsq_patternType);
	if (scissor)
		CmdSetScissor(ctx->cmd, 0, 1, &ctx->bounds);

#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	vke_cmd_label_end (ctx->cmd);
#endif
}

void VkgContext::select_font_face (const char* name){
	if (strcmp(ctx->selectedFontName, name) == 0)
		return;
	strcpy (ctx->selectedFontName, name);
	ctx->currentFont = NULL;
	ctx->currentFontSize = NULL;
}


#ifdef DEBUG
	static vec2f debugLinePoints[1000];
	static uint32_t dlpCount = 0;
	#if defined (VKVG_DBG_UTILS)
		const float DBG_LAB_COLOR_SAV[4]	= {1,0,1,1};
		const float DBG_LAB_COLOR_CLIP[4]	= {0,1,1,1};
	#endif
#endif

//todo:this could be used to define a default background
static VkClearValue clearValues[3] = {
	{ .color.float32 = {0,0,0,0} },
	{ .depthStencil  = {1.0f, 0} },
	{ .color.float32 = {0,0,0,0} }
};

io_implement(vkg_surface);

void VkgContext::init_ctx () {
	ctx->lineWidth		= 1;
	ctx->miterLimit		= 10;
	ctx->curOperator	= VKVG_OPERATOR_OVER;
	ctx->curFillRule	= VKVG_FILL_RULE_NON_ZERO;
	ctx->bounds = (VkRect2D) {{0,0},{ctx->pSurf->width,ctx->pSurf->height}};
	ctx->pushConsts = (push_constants) {
			{.a = 1},
			{(float)ctx->pSurf->width,(float)ctx->pSurf->height},
			VKVG_PATTERN_TYPE_SOLID,
			1.0f,
			VKVG_IDENTITY_MATRIX,
			VKVG_IDENTITY_MATRIX
	};
	ctx->clearRect = (VkClearRect) {{{0},{ctx->pSurf->width, ctx->pSurf->height}},0,1};
	ctx->renderPassBeginInfo.framebuffer = ctx->pSurf->fb;
	ctx->renderPassBeginInfo.renderArea.extent.width = ctx->pSurf->width;
	ctx->renderPassBeginInfo.renderArea.extent.height = ctx->pSurf->height;
	ctx->renderPassBeginInfo.pClearValues = clearValues;

	io_sync(ctx->pSurf);
	if (ctx->pSurf->newSurf)
		ctx->renderPassBeginInfo.renderPass = ctx->dev->renderPass_ClearAll;
	else
		ctx->renderPassBeginInfo.renderPass = ctx->dev->renderPass_ClearStencil;
	ctx->pSurf->newSurf = false;

	io_unsync(ctx->pSurf);
	io_grab(vkg_surface, ctx->pSurf);

	if (ctx->dev->samples == VK_SAMPLE_COUNT_1_BIT)
		ctx->renderPassBeginInfo.clearValueCount = 2;
	else
		ctx->renderPassBeginInfo.clearValueCount = 3;

	ctx->selectedCharSize	= 10 << 6;
	ctx->currentFont		= NULL;
	ctx->selectedFontName[0]= 0;
	ctx->pattern			= NULL;
	ctx->curColor			= 0xff000000;//opaque black
	ctx->cmdStarted			= false;
	ctx->curClipState		= vkg_clip_state_none;
	ctx->vertCount			= ctx->indCount = 0;
#ifdef VKVG_ENABLE_VK_TIMELINE_SEMAPHORE
	ctx->timelineStep		= 0;
#endif
}


void VkgContext::clear_context () {
	//free saved context stack elmt
	vkg_context_save* next = ctx->pSavedCtxs;
	ctx->pSavedCtxs = NULL;
	while (next != NULL) {
		vkg_context_save* cur = next;
		next = cur->pNext;
		_free_ctx_save (cur);
	}
	//free additional stencil use in save/restore process
	if (ctx->savedStencils) {
		uint8_t curSaveStencil = ctx->curSavBit / 6;
		for (int i=curSaveStencil;i>0;i--)
			vke_image_drop(ctx->savedStencils[i-1]);
		free(ctx->savedStencils);
		ctx->savedStencils = NULL;
		ctx->curSavBit = 0;
	}

	//remove context from double linked list of context in device
	/*if (ctx->dev->lastCtx == ctx){
		ctx->dev->lastCtx = ctx->pPrev;
		if (ctx->pPrev != NULL)
			ctx->pPrev->pNext = NULL;
	}else if (ctx->pPrev == NULL){
		//first elmt, and it's not last one so pnext is not null
		ctx->pNext->pPrev = NULL;
	} else {
		ctx->pPrev->pNext = ctx->pNext;
		ctx->pNext->pPrev = ctx->pPrev;
	}*/
	if (ctx->dashCount > 0) {
		free(ctx->dashes);
		ctx->dashCount = 0;
	}
}

VkgContext::~VkgContext() {
	vke_log(VKE_LOG_INFO, "DESTROY Context: ctx = %p (status:%d); surf = %p\n", ctx, ctx->status, ctx->pSurf);

	vkg_flush (ctx);

	vke_log(VKE_LOG_DBG_ARRAYS, "END\tctx = %p; pathes:%d pts:%d vch:%d vbo:%d ich:%d ibo:%d\n", ctx, ctx->sizePathes, ctx->sizePoints, ctx->sizeVertices, ctx->sizeVBO, ctx->sizeIndices, ctx->sizeIBO);

#if VKVG_RECORDING
	if (ctx->recording)
		_destroy_recording(ctx->recording);
#endif

	if (ctx->pattern)
		VkgPattern::drop(ctx->pattern);

	_clear_context (ctx);

#if VKVG_DBG_STATS
	if (ctx->dev->threadAware)
		mtx_lock (&ctx->dev->mutex);
	
	vkg_debug_stats* dbgstats = &ctx->dev->debug_stats;
	if (dbgstats->sizePoints < ctx->sizePoints)
		dbgstats->sizePoints = ctx->sizePoints;
	if (dbgstats->sizePathes < ctx->sizePathes)
		dbgstats->sizePathes = ctx->sizePathes;
	if (dbgstats->sizeVertices < ctx->sizeVertices)
		dbgstats->sizeVertices = ctx->sizeVertices;
	if (dbgstats->sizeIndices < ctx->sizeIndices)
		dbgstats->sizeIndices = ctx->sizeIndices;
	if (dbgstats->sizeVBO < ctx->sizeVBO)
		dbgstats->sizeVBO = ctx->sizeVBO;
	if (dbgstats->sizeIBO < ctx->sizeIBO)
		dbgstats->sizeIBO = ctx->sizeIBO;

	if (ctx->dev->threadAware)
		mtx_unlock (&ctx->dev->mutex);
#endif

	vkg_surface_drop(ctx->pSurf);

	if (!ctx->status && ctx->dev->cachedContextCount < VKVG_MAX_CACHED_CONTEXT_COUNT) {
		_device_store_context (ctx);
		return;
	}

	_release_context_ressources (ctx);
}

VkgContext::VkgContext(VkgSurface surf)
{
	VkgDevice dev = surf->dev;
	VkgContext ctx = NULL;

	if (_device_try_get_cached_context (dev, &ctx) ) {
		ctx->pSurf = surf;

		if (!surf || surf->status) {
			ctx->status = VKE_STATUS_INVALID_SURFACE;
			return ctx;
		}

		_init_ctx (ctx);
		_update_descriptor_set (surf->dev->emptyImg, ctx->dsSrc);
		_clear_path	(ctx);
		ctx->cmd = ctx->cmdBuffers[0];//current recording buffer
		ctx->status = VKE_STATUS_SUCCESS;
		return ctx;
	}

	ctx = io_new(vkg_context);
	
	vke_log(VKE_LOG_INFO, "CREATE Context: ctx = %p; surf = %p\n", ctx, surf);

	if (!ctx)
		return (VkgContext)&_no_mem_status;

	ctx->pSurf = surf;

	if (!surf || surf->status) {
		ctx->status = VKE_STATUS_INVALID_SURFACE;
		return ctx;
	}

	ctx->sizePoints		= VKVG_PTS_SIZE;
	ctx->sizeVertices	= ctx->sizeVBO = VKVG_VBO_SIZE;
	ctx->sizeIndices	= ctx->sizeIBO = VKVG_IBO_SIZE;
	ctx->sizePathes		= VKVG_PATHES_SIZE;
	ctx->renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;

	ctx->dev = surf->dev;

	_init_ctx (ctx);

	ctx->points			= (vec2f*)malloc (VKVG_VBO_SIZE * sizeof(vec2f));
	ctx->pathes			= (uint32_t*)malloc (VKVG_PATHES_SIZE * sizeof(uint32_t));
	ctx->vertexCache	= (Vertex*)malloc (ctx->sizeVertices * sizeof(Vertex));
	ctx->indexCache		= (VKVG_IBO_INDEX_TYPE*)malloc (ctx->sizeIndices * sizeof(VKVG_IBO_INDEX_TYPE));

	if (!ctx->points || !ctx->pathes || !ctx->vertexCache || !ctx->indexCache) {
		dev->status = VKE_STATUS_NO_MEMORY;
		if (ctx->points)
			free(ctx->points);
		if (ctx->pathes)
			free(ctx->pathes);
		if (ctx->vertexCache)
			free(ctx->vertexCache);
		if (ctx->indexCache)
			free(ctx->indexCache);
		return NULL;
	}

	//for context to be thread safe, command pool and descriptor pool have to be created in the thread of the context.
	ctx->cmdPool	= vke_cmd_pool_create (dev->vke, dev->gQueue->familyIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

#ifndef VKVG_ENABLE_VK_TIMELINE_SEMAPHORE
	ctx->flushFence	= vke_fence_create_signaled (ctx->dev->vke);
#endif

	_create_vertices_buff	(ctx);
	_create_gradient_buff	(ctx);
	_create_cmd_buff		(ctx);
	_createDescriptorPool	(ctx);
	_init_descriptor_sets	(ctx);
	_font_cache_update_context_descset (ctx);
	_update_descriptor_set	(surf->dev->emptyImg, ctx->dsSrc);
	_update_gradient_desc_set(ctx);

	_clear_path				(ctx);

	ctx->cmd = ctx->cmdBuffers[0];//current recording buffer

	ctx->references = 1;
	ctx->status = VKE_STATUS_SUCCESS;

	vke_log(VKE_LOG_DBG_ARRAYS, "INIT\tctx = %p; pathes:%llu pts:%llu vch:%d vbo:%d ich:%d ibo:%d\n", ctx, (uint64_t)ctx->sizePathes, (uint64_t)ctx->sizePoints, ctx->sizeVertices, ctx->sizeVBO, ctx->sizeIndices, ctx->sizeIBO);

#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	vke_device_set_object_name(dev->vke, VK_OBJECT_TYPE_COMMAND_POOL, (uint64_t)ctx->cmdPool, "CTX Cmd Pool");
	vke_device_set_object_name(dev->vke, VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)ctx->cmdBuffers[0], "CTX Cmd Buff A");
	vke_device_set_object_name(dev->vke, VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)ctx->cmdBuffers[1], "CTX Cmd Buff B");
#ifndef VKVG_ENABLE_VK_TIMELINE_SEMAPHORE
	vke_device_set_object_name(dev->vke, VK_OBJECT_TYPE_FENCE, (uint64_t)ctx->flushFence, "CTX Flush Fence");
#endif
	vke_device_set_object_name(dev->vke, VK_OBJECT_TYPE_DESCRIPTOR_POOL, (uint64_t)ctx->descriptorPool, "CTX Descriptor Pool");
	vke_device_set_object_name(dev->vke, VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)ctx->dsSrc, "CTX DescSet SOURCE");
	vke_device_set_object_name(dev->vke, VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)ctx->dsFont, "CTX DescSet FONT");
	vke_device_set_object_name(dev->vke, VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)ctx->dsGrad, "CTX DescSet GRADIENT");

	vke_device_set_object_name(dev->vke, VK_OBJECT_TYPE_BUFFER, (uint64_t)ctx->indices.buffer, "CTX Index Buff");
	vke_device_set_object_name(dev->vke, VK_OBJECT_TYPE_BUFFER, (uint64_t)ctx->vertices.buffer, "CTX Vertex Buff");
#endif

	return ctx;
}
void VkgContext::flush (VkgContext ctx){
	if (ctx->status)
		return;
	_flush_cmd_buff		(ctx);
	_wait_ctx_flush_end	(ctx);
/*
#ifdef DEBUG

	vec4 red = {0,0,1,1};
	vec4 green = {0,1,0,1};
	vec4 white = {1,1,1,1};

	int j = 0;
	while (j < dlpCount) {
		add_line(debugLinePoints[j], debugLinePoints[j+1],green);
		j+=2;
		add_line(debugLinePoints[j], debugLinePoints[j+1],red);
		j+=2;
		add_line(debugLinePoints[j], debugLinePoints[j+1],white);
		j+=2;
	}
	dlpCount = 0;
	CmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pSurf->dev->pipelineLineList);
	CmdDrawIndexed(ctx->cmd, ctx->indCount-ctx->curIndStart, 1, ctx->curIndStart, 0, 1);
	_flush_cmd_buff(ctx);
#endif
*/
}

void VkgContext::set_opacity (float opacity) {
	if (ctx->status)
		return;

	if (EQUF(ctx->pushConsts.opacity, opacity))
		return;

	emit_draw_cmd_undrawn_vertices (ctx);
	ctx->pushConsts.opacity = opacity;
	ctx->pushCstDirty = true;
}
float VkgContext::get_opacity (VkgContext ctx) {
	if (ctx->status)
		return 0;
	return ctx->pushConsts.opacity;
}
VkeStatus VkgContext::status (VkgContext ctx) {
	return ctx->status;
}

void VkgContext::new_sub_path (VkgContext ctx){
	if (ctx->status)
		return;

	RECORD(data, VKVG_CMD_NEW_SUB_PATH);
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: new_sub_path:\n");

	finish_path(ctx);
}
void VkgContext::new_path (VkgContext ctx){
	if (ctx->status)
		return;

	vke_log(VKE_LOG_INFO_CMD, "\tCMD: new_path:\n");

	_clear_path(ctx);
	RECORD(data, VKVG_CMD_NEW_PATH);
}
void VkgContext::close_path (VkgContext ctx){
	if (ctx->status)
		return;

	RECORD(data, VKVG_CMD_CLOSE_PATH);
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: close_path:\n");

	if (ctx->pathes[ctx->pathPtr] & PATH_CLOSED_BIT) //already closed
		return;
	//check if at least 3 points are present
	if (ctx->pathes[ctx->pathPtr] < 3)
		return;

	//prevent closing on the same point
	if (vec2f_equ(ctx->points[ctx->pointCount-1],
				 ctx->points[ctx->pointCount - ctx->pathes[ctx->pathPtr]])) {
		if (ctx->pathes[ctx->pathPtr] < 4)//ensure enough points left for closing
			return;
		_remove_last_point(ctx);
	}

	ctx->pathes[ctx->pathPtr] |= PATH_CLOSED_BIT;

	finish_path(ctx);
}
void VkgContext::rel_line_to (float dx, float dy){
	if (ctx->status)
		return;

	RECORD(data, VKVG_CMD_REL_LINE_TO, dx, dy);
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: rel_line_to: %f, %f\n", dx, dy);

	if (current_path_is_empty(ctx))
		add_point(0, 0);
	vec2f cp = _get_current_position(ctx);
	line_to(cp[X] + dx, cp[Y] + dy);
}
void VkgContext::line_to (float x, float y)
{
	if (ctx->status)
		return;

	RECORD(data, VKVG_CMD_LINE_TO, x, y);
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: line_to: %f, %f\n", x, y);
	line_to(x, y);
}
void VkgContext::arc (float xc, float yc, float radius, float a1, float a2){
	if (ctx->status)
		return;

	RECORD(data, VKVG_CMD_ARC, xc, yc, radius, a1, a2);
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: arc: %f,%f %f %f %f\n", xc, yc, radius, a1, a2);

	while (a2 < a1)//positive arc must have a1<a2
		a2 += 2.f*M_PIF;

	if (a2 - a1 > 2.f * M_PIF) //limit arc to 2PI
		a2 = a1 + 2.f * M_PIF;

	vec2f v = {cosf(a1)*radius + xc, sinf(a1)*radius + yc};

	float step = _get_arc_step(radius);
	float a = a1;

	if (current_path_is_empty(ctx)){
		set_curve_start (ctx);
		add_point (v[X], v[Y]);
		if (!ctx->pathPtr)
			ctx->simpleConvex = true;
		else
			ctx->simpleConvex = false;
	} else {
		line_to(v[X], v[Y]);
		set_curve_start (ctx);
		ctx->simpleConvex = false;
	}

	a+=step;

	if (EQUF(a2, a1))
		return;

	while(a < a2){
		v[X] = cosf(a)*radius + xc;
		v[Y] = sinf(a)*radius + yc;
		add_point (v[X], v[Y]);
		a+=step;
	}

	if (EQUF(a2-a1,M_PIF*2.f)){//if arc is complete circle, last point is the same as the first one
		set_curve_end();
		close_path();
		return;
	}
	a = a2;
	v[X] = cosf(a)*radius + xc;
	v[Y] = sinf(a)*radius + yc;
	add_point (v[X], v[Y]);
	set_curve_end(ctx);
}
void VkgContext::arc_negative (float xc, float yc, float radius, float a1, float a2) {
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_ARC_NEG, xc, yc, radius, a1, a2);
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: %f,%f %f %f %f\n", xc, yc, radius, a1, a2);
	while (a2 > a1)
		a2 -= 2.f*M_PIF;
	if (a1 - a2 > a1 + 2.f * M_PIF) //limit arc to 2PI
		a2 = a1 - 2.f * M_PIF;

	vec2f v = {cosf(a1)*radius + xc, sinf(a1)*radius + yc};

	float step = _get_arc_step(radius);
	float a = a1;

	if (current_path_is_empty(ctx)){
		set_curve_start (ctx);
		add_point (v[X], v[Y]);
		if (!ctx->pathPtr)
			ctx->simpleConvex = true;
		else
			ctx->simpleConvex = false;
	} else {
		line_to(v[X], v[Y]);
		set_curve_start (ctx);
		ctx->simpleConvex = false;
	}

	a-=step;

	if (EQUF(a2, a1))
		return;

	while(a > a2){
		v[X] = cosf(a)*radius + xc;
		v[Y] = sinf(a)*radius + yc;
		add_point (ctx,v[X],v[Y]);
		a-=step;
	}

	if (EQUF(a1-a2,M_PIF*2.f)){//if arc is complete circle, last point is the same as the first one
		set_curve_end(ctx);
		close_path(ctx);
		return;
	}

	a = a2;
	//vec2f lastP = v;
	v[X] = cosf(a)*radius + xc;
	v[Y] = sinf(a)*radius + yc;
	//if (!vec2f_equ (v,lastP))
		add_point (v[X], v[Y]);
	set_curve_end(ctx);
}
void VkgContext::rel_move_to (float x, float y)
{
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_REL_MOVE_TO, x, y);
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: rel_mote_to: %f, %f\n", x, y);
	if (current_path_is_empty(ctx))
		add_point(0, 0);
	vec2f cp = _get_current_position(ctx);
	finish_path(ctx);
	add_point (cp[X] + x, cp[Y] + y);
}
void VkgContext::move_to (float x, float y)
{
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_MOVE_TO, x, y);
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: move_to: %f,%f\n", x, y);
	finish_path(ctx);
	add_point (x, y);
}
bool VkgContext::has_current_point () {
	if (ctx->status)
		return false;
	return !current_path_is_empty(ctx);
}
void VkgContext::get_current_point (float* x, float* y) {
	if (ctx->status || current_path_is_empty(ctx)) {
		*x = *y = 0;
		return;
	}
	vec2f cp = _get_current_position(ctx);
	*x = cp[X];
	*y = cp[Y];
}
void VkgContext::curve_to (float x1, float y1, float x2, float y2, float x3, float y3) {
	//prevent running recursive_bezier when all 4 curve points are equal
	if (EQUF(x1,x2) && EQUF(x2,x3) && EQUF(y1,y2) && EQUF(y2,y3)) {
		if (current_path_is_empty(ctx) || (EQUF(_get_current_position(ctx)[X],x1) && EQUF(_get_current_position(ctx)[Y],y1)))
			return;
	}
	ctx->simpleConvex = false;
	set_curve_start (ctx);
	if (current_path_is_empty(ctx))
		add_point(x1, y1);

	vec2f cp = _get_current_position(ctx);

	//compute dyn distanceTolerance depending on current scale
	float sx = 1, sy = 1;
	ctx->pushConsts.mat.matrix_get_scale (&sx, &sy);

	float distanceTolerance = fabs(0.25f / fmaxf(sx,sy));

	recursive_bezier (distanceTolerance, cp[X], cp[Y], x1, y1, x2, y2, x3, y3, 0);
	/*cp[X] = x3;
	cp[Y] = y3;
	if (!vec2f_equ(ctx->points[ctx->pointCount-1],cp))*/
		add_point(ctx,x3,y3);
	set_curve_end (ctx);
}
const double quadraticFact = 2.0/3.0;
void VkgContext::quadratic_to (float x1, float y1, float x2, float y2) {
	float x0, y0;
	if (current_path_is_empty(ctx)) {
		x0 = x1;
		y0 = y1;
	} else
		vkg_get_current_point (&x0, &y0);
	_curve_to (ctx,
					x0 + (x1 - x0) * quadraticFact,
					y0 + (y1 - y0) * quadraticFact,
					x2 + (x1 - x2) * quadraticFact,
					y2 + (y1 - y2) * quadraticFact,
					x2, y2);
}
void VkgContext::quadratic_to (float x1, float y1, float x2, float y2) {
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_QUADRATIC_TO, x1, y1, x2, y2);
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: quadratic_to: %f, %f, %f, %f\n", x1, y1, x2, y2);
	quadratic_to(x1, y1, x2, y2);
}
void VkgContext::rel_quadratic_to (float x1, float y1, float x2, float y2) {
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_REL_QUADRATIC_TO, x1, y1, x2, y2);
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: rel_quadratic_to: %f, %f, %f, %f\n", x1, y1, x2, y2);
	vec2f cp = _get_current_position(ctx);
	quadratic_to (cp[X] + x1, cp[Y] + y1, cp[X] + x2, cp[Y] + y2);
}
void VkgContext::curve_to (float x1, float y1, float x2, float y2, float x3, float y3) {
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_CURVE_TO, x1, y1, x2, y2, x3, y3);
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: curve_to %f,%f %f,%f %f,%f:\n", x1, y1, x2, y2, x3, y3);
	_curve_to (x1, y1, x2, y2, x3, y3);
}
void VkgContext::rel_curve_to (float x1, float y1, float x2, float y2, float x3, float y3) {
	if (ctx->status)
		return;
	if (current_path_is_empty(ctx)) {
		ctx->status = VKE_STATUS_NO_CURRENT_POINT;
		return;
	}
	RECORD(data, (uint32_t)VKVG_CMD_REL_CURVE_TO, x1, y1, x2, y2, x3, y3);
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: rel curve_to %f,%f %f,%f %f,%f:\n", x1, y1, x2, y2, x3, y3);
	vec2f cp = _get_current_position(ctx);
	_curve_to (cp[X] + x1, cp[Y] + y1, cp[X] + x2, cp[Y] + y2, cp[X] + x3, cp[Y] + y3);
}
void VkgContext::fill_rectangle (float x, float y, float w, float h){
	if (ctx->status)
		return;
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: fill_rectangle:\n");
	vao_add_rectangle (ctx,x,y,w,h);
	//_record_draw_cmd(ctx);
}

VkeStatus VkgContext::rectangle (float x, float y, float w, float h){
	if (ctx->status)
		return ctx->status;
	RECORD2(data, VKVG_CMD_RECTANGLE, x, y, w, h);
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: rectangle: %f,%f,%f,%f\n", x, y, w, h);
	finish_path (ctx);

	if (w <= 0 || h <= 0)
		return VKE_STATUS_INVALID_RECT;

	add_point (x, y);
	add_point (x + w, y);
	add_point (x + w, y + h);
	add_point (x, y + h);	

	ctx->pathes[ctx->pathPtr] |= (PATH_CLOSED_BIT|PATH_IS_CONVEX_BIT);

	finish_path(ctx);
	return VKE_STATUS_SUCCESS;
}
VkeStatus VkgContext::rounded_rectangle (float x, float y, float w, float h, float radius){
	if (ctx->status)
		return ctx->status;
	vke_log(VKE_LOG_INFO_CMD, "CMD: rounded_rectangle:\n");
	finish_path (ctx);

	if (w <= 0 || h <= 0)
		return VKE_STATUS_INVALID_RECT;

	if ((radius > w / 2.0f) || (radius > h / 2.0f))
		radius = fmin (w / 2.0f, h / 2.0f);

	move_to(x, y + radius);
	arc(x + radius, y + radius, radius, M_PIF, -M_PIF_2);
	line_to(x + w - radius, y);
	arc(x + w - radius, y + radius, radius, -M_PIF_2, 0);
	line_to(x + w, y + h - radius);
	arc(x + w - radius, y + h - radius, radius, 0, M_PIF_2);
	line_to(x + radius, y + h);
	arc(x + radius, y + h - radius, radius, M_PIF_2, M_PIF);
	line_to(x, y + radius);
	close_path(ctx);

	return VKE_STATUS_SUCCESS;
}
void VkgContext::rounded_rectangle2 (float x, float y, float w, float h, float rx, float ry){
	if (ctx->status)
		return;
	vke_log(VKE_LOG_INFO_CMD, "CMD: rounded_rectangle2:\n");
	move_to (x+rx, y);
	line_to (x+w-rx, y);
	elliptic_arc_to(x+w, y+ry, false, true, rx, ry, 0);

	line_to (x+w, y+h-ry);
	elliptic_arc_to(x+w-rx, y+h, false, true, rx, ry, 0);

	line_to (x+rx, y+h);
	elliptic_arc_to(x, y+h-ry , false, true, rx, ry, 0);

	line_to (x, y+ry);
	elliptic_arc_to(x+rx, y , false, true, rx, ry, 0);

	close_path(ctx);
}
void VkgContext::path_extents (float *x1, float *y1, float *x2, float *y2) {
	if (ctx->status)
		return;

	finish_path(ctx);

	if (!ctx->pathPtr) {//no path
		*x1 = *x2 = *y1 = *y2 = 0;
		return;
	}

	path_extents(false, x1, y1, x2, y2);
}

vkg_clip_state VkgContext::get_previous_clip_state () {
	if (!ctx->pSavedCtxs)//no clip saved => clear
		return vkg_clip_state_clear;
	return ctx->pSavedCtxs->clippingState;
}
static const VkClearAttachment clearStencil		   = {VK_IMAGE_ASPECT_STENCIL_BIT, 1, {{{0}}}};
static const VkClearAttachment clearColorAttach	   = {VK_IMAGE_ASPECT_COLOR_BIT,   0, {{{0}}}};

void VkgContext::reset_clip () {
	emit_draw_cmd_undrawn_vertices(ctx);
	if (!ctx->cmdStarted) {
		//if command buffer is not already started and in a renderpass, we use the renderpass
		//with the loadop clear for stencil
		ctx->renderPassBeginInfo.renderPass = ctx->dev->renderPass_ClearStencil;
		//force run of one renderpass (even empty) to perform clear load op
		_start_cmd_for_render_pass(ctx);
		return;
	}
	vkCmdClearAttachments(ctx->cmd, 1, &clearStencil, 1, &ctx->clearRect);
}

void VkgContext::reset_clip() {
	if (ctx->status)
		return;

	RECORD(data, VKVG_CMD_RESET_CLIP);

	if (ctx->curClipState == vkg_clip_state_clear)
		return;
	if (_get_previous_clip_state(ctx) == vkg_clip_state_clear)
		ctx->curClipState = vkg_clip_state_none;
	else
		ctx->curClipState = vkg_clip_state_clear;

	_reset_clip (ctx);
}
void VkgContext::clear (){
	if (ctx->status)
		return;

	RECORD(data, VKVG_CMD_CLEAR);

	if (_get_previous_clip_state(ctx) == vkg_clip_state_clear)
		ctx->curClipState = vkg_clip_state_none;
	else
		ctx->curClipState = vkg_clip_state_clear;

	emit_draw_cmd_undrawn_vertices(ctx);
	if (!ctx->cmdStarted) {
		ctx->renderPassBeginInfo.renderPass = ctx->dev->renderPass_ClearAll;
		_start_cmd_for_render_pass(ctx);
		return;
	}
	VkClearAttachment ca[2] = {clearColorAttach, clearStencil};
	vkCmdClearAttachments(ctx->cmd, 2, ca, 1, &ctx->clearRect);
}
void VkgContext::clip_preserve (){
	finish_path(ctx);

	if (!ctx->pathPtr)//nothing to clip
		return;

	emit_draw_cmd_undrawn_vertices(ctx);

	vke_log(VKE_LOG_INFO, "CLIP: ctx = %p; path cpt = %d;\n", ctx, ctx->pathPtr / 2);

	ensure_renderpass_is_started(ctx);

#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	vke_cmd_label_start(ctx->cmd, "clip", DBG_LAB_COLOR_CLIP);
#endif

	if (ctx->curFillRule == VKVG_FILL_RULE_EVEN_ODD){
		poly_fill				(NULL);
		CmdBindPipeline			(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->dev->pipelineClipping);
	} else {
		CmdBindPipeline			(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->dev->pipelineClipping);
		CmdSetStencilReference	(ctx->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_FILL_BIT);
		CmdSetStencilCompareMask(ctx->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_CLIP_BIT);
		CmdSetStencilWriteMask	(ctx->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_FILL_BIT);
		fill_non_zero(ctx);
		emit_draw_cmd_undrawn_vertices(ctx);
	}
	CmdSetStencilReference	(ctx->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_CLIP_BIT);
	CmdSetStencilCompareMask(ctx->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_FILL_BIT);
	CmdSetStencilWriteMask	(ctx->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_ALL_BIT);

	draw_full_screen_quad (NULL);

	bind_draw_pipeline (ctx);
	CmdSetStencilCompareMask (ctx->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_CLIP_BIT);

#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	vke_cmd_label_end (ctx->cmd);
#endif

	ctx->curClipState = vkg_clip_state_clip;
}
void _fill_preserve (VkgContext ctx){
	finish_path(ctx);

	if (!ctx->pathPtr)//nothing to fill
		return;

	vke_log(VKE_LOG_INFO, "FILL: ctx = %p; path cpt = %d;\n", ctx, ctx->subpathCount);

	 if (ctx->curFillRule == VKVG_FILL_RULE_EVEN_ODD){
		 emit_draw_cmd_undrawn_vertices(ctx);
		vec4 bounds = {FLT_MAX,FLT_MAX,FLT_MIN,FLT_MIN};
		poly_fill				(&bounds);
		bind_draw_pipeline		(ctx);
		CmdSetStencilCompareMask(ctx->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_FILL_BIT);
		draw_full_screen_quad	(&bounds);
		CmdSetStencilCompareMask(ctx->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_CLIP_BIT);
		return;
	}

	if (ctx->vertCount - ctx->curVertOffset + ctx->pointCount > VKVG_IBO_MAX)
		emit_draw_cmd_undrawn_vertices(ctx);//limit draw call to addressable vx with choosen index type

	if (ctx->pattern)//if not solid color, source img or gradient has to be bound
		ensure_renderpass_is_started(ctx);
	fill_non_zero(ctx);
}
void _stroke_preserve (VkgContext ctx)
{
	finish_path(ctx);

	if (!ctx->pathPtr)//nothing to stroke
		return;

	vke_log(VKE_LOG_INFO, "STROKE: ctx = %p; path ptr = %d;\n", ctx, ctx->pathPtr);

	stroke_context str = {0};
	str.hw = ctx->lineWidth * 0.5f;
	str.lhMax = ctx->miterLimit * ctx->lineWidth;
	uint32_t ptrPath = 0;

	while (ptrPath < ctx->pathPtr){
		uint32_t ptrSegment = 0, lastSegmentPointIdx = 0;
		uint32_t firstPathPointIdx = str.cp;
		uint32_t pathPointCount = ctx->pathes[ptrPath]&PATH_ELT_MASK;
		uint32_t lastPathPointIdx = str.cp + pathPointCount - 1;

		dash_context_t dc = {0};

		if (path_has_curves (ctx,ptrPath)) {
			ptrSegment = 1;
			lastSegmentPointIdx = str.cp + (ctx->pathes[ptrPath+ptrSegment]&PATH_ELT_MASK)-1;
		}

		str.firstIdx = (VKVG_IBO_INDEX_TYPE)(ctx->vertCount - ctx->curVertOffset);

		//vke_log(VKE_LOG_INFO_PATH, "\tPATH: points count=%10d end point idx=%10d", ctx->pathes[ptrPath]&PATH_ELT_MASK, lastPathPointIdx);

		if (ctx->dashCount > 0) {
			//init dash stroke
			dc.dashOn = true;
			dc.curDash = 0;	//current dash index
			dc.totDashLength = 0;//limit offset to total length of dashes
			for (uint32_t i=0;i<ctx->dashCount;i++)
				dc.totDashLength += ctx->dashes[i];
			if (dc.totDashLength == 0){
				ctx->status = VKE_STATUS_INVALID_DASH;
				return;
			}
			dc.curDashOffset = fmodf(ctx->dashOffset, dc.totDashLength);	//cur dash offset between defined path point and last dash segment(on/off) start
			str.iL = lastPathPointIdx;
		} else if (_path_is_closed(ctx,ptrPath)){
			str.iL = lastPathPointIdx;
		} else {
			_draw_stoke_cap(&str, ctx->points[str.cp], vec2f_line_norm(ctx->points[str.cp], ctx->points[str.cp+1]), true);
			str.iL = str.cp++;
		}

		if (path_has_curves (ctx,ptrPath)) {
			while (str.cp < lastPathPointIdx){

				bool curved = ctx->pathes [ptrPath + ptrSegment] & PATH_HAS_CURVES_BIT;
				if (lastSegmentPointIdx == lastPathPointIdx)//last segment of path, dont draw end point here
					lastSegmentPointIdx--;
				while (str.cp <= lastSegmentPointIdx)
					_draw_segment(&str, &dc, curved);

				ptrSegment ++;
				uint32_t cptSegPts = ctx->pathes [ptrPath + ptrSegment]&PATH_ELT_MASK;
				lastSegmentPointIdx = str.cp + cptSegPts - 1;
				if (lastSegmentPointIdx == lastPathPointIdx && cptSegPts == 1) {
					//single point last segment
					ptrSegment++;
					break;
				}
			}
		}else while (str.cp < lastPathPointIdx)
			_draw_segment(&str, &dc, false);

		if (ctx->dashCount > 0) {
			if (_path_is_closed(ctx,ptrPath)){
				str.iR = firstPathPointIdx;

				draw_dashed_segment(&str, &dc, false);

				str.iL++;
				str.cp++;
			}
			if (!dc.dashOn){
				//finishing last dash that is already started, draw end caps but not too close to start
				//the default gap is the next void
				int32_t prevDash = (int32_t)dc.curDash-1;
				if (prevDash < 0)
					dc.curDash = ctx->dashCount-1;
				float m = fminf (ctx->dashes[prevDash] - dc.curDashOffset, ctx->dashes[dc.curDash]);
				vec2f p = vec2f_sub(ctx->points[str.iR], vec2f_scale(dc.normal, m));
				_draw_stoke_cap (&str, p, dc.normal, false);
			}
		} else if (_path_is_closed(ctx,ptrPath)){
			str.iR = firstPathPointIdx;
			bool inverse = build_vb_step (&str, false);

			VKVG_IBO_INDEX_TYPE* inds = &ctx->indexCache [ctx->indCount-6];
			VKVG_IBO_INDEX_TYPE ii = str.firstIdx;
			if (inverse){
				inds[1] = ii+1;
				inds[4] = ii+1;
				inds[5] = ii;
			} else {
				inds[1] = ii;
				inds[4] = ii;
				inds[5] = ii+1;
			}
			str.cp++;
		}else
			_draw_stoke_cap (&str, ctx->points[str.cp], vec2f_line_norm(ctx->points[str.cp-1], ctx->points[str.cp]), false);

		str.cp = firstPathPointIdx + pathPointCount;

		if (ptrSegment > 0)
			ptrPath += ptrSegment;
		else
			ptrPath++;

		//limit batch size here to 1/3 of the ibo index type ability
		if (ctx->vertCount - ctx->curVertOffset > VKVG_IBO_MAX / 3)
			emit_draw_cmd_undrawn_vertices (ctx);
	}

}

void VkgContext::clip (){
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_CLIP);
	_clip_preserve(ctx);
	_clear_path(ctx);
}
void VkgContext::stroke ()
{
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_STROKE);
	_stroke_preserve(ctx);
	_clear_path(ctx);
}
void VkgContext::fill (){
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_FILL);
	_fill_preserve(ctx);
	_clear_path(ctx);
}
void VkgContext::clip_preserve () {
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_CLIP_PRESERVE);
	_clip_preserve (ctx);
}
void VkgContext::fill_preserve () {
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_FILL_PRESERVE);
	_fill_preserve (ctx);
}
void VkgContext::stroke_preserve () {
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_STROKE_PRESERVE);
	_stroke_preserve (ctx);
}

void VkgContext::paint (){
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_PAINT);
	finish_path (ctx);

	if (ctx->pathPtr) {
		fill();
		return;
	}

	ensure_renderpass_is_started ();
	draw_full_screen_quad (NULL);
}
void VkgContext::set_source_color (uint32_t c) {
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_SET_SOURCE_COLOR, c);
	ctx->curColor = c;
	_update_cur_pattern (NULL);
}
void VkgContext::set_source_rgb (float r, float g, float b) {
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_SET_SOURCE_RGB, r, g, b);
	ctx->curColor = CreateRgbaf(r,g,b,1);
	_update_cur_pattern (NULL);
}
void VkgContext::set_source_rgba (float r, float g, float b, float a)
{
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_SET_SOURCE_RGBA, r, g, b, a);
	ctx->curColor = CreateRgbaf(r,g,b,a);
	_update_cur_pattern (NULL);
}
void VkgContext::set_source_surface(VkgSurface surf, float x, float y){
	if (ctx->status || surf->status)
		return;
	RECORD(data, VKVG_CMD_SET_SOURCE_SURFACE, x, y, surf);
	ctx->pushConsts.source[X] = x;
	ctx->pushConsts.source[Y] = y;
	update_cur_pattern (VkgPattern(surf));
	ctx->pushCstDirty = true;
}
void VkgContext::set_source (VkgPattern pat){
	if (ctx->status || pat->status)
		return;
	RECORD(VKVG_CMD_SET_SOURCE, pat);
	update_cur_pattern (pat);
	VkgPattern::grab	(pat);
}
void VkgContext::set_line_width (float width){
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_SET_LINE_WIDTH, width);
	ctx->lineWidth = width;
}
void VkgContext::set_miter_limit (float limit){
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_SET_LINE_WIDTH, limit);
	ctx->miterLimit = limit;
}
void VkgContext::set_line_cap (vkg_line_cap cap){
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_SET_LINE_CAP, cap);
	ctx->lineCap = cap;
}
void VkgContext::set_line_join (vkg_line_join join){
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_SET_LINE_JOIN, join);
	ctx->lineJoin = join;
}
void VkgContext::set_operator (vkg_operator op){
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_SET_OPERATOR, op);
	if (op == ctx->curOperator)
		return;

	emit_draw_cmd_undrawn_vertices();//draw call with different ops cant be combined, so emit draw cmd for previous vertices.

	ctx->curOperator = op;

	if (ctx->cmdStarted)
		bind_draw_pipeline (ctx);
}
void VkgContext::set_fill_rule (vkg_fill_rule fr){
	if (ctx->status)
		return;
#ifndef __APPLE__
	RECORD(data, VKVG_CMD_SET_FILL_RULE, fr);
	ctx->curFillRule = fr;
#endif
}
vkg_fill_rule VkgContext::get_fill_rule (){
	if (ctx->status)
		return VKVG_FILL_RULE_NON_ZERO;
	return ctx->curFillRule;
}
float VkgContext::get_line_width () {
	if (ctx->status)
		return 0;
	return ctx->lineWidth;
}
void VkgContext::set_dash (const float* dashes, uint32_t num_dashes, float offset){
	if (ctx->status)
		return;
	if (ctx->dashCount > 0)
		free (ctx->dashes);
	RECORD(data, VKVG_CMD_SET_DASH, num_dashes, offset, dashes);
	ctx->dashCount = num_dashes;
	ctx->dashOffset = offset;
	if (ctx->dashCount == 0)
		return;
	ctx->dashes = (float*)malloc (sizeof(float) * ctx->dashCount);
	memcpy (ctx->dashes, dashes, sizeof(float) * ctx->dashCount);
}
void VkgContext::get_dash (const float* dashes, uint32_t* num_dashes, float* offset){
	if (ctx->status)
		return;
	*num_dashes = ctx->dashCount;
	*offset = ctx->dashOffset;
	if (ctx->dashCount == 0 || dashes == NULL)
		return;
	memcpy ((float*)dashes, ctx->dashes, sizeof(float) * ctx->dashCount);
}


vkg_line_cap VkgContext::get_line_cap (){
	if (ctx->status)
		return (vkg_line_cap)0;
	return ctx->lineCap;
}
vkg_line_join VkgContext::get_line_join (){
	if (ctx->status)
		return (vkg_line_join)0;
	return ctx->lineJoin;
}
vkg_operator VkgContext::get_operator (VkgContext ctx){
	if (ctx->status)
		return (vkg_operator)0;
	return ctx->curOperator;
}
VkgPattern VkgContext::get_source (VkgContext ctx){
	if (ctx->status)
		return NULL;
	io_grab(vkg_pattern, ctx->pattern);
	return ctx->pattern;
}

void VkgContext::select_font_face (const char* name){
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_SET_FONT_FACE, name);
	select_font_face (name);
}
void VkgContext::load_font_from_path (const char* path, const char* name){
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_SET_FONT_PATH, name);
	vkg_font_identity* fid = _font_cache_add_font_identity(path, name);
	if (!_font_cache_load_font_file_in_memory (fid)) {
		ctx->status = VKE_STATUS_FILE_NOT_FOUND;
		return;
	}
	select_font_face (name);
}
void VkgContext::load_font_from_memory (unsigned char* fontBuffer, long fontBufferByteSize, const char* name) {
	if (ctx->status)
		return;
	//RECORD(data, VKVG_CMD_SET_FONT_PATH, name);
	vkg_font_identity* fid = _font_cache_add_font_identity (NULL, name);
	fid->fontBuffer = fontBuffer;
	fid->fontBufSize = fontBufferByteSize;

	select_font_face (name);
}
void VkgContext::set_font_size (uint32_t size){
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_SET_FONT_SIZE, size);
#ifdef VKVG_USE_FREETYPE
	long newSize = size << 6;
#else
	long newSize = size;
#endif
	if (ctx->selectedCharSize == newSize)
		return;
	ctx->selectedCharSize = newSize;
	ctx->currentFont = NULL;
	ctx->currentFontSize = NULL;
}

void VkgContext::set_text_direction (vkg_context* ctx, vkg_direction direction){

}

void VkgContext::show_text (const char* text){
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_SHOW_TEXT, text);
	vke_log(VKE_LOG_INFO_CMD, "CMD: show_text:\n");
	//ensure_renderpass_is_started(ctx);
	_font_cache_show_text (text);
	//_flush_undrawn_vertices (ctx);
}


/// replace macro and templates with -> gen (gen-erative); they are real functions and made inline.  the c99 compiler typically does a good number on those.  this is reduction
/// 

// caller:: scope makes sense, you should be able to access vars from there
// this has implications and mostly good ones lol
// restrict to 1 caller vars and only ones that are referenced
// add namespaces of course, replace with namespace_*.  the compiler will bark at you instead of naming it an alternate

VkgText VkgContext::text_run_create (const char* text) {
	if (ctx->status)
		return NULL;
	return font_cache_create_text_run(text, -1);
}

VkgText VkgContext::text_run_create_with_length (const char* text, uint32_t length) {
	if (ctx->status)
		return NULL;
	vkg_text_run *tr = io_new(vkg_text_run);
	font_cache_create_text_run(text, length, tr);
	return tr;
}

uint32_t VkgContext::text_run_get_glyph_count (VkgText textRun) {
	return textRun->glyph_count;
}

void VkgContext::text_run_get_glyph_position (VkgText textRun,
									   uint32_t index,
									   vkg_glyph_info_t* pGlyphInfo) {
	if (index >= textRun->glyph_count) {
		*pGlyphInfo = (vkg_glyph_info_t){0};
		return;
	}
#if VKVG_USE_HARFBUZZ
	memcpy (pGlyphInfo, &textRun->glyphs[index], sizeof(vkg_glyph_info_t));
#else
	*pGlyphInfo = textRun->glyphs[index];
#endif
}

void VkgContext::text_run_destroy(VkgText textRun) {
	_font_cache_destroy_text_run (textRun);
	free (textRun);
}

void VkgContext::show_text_run (VkgText textRun) {
	if (ctx->status)
		return;
	_font_cache_show_text_run(textRun);
}

void VkgContext::text_run_get_extents (VkgText textRun, vkg_text_extents* extents) {
	*extents = textRun->extents;
}

void VkgContext::text_extents (const char* text, vkg_text_extents* extents) {
	if (ctx->status)
		return;
	_font_cache_text_extents(text, -1, extents);
}

void VkgContext::font_extents (vkg_font_extents* extents) {
	if (ctx->status)
		return;
	_font_cache_font_extents(extents);
}

void VkgContext::save (VkgContext ctx){
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_SAVE);
	vke_log(VKE_LOG_INFO, "SAVE CONTEXT: ctx = %p\n", ctx);

	VkgDevice dev = ctx->dev;
	vkg_context_save* sav = (vkg_context_save*)calloc(1,sizeof(vkg_context_save));

	_flush_cmd_buff (ctx);
	if (!_wait_ctx_flush_end (ctx)) {
		free (sav);
		return;
	}

	if (ctx->curClipState == vkg_clip_state_clip) {
		sav->clippingState = vkg_clip_state_clip_saved;

		uint8_t curSaveStencil = ctx->curSavBit / 6;

		if (ctx->curSavBit > 0 && ctx->curSavBit % 6 == 0){//new save/restore stencil image have to be created
			VkeImage* savedStencilsPtr = NULL;
			if (savedStencilsPtr)
				savedStencilsPtr = (VkeImage*)realloc(ctx->savedStencils, curSaveStencil * sizeof(VkeImage));
			else
				savedStencilsPtr = (VkeImage*)malloc(curSaveStencil * sizeof(VkeImage));
			if (savedStencilsPtr == NULL) {
				free(sav);
				ctx->status = VKE_STATUS_NO_MEMORY;
				return;
			}
			ctx->savedStencils = savedStencilsPtr;
			VkeImage savStencil = vke_image_ms_create ((VkeDevice)dev, dev->stencilFormat, dev->samples, ctx->pSurf->width, ctx->pSurf->height,
									VKE_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
			ctx->savedStencils[curSaveStencil-1] = savStencil;

			vke_cmd_begin (ctx->cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
			ctx->cmdStarted = true;

	#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
			vke_cmd_label_start(ctx->cmd, "new save/restore stencil", DBG_LAB_COLOR_SAV);
	#endif

			vke_image_set_layout (ctx->cmd, ctx->pSurf->stencil, dev->stencilAspectFlag,
								  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
								  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
			vke_image_set_layout (ctx->cmd, savStencil, dev->stencilAspectFlag,
								  VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								  VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

			VkImageCopy cregion = { .srcSubresource = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 0, 1},
									.dstSubresource = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 0, 1},
									.extent = {ctx->pSurf->width,ctx->pSurf->height,1}};
			vkCmdCopyImage(ctx->cmd,
						   vke_image_get_vkimage(ctx->pSurf->stencil),VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						   vke_image_get_vkimage(savStencil),		 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						   1, &cregion);

			vke_image_set_layout (ctx->cmd, ctx->pSurf->stencil, dev->stencilAspectFlag,
								  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
								  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);

	#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
			vke_cmd_label_end (ctx->cmd);
	#endif

			VK_CHECK_RESULT(vkEndCommandBuffer(ctx->cmd));
			_wait_and_submit_cmd(ctx);
		}

		uint8_t curSaveBit = 1 << (ctx->curSavBit % 6 + 2);

		_start_cmd_for_render_pass (ctx);

	#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
		vke_cmd_label_start(ctx->cmd, "save rp", DBG_LAB_COLOR_SAV);
	#endif

		CmdBindPipeline			(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->dev->pipelineClipping);

		CmdSetStencilReference	(ctx->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_CLIP_BIT|curSaveBit);
		CmdSetStencilCompareMask(ctx->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_CLIP_BIT);
		CmdSetStencilWriteMask	(ctx->cmd, VK_STENCIL_FRONT_AND_BACK, curSaveBit);

		draw_full_screen_quad (NULL);

		bind_draw_pipeline (ctx);
		CmdSetStencilCompareMask(ctx->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_CLIP_BIT);

	#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
		vke_cmd_label_end (ctx->cmd);
	#endif
		ctx->curSavBit++;
	} else if (ctx->curClipState == vkg_clip_state_none)
		sav->clippingState = (_get_previous_clip_state(ctx) & 0x03);
	else
		sav->clippingState = vkg_clip_state_clear;

	sav->dashOffset = ctx->dashOffset;
	sav->dashCount	= ctx->dashCount;
	if (ctx->dashCount > 0) {
		sav->dashes = (float*)malloc (sizeof(float) * ctx->dashCount);
		memcpy (sav->dashes, ctx->dashes, sizeof(float) * ctx->dashCount);
	}
	sav->lineWidth	= ctx->lineWidth;
	sav->miterLimit	= ctx->miterLimit;
	sav->curOperator= ctx->curOperator;
	sav->lineCap	= ctx->lineCap;
	sav->lineWidth	= ctx->lineWidth;
	sav->curFillRule= ctx->curFillRule;

	sav->selectedCharSize = ctx->selectedCharSize;
	strcpy (sav->selectedFontName, ctx->selectedFontName);

	sav->currentFont  = ctx->currentFont;
	sav->textDirection= ctx->textDirection;
	sav->pushConsts	  = ctx->pushConsts;
	if (ctx->pattern) {
		sav->pattern = ctx->pattern;//TODO:pattern sav must be imutable (copy?)
		io_grab(vkg_pattern, ctx->pattern);
	} else
		sav->curColor = ctx->curColor;

	sav->pNext		= ctx->pSavedCtxs;
	ctx->pSavedCtxs = sav;

}
void VkgContext::restore (VkgContext ctx){
	if (ctx->status)
		return;

	RECORD(data, VKVG_CMD_RESTORE);

	if (ctx->pSavedCtxs == NULL){
		ctx->status = VKE_STATUS_INVALID_RESTORE;
		return;
	}

	vke_log(VKE_LOG_INFO, "RESTORE CONTEXT: ctx = %p\n", ctx);

	vkg_context_save* sav = ctx->pSavedCtxs;
	ctx->pSavedCtxs = sav->pNext;

	_flush_cmd_buff (ctx);
	if (!_wait_ctx_flush_end (ctx))
		return;

	ctx->pushConsts	  = sav->pushConsts;
	ctx->pushCstDirty = true;

	if (ctx->curClipState) {//!=none
		if (ctx->curClipState == vkg_clip_state_clip && sav->clippingState == vkg_clip_state_clear) {
			_reset_clip (ctx);
		} else {

			uint8_t curSaveBit = 1 << ((ctx->curSavBit-1) % 6 + 2);

			_start_cmd_for_render_pass (ctx);

#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
			vke_cmd_label_start(ctx->cmd, "restore rp", DBG_LAB_COLOR_SAV);
#endif

			CmdBindPipeline			(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->dev->pipelineClipping);

			CmdSetStencilReference	(ctx->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_CLIP_BIT|curSaveBit);
			CmdSetStencilCompareMask(ctx->cmd, VK_STENCIL_FRONT_AND_BACK, curSaveBit);
			CmdSetStencilWriteMask	(ctx->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_CLIP_BIT);

			draw_full_screen_quad (NULL);

			bind_draw_pipeline (ctx);
			CmdSetStencilCompareMask (ctx->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_CLIP_BIT);

#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
			vke_cmd_label_end (ctx->cmd);
#endif

			_flush_cmd_buff (ctx);
			if (!_wait_ctx_flush_end (ctx))
				return;
		}
	}
	if (sav->clippingState == vkg_clip_state_clip_saved) {
		ctx->curSavBit--;

		uint8_t curSaveStencil = ctx->curSavBit / 6;
		if (ctx->curSavBit > 0 && ctx->curSavBit % 6 == 0){//addtional save/restore stencil image have to be copied back to surf stencil first
			VkeImage savStencil = ctx->savedStencils[curSaveStencil-1];

			vke_cmd_begin (ctx->cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
			ctx->cmdStarted = true;

#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
			vke_cmd_label_start(ctx->cmd, "additional stencil copy while restoring", DBG_LAB_COLOR_SAV);
#endif

			vke_image_set_layout (ctx->cmd, ctx->pSurf->stencil, ctx->dev->stencilAspectFlag,
								  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
			vke_image_set_layout (ctx->cmd, savStencil, ctx->dev->stencilAspectFlag,
								  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
								  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

			VkImageCopy cregion = { .srcSubresource = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 0, 1},
									.dstSubresource = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 0, 1},
									.extent = {ctx->pSurf->width,ctx->pSurf->height,1}};
			vkCmdCopyImage(ctx->cmd,
						   vke_image_get_vkimage (savStencil),		 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						   vke_image_get_vkimage (ctx->pSurf->stencil),VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						   1, &cregion);
			vke_image_set_layout (ctx->cmd, ctx->pSurf->stencil, ctx->dev->stencilAspectFlag,
								  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
								  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);

#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
			vke_cmd_label_end (ctx->cmd);
#endif

			VK_CHECK_RESULT(vkEndCommandBuffer(ctx->cmd));
			_wait_and_submit_cmd (ctx);
			if (!_wait_ctx_flush_end (ctx))
				return;
			vke_image_drop(savStencil);
		}
	}

	ctx->curClipState = vkg_clip_state_none;

	ctx->dashOffset = sav->dashOffset;
	if (ctx->dashCount > 0)
		free (ctx->dashes);
	ctx->dashCount	= sav->dashCount;
	if (ctx->dashCount > 0) {
		ctx->dashes = (float*)malloc (sizeof(float) * ctx->dashCount);
		memcpy (ctx->dashes, sav->dashes, sizeof(float) * ctx->dashCount);
	}

	ctx->lineWidth	= sav->lineWidth;
	ctx->miterLimit	= sav->miterLimit;
	ctx->curOperator= sav->curOperator;
	ctx->lineCap	= sav->lineCap;
	ctx->lineJoin	= sav->lineJoint;
	ctx->curFillRule= sav->curFillRule;

	ctx->selectedCharSize = sav->selectedCharSize;
	strcpy (ctx->selectedFontName, sav->selectedFontName);

	ctx->currentFont  = sav->currentFont;
	ctx->textDirection= sav->textDirection;

	if (sav->pattern) {
		if (sav->pattern != ctx->pattern)
			_update_cur_pattern (ctx, sav->pattern);
		else
			VkgPattern::drop(sav->pattern);
	} else {
		ctx->curColor = sav->curColor;
		_update_cur_pattern (ctx, NULL);
	}

	_free_ctx_save(sav);
}

void VkgContext::translate (float dx, float dy){
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_TRANSLATE, dx, dy);
	vke_log(VKE_LOG_INFO_CMD, "CMD: translate: %f, %f\n", dx, dy);
	emit_draw_cmd_undrawn_vertices(ctx);
	vkg_matrix_translate (&ctx->pushConsts.mat, dx, dy);
	_set_mat_inv_and_vkCmdPush (ctx);
}
void VkgContext::scale (float sx, float sy){
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_SCALE, sx, sy);
	vke_log(VKE_LOG_INFO_CMD, "CMD: scale: %f, %f\n", sx, sy);
	emit_draw_cmd_undrawn_vertices(ctx);
	vkg_matrix_scale (&ctx->pushConsts.mat, sx, sy);
	_set_mat_inv_and_vkCmdPush (ctx);
}
void VkgContext::rotate (float radians){
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_ROTATE, radians);
	vke_log(VKE_LOG_INFO_CMD, "CMD: rotate: %f\n", radians);
	emit_draw_cmd_undrawn_vertices(ctx);
	vkg_matrix_rotate (&ctx->pushConsts.mat, radians);
	_set_mat_inv_and_vkCmdPush (ctx);
}
void VkgContext::transform (const vkg_matrix* matrix) {
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_TRANSFORM, matrix);
	vke_log(VKE_LOG_INFO_CMD, "CMD: transform: %f, %f, %f, %f, %f, %f\n", matrix->xx, matrix->yx, matrix->xy, matrix->yy, matrix->x0, matrix->y0);
	emit_draw_cmd_undrawn_vertices(ctx);
	vkg_matrix res;
	vkg_matrix_multiply (&res, &ctx->pushConsts.mat, matrix);
	ctx->pushConsts.mat = res;
	_set_mat_inv_and_vkCmdPush (ctx);
}
void VkgContext::identity_matrix (VkgContext ctx) {
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_IDENTITY_MATRIX);
	vke_log(VKE_LOG_INFO_CMD, "CMD: identity_matrix:\n");
	emit_draw_cmd_undrawn_vertices(ctx);
	vkg_matrix im = VKVG_IDENTITY_MATRIX;
	ctx->pushConsts.mat = im;
	_set_mat_inv_and_vkCmdPush (ctx);
}
void VkgContext::set_matrix (const vkg_matrix* matrix){
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_SET_MATRIX, matrix);
	vke_log(VKE_LOG_INFO_CMD, "CMD: set_matrix: %f, %f, %f, %f, %f, %f\n", matrix->xx, matrix->yx, matrix->xy, matrix->yy, matrix->x0, matrix->y0);
	emit_draw_cmd_undrawn_vertices(ctx);
	ctx->pushConsts.mat = (*matrix);
	_set_mat_inv_and_vkCmdPush (ctx);
}
void VkgContext::get_matrix (vkg_matrix* const matrix){
	*matrix = ctx->pushConsts.mat;
}

void VkgContext::elliptic_arc_to (float x2, float y2, bool largeArc, bool sweepFlag, float rx, float ry, float phi) {
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_ELLIPTICAL_ARC_TO, x2, y2, rx, ry, phi, largeArc, sweepFlag);
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: elliptic_arc_to: x2:%10.5f y2:%10.5f large:%d sweep:%d rx:%10.5f ry:%10.5f phi:%10.5f \n", x2,y2,largeArc,sweepFlag,rx,ry,phi);
	float x1, y1;
	vkg_get_current_point(ctx, &x1, &y1);
	_elliptic_arc(ctx, x1, y1, x2, y2, largeArc, sweepFlag, rx, ry, phi);
}
void VkgContext::rel_elliptic_arc_to (float x2, float y2, bool largeArc, bool sweepFlag, float rx, float ry, float phi) {
	if (ctx->status)
		return;
	RECORD(data, VKVG_CMD_REL_ELLIPTICAL_ARC_TO, x2, y2, rx, ry, phi, largeArc, sweepFlag);
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: rel_elliptic_arc_to: x2:%10.5f y2:%10.5f large:%d sweep:%d rx:%10.5f ry:%10.5f phi:%10.5f \n", x2,y2,largeArc,sweepFlag,rx,ry,phi);

	float x1, y1;
	vkg_get_current_point(ctx, &x1, &y1);
	_elliptic_arc(ctx, x1, y1, x2+x1, y2+y1, largeArc, sweepFlag, rx, ry, phi);
}

void VkgContext::ellipse (float radiusX, float radiusY, float x, float y, float rotationAngle) {
	if (ctx->status)
		return;
	vke_log(VKE_LOG_INFO_CMD, "CMD: ellipse:\n");

	float width_two_thirds = radiusX * 4 / 3;

	float dx1 = sinf(rotationAngle) * radiusY;
	float dy1 = cosf(rotationAngle) * radiusY;
	float dx2 = cosf(rotationAngle) * width_two_thirds;
	float dy2 = sinf(rotationAngle) * width_two_thirds;

	float topCenterX = x - dx1;
	float topCenterY = y + dy1;
	float topRightX = topCenterX + dx2;
	float topRightY = topCenterY + dy2;
	float topLeftX = topCenterX - dx2;
	float topLeftY = topCenterY - dy2;

	float bottomCenterX = x + dx1;
	float bottomCenterY = y - dy1;
	float bottomRightX = bottomCenterX + dx2;
	float bottomRightY = bottomCenterY + dy2;
	float bottomLeftX = bottomCenterX - dx2;
	float bottomLeftY = bottomCenterY - dy2;

	finish_path(ctx);
	add_point (ctx, bottomCenterX, bottomCenterY);

	_curve_to (ctx, bottomRightX, bottomRightY, topRightX, topRightY, topCenterX, topCenterY);
	_curve_to (ctx, topLeftX, topLeftY, bottomLeftX, bottomLeftY, bottomCenterX, bottomCenterY);

	ctx->pathes[ctx->pathPtr] |= PATH_CLOSED_BIT;
	finish_path(ctx);
}

VkgSurface VkgContext::get_target (VkgContext ctx) {
	if (ctx->status)
		return NULL;
	return ctx->pSurf;
}

const char *
vkg_status_to_string (VkeStatus status) {
	switch (status) {
	case VKE_STATUS_SUCCESS:
		return "no error has occurred";
	case VKE_STATUS_NO_MEMORY:
		return "out of memory";
	case VKE_STATUS_INVALID_RESTORE:
		return "vkg_restore() without matching vkg_save()";
	case VKE_STATUS_NO_CURRENT_POINT:
		return "no current point defined";
	case VKE_STATUS_INVALID_MATRIX:
		return "invalid matrix (not invertible)";
	case VKE_STATUS_INVALID_STATUS:
		return "invalid value for an input VkeStatus";
	case VKE_STATUS_INVALID_INDEX:
		return "invalid index passed to getter";
	case VKE_STATUS_NULL_POINTER:
		return "NULL pointer";
	case VKE_STATUS_WRITE_ERROR:
		return "error while writing to output stream";
	case VKE_STATUS_PATTERN_TYPE_MISMATCH:
		return "the pattern type is not appropriate for the operation";
	case VKE_STATUS_PATTERN_INVALID_GRADIENT:
		return "the stops count is zero";
	case VKE_STATUS_INVALID_FORMAT:
		return "invalid value for an input vkg_format_t";
	case VKE_STATUS_FILE_NOT_FOUND:
		return "file not found";
	case VKE_STATUS_INVALID_DASH:
		return "invalid value for a dash setting";
	case VKE_STATUS_INVALID_RECT:
		return "a rectangle has the height or width equal to 0";
	case VKE_STATUS_TIMEOUT:
		return "waiting for a Vulkan operation to finish resulted in a fence timeout (5 seconds)";
	case VKE_STATUS_DEVICE_ERROR:
		return "the initialization of the device resulted in an error";
	case VKE_STATUS_INVALID_IMAGE:
		return "invalid image";
	case VKE_STATUS_INVALID_SURFACE:
		return "invalid surface";
	case VKE_STATUS_INVALID_FONT:
		return "unresolved font name";
	default:
		return "<unknown error status>";
	}
}
