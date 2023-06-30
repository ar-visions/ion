
#include <vkg/internal.hh>

//credits for bezier algorithms to:
//		Anti-Grain Geometry (AGG) - Version 2.5
//		A high quality rendering engine for C++
//		Copyright (C) 2002-2006 Maxim Shemanarev
//		Contact: mcseem@antigrain.com
//				 mcseemagg@yahoo.com
//				 http://antigrain.com

#include <vke/vke.hh>
#include <glutess/glutess.h>

namespace ion {

void VkgContext::resize_vertex_cache (uint32_t newSize) {
	VkgVertex* tmp = (VkgVertex*) ::realloc (data->vertexCache, (size_t)newSize * sizeof(VkgVertex));
	vke_log(VKE_LOG_DBG_ARRAYS, "resize vertex cache (vx count=%u): old size: %u -> new size: %u size(byte): %zu Ptr: %p -> %p\n",
		data->vertCount, data->sizeVertices, newSize, (size_t)newSize * sizeof(VkgVertex), data->vertexCache, tmp);
	if (tmp == NULL) {
		data->status = VKE_STATUS_NO_MEMORY;
		vke_log(VKE_LOG_ERR, "resize vertex cache failed: vert count: %u byte size: %zu\n", newSize, newSize * sizeof(VkgVertex));
		return;
	}
	data->vertexCache = tmp;
	data->sizeVertices = newSize;
}

void VkgContext::resize_index_cache (uint32_t newSize) {
	VKVG_IBO_INDEX_TYPE* tmp = (VKVG_IBO_INDEX_TYPE*) ::realloc (data->indexCache, (size_t)newSize * sizeof(VKVG_IBO_INDEX_TYPE));
	vke_log(VKE_LOG_DBG_ARRAYS, "resize IBO: new size: %lu Ptr: %p -> %p\n", (size_t)newSize * sizeof(VKVG_IBO_INDEX_TYPE), data->indexCache, tmp);
	if (tmp == NULL) {
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
	resize_vertex_cache (newSize);
}

void VkgContext::check_vertex_cache_size () {
	assert(data->sizeVertices > data->vertCount);
	if (data->sizeVertices - VKVG_ARRAY_THRESHOLD > data->vertCount)
		return;
	resize_vertex_cache (data->sizeVertices + VKVG_VBO_SIZE);
}

void VkgContext::ensure_index_cache_size (uint32_t addedIndicesCount) {
	assert(data->sizeIndices > data->indCount);
	if (data->sizeIndices - VKVG_ARRAY_THRESHOLD > data->indCount + addedIndicesCount)
		return;
	uint32_t newSize = data->sizeIndices + addedIndicesCount;
	uint32_t modulo = addedIndicesCount % VKVG_IBO_SIZE;
	if (modulo > 0)
		newSize += VKVG_IBO_SIZE - modulo;
	resize_index_cache (newSize);
}

void VkgContext::check_index_cache_size () {
	if (data->sizeIndices - VKVG_ARRAY_THRESHOLD > data->indCount)
		return;
	resize_index_cache (data->sizeIndices + VKVG_IBO_SIZE);
}

//check host path array size, return true if error. pathPtr is already incremented
bool VkgContext::check_pathes_array () {
	if (data->sizePathes - data->pathPtr - data->segmentPtr > VKVG_ARRAY_THRESHOLD)
		return false;
	data->sizePathes += VKVG_PATHES_SIZE;
	uint32_t* tmp = (uint32_t*) ::realloc (data->pathes, (size_t)data->sizePathes * sizeof(uint32_t));
	vke_log(VKE_LOG_DBG_ARRAYS, "resize PATH: new size: %u Ptr: %p -> %p\n", data->sizePathes, data->pathes, tmp);
	if (tmp == NULL) {
		data->status = VKE_STATUS_NO_MEMORY;
		vke_log(VKE_LOG_ERR, "resize PATH failed: new size(byte): %zu\n", data->sizePathes * sizeof(uint32_t));
		clear_path();
		return true;
	}
	data->pathes = tmp;
	return false;
}

//check host point array size, return true if error
bool VkgContext::check_point_array() {
	if (data->sizePoints - VKVG_ARRAY_THRESHOLD > data->pointCount)
		return false;
	data->sizePoints += VKVG_PTS_SIZE;
	vec2f* tmp = (vec2f*) ::realloc (data->points, (size_t)data->sizePoints * sizeof(vec2f));
	vke_log(VKE_LOG_DBG_ARRAYS, "resize Points: new size(point): %u Ptr: %p -> %p\n", data->sizePoints, data->points, tmp);
	if (tmp == NULL) {
		data->status = VKE_STATUS_NO_MEMORY;
		vke_log(VKE_LOG_ERR, "resize PATH failed: new size(byte): %zu\n", data->sizePoints * sizeof(vec2f));
		clear_path ();
		return true;
	}
	data->points = tmp;
	return false;
}

bool VkgContext::current_path_is_empty () {
	return data->pathes [data->pathPtr] == 0;
}

//this function expect that current point exists
vec2f* VkgContext::get_current_position () {
	return &data->points[data->pointCount-1];
}

//set curve start point and set path has curve bit
void VkgContext::set_curve_start () {
	if (data->segmentPtr > 0) {
		//check if current segment has points (straight)
		if ((data->pathes [data->pathPtr + data->segmentPtr]&PATH_ELT_MASK) > 0)
			data->segmentPtr++;
	} else {
		//not yet segmented path, first segment length is copied
		if (data->pathes [data->pathPtr] > 0) {//create first straight segment first
			data->pathes [data->pathPtr + 1] = data->pathes [data->pathPtr];
			data->segmentPtr = 2;
		} else
			data->segmentPtr = 1;
	}
	check_pathes_array();
	data->pathes [data->pathPtr + data->segmentPtr] = 0;
}

//compute segment length and set is curved bit
void VkgContext::set_curve_end () {
	//data->pathes [data->pathPtr + data->segmentPtr] = data->pathes [data->pathPtr] - data->pathes [data->pathPtr + data->segmentPtr];
	data->pathes [data->pathPtr + data->segmentPtr] |= PATH_HAS_CURVES_BIT;
	data->segmentPtr++;
	check_pathes_array();
	data->pathes [data->pathPtr + data->segmentPtr] = 0;
}

//path start pointed at ptrPath has curve bit
bool VkgContext::path_has_curves (uint32_t ptrPath) {
	return data->pathes[ptrPath] & PATH_HAS_CURVES_BIT;
}

void VkgContext::finish_path () {
	if (data->pathes [data->pathPtr] == 0)//empty
		return;
	if ((data->pathes [data->pathPtr]&PATH_ELT_MASK) < 2) {
		//only current pos is in path
		data->pointCount -= data->pathes[data->pathPtr];//what about the bounds?
		data->pathes[data->pathPtr] = 0;
		data->segmentPtr = 0;
		return;
	}

	vke_log(VKE_LOG_INFO_PATH, "PATH: points count=%10d\n", data->pathes[data->pathPtr]&PATH_ELT_MASK);

	if (data->pathPtr == 0 && data->simpleConvex)
		data->pathes[0] |= PATH_IS_CONVEX_BIT;

	if (data->segmentPtr > 0) {//pathes having curves are segmented
		data->pathes[data->pathPtr] |= PATH_HAS_CURVES_BIT;
		//curved segment increment segmentPtr on curve end,
		//so if last segment is not a curve and point count > 0
		if ((data->pathes[data->pathPtr+data->segmentPtr]&PATH_HAS_CURVES_BIT)==0 &&
				(data->pathes[data->pathPtr+data->segmentPtr]&PATH_ELT_MASK) > 0)
			data->segmentPtr++;//current segment has to be included
		data->pathPtr += data->segmentPtr;
	} else
		data->pathPtr ++;

	if (check_pathes_array())
		return;

	data->pathes[data->pathPtr] = 0;
	data->segmentPtr = 0;
	data->subpathCount++;
	data->simpleConvex = false;
}

//clear path datas in context
void VkgContext::clear_path () {
	data->pathPtr = 0;
	data->pathes [data->pathPtr] = 0;
	data->pointCount = 0;
	data->segmentPtr = 0;
	data->subpathCount = 0;
	data->simpleConvex = false;
}

void VkgContext::remove_last_point () {
	data->pathes[data->pathPtr]--;
	data->pointCount--;
	if (data->segmentPtr > 0) {//if path is segmented
		if (!data->pathes [data->pathPtr + data->segmentPtr])//if current segment is empty
			data->segmentPtr--;
		data->pathes [data->pathPtr + data->segmentPtr]--;//decrement last segment point count
		if ((data->pathes [data->pathPtr + data->segmentPtr]&PATH_ELT_MASK) == 0)//if no point left (was only one)
			data->pathes [data->pathPtr + data->segmentPtr] = 0;//reset current segment
		else if (data->pathes [data->pathPtr + data->segmentPtr]&PATH_HAS_CURVES_BIT)//if segment is a curve
			data->segmentPtr++;//then segPtr has to be forwarded to new segment
	}
}

bool VkgContext::path_is_closed (uint32_t ptrPath) {
	return data->pathes[ptrPath] & PATH_CLOSED_BIT;
}

void VkgContext::add_point (float x, float y) {
	if (check_point_array())
		return;
	if (isnan(x) || isnan(y)) {
		vke_log(VKE_LOG_DEBUG, "add_point: (%f, %f)\n", x, y);
		return;
	}
	vec2f v = {x,y};
	/*if (!current_path_is_empty() && vec2f_len(vec2f_sub(data->points[data->pointCount-1], v))<1.f)
		return;*/
	vke_log(VKE_LOG_INFO_PTS, "add_point: (%f, %f)\n", x, y);

	data->points[data->pointCount] = v;
	data->pointCount++;//total point count of pathes, (for array bounds check)
	data->pathes[data->pathPtr]++;//total point count in path
	if (data->segmentPtr > 0)
		data->pathes[data->pathPtr + data->segmentPtr]++;//total point count in path's segment
}

float normalizeAngle(float a)
{
	float res = ROUND_DOWN(fmodf(a, 2.0f * M_PIF), 100);
	if (res < 0.0f)
		res += 2.0f * M_PIF;
	return res;
}

float VkgContext::get_arc_step(float radius) {
	float sx, sy;
	data->pushConsts.mat.get_scale(&sx, &sy);
	float r = radius * fabsf(fmaxf(sx,sy));
	if (r < 30.0f)
		return fminf(M_PIF / 3.f, M_PIF / r);
	return fminf(M_PIF / 3.f,M_PIF / (r * 0.4f));
}

void VkgContext::create_gradient_buff() {
	data->uboGrad = VkeBuffer(data->dev->vke,
		VkeBufferUsage::UNIFORM_BUFFER,// VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VKE_MEMORY_USAGE_CPU_TO_GPU,
		sizeof(vkg_gradient), true);
}

void VkgContext::create_vertices_buff() {
	data->vertices = VkeBuffer(data->dev->vke,
		VkeBufferUsage::VERTEX_BUFFER, //VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VKE_MEMORY_USAGE_CPU_TO_GPU,
		data->sizeVBO * sizeof(VkgVertex), true);
	data->indices = VkeBuffer(data->dev->vke,
		VkeBufferUsage::INDEX_BUFFER, //VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VKE_MEMORY_USAGE_CPU_TO_GPU,
		data->sizeIBO * sizeof(VKVG_IBO_INDEX_TYPE), true);
}

void VkgContext::resize_vbo(size_t new_size) {
	if (!wait_ctx_flush_end())//wait previous cmd if not completed
		return;
	vke_log(VKE_LOG_DBG_ARRAYS, "resize VBO: %d -> ", data->sizeVBO);
	data->sizeVBO = u32(new_size);
	uint32_t mod = data->sizeVBO % VKVG_VBO_SIZE;
	if (mod > 0)
		data->sizeVBO += VKVG_VBO_SIZE - mod;
	vke_log(VKE_LOG_DBG_ARRAYS, "%d\n", data->sizeVBO);
	data->vertices.resize(data->sizeVBO * sizeof(VkgVertex), true);
}

void VkgContext::resize_ibo(size_t new_size) {
	if (!wait_ctx_flush_end())//wait previous cmd if not completed
		return;
	data->sizeIBO = new_size;
	uint32_t mod = data->sizeIBO % VKVG_IBO_SIZE;
	if (mod > 0)
		data->sizeIBO += VKVG_IBO_SIZE - mod;
	vke_log(VKE_LOG_DBG_ARRAYS, "resize IBO: new size: %d\n", data->sizeIBO);	
	data->indices.resize(data->sizeIBO * sizeof(VKVG_IBO_INDEX_TYPE), true);
}

void VkgContext::add_vertexf(float x, float y) {
	VkgVertex* pVert = &data->vertexCache[data->vertCount];
	pVert->pos.x = x;
	pVert->pos.y = y;
	pVert->color = data->curColor;
	pVert->uv.z = -1;
	vke_log(VKE_LOG_INFO_VBO, "Add Vertexf %10d: pos:(%10.4f, %10.4f) uv:(%10.4f,%10.4f,%10.4f) color:0x%.8x \n", data->vertCount, pVert->pos.x, pVert->pos.y, pVert->uv.x, pVert->uv.y, pVert->uv.z, pVert->color);
	data->vertCount++;
	check_vertex_cache_size();
}

void VkgContext::add_vertexf_unchecked(float x, float y) {
	VkgVertex* pVert = &data->vertexCache[data->vertCount];
	pVert->pos.x = x;
	pVert->pos.y = y;
	pVert->color = data->curColor;
	pVert->uv.z = -1;
	vke_log(VKE_LOG_INFO_VBO, "Add Vertexf %10d: pos:(%10.4f, %10.4f) uv:(%10.4f,%10.4f,%10.4f) color:0x%.8x \n", data->vertCount, pVert->pos.x, pVert->pos.y, pVert->uv.x, pVert->uv.y, pVert->uv.z, pVert->color);
	data->vertCount++;
}

void VkgContext::add_vertex(VkgVertex &v) {
	data->vertexCache[data->vertCount] = v;
	vke_log(VKE_LOG_INFO_VBO, "Add VkgVertex  %10d: pos:(%10.4f, %10.4f) uv:(%10.4f,%10.4f,%10.4f) color:0x%.8x \n", data->vertCount, v.pos.x, v.pos.y, v.uv.x, v.uv.y, v.uv.z, v.color);
	data->vertCount++;
	check_vertex_cache_size();
}

void VkgContext::set_vertex(uint32_t idx, VkgVertex v) {
	data->vertexCache[idx] = v;
}

void VkgContext::add_indice(VKVG_IBO_INDEX_TYPE i) {
	data->indexCache[data->indCount++] = i;
	check_index_cache_size();
}

void VkgContext::add_indice_for_fan(VKVG_IBO_INDEX_TYPE i) {
	VKVG_IBO_INDEX_TYPE* inds = &data->indexCache[data->indCount];
	inds[0] = data->tesselator_fan_start;
	inds[1] = data->indexCache[data->indCount-1];
	inds[2] = i;
	data->indCount+=3;
	check_index_cache_size();
}

void VkgContext::add_indice_for_strip(VKVG_IBO_INDEX_TYPE i, bool odd) {
	VKVG_IBO_INDEX_TYPE* inds = &data->indexCache[data->indCount];
	if (odd) {
		inds[0] = data->indexCache[data->indCount-2];
		inds[1] = i;
		inds[2] = data->indexCache[data->indCount-1];
	} else {
		inds[0] = data->indexCache[data->indCount-1];
		inds[1] = data->indexCache[data->indCount-2];
		inds[2] = i;
	}
	data->indCount+=3;
	check_index_cache_size();
}

void VkgContext::add_tri_indices_for_rect (VKVG_IBO_INDEX_TYPE i) {
	VKVG_IBO_INDEX_TYPE* inds = &data->indexCache[data->indCount];
	inds[0] = i;
	inds[1] = i+2;
	inds[2] = i+1;
	inds[3] = i+1;
	inds[4] = i+2;
	inds[5] = i+3;
	data->indCount+=6;

	check_index_cache_size();
	vke_log(VKE_LOG_INFO_IBO, "Rectangle IDX: %d %d %d | %d %d %d (count=%d)\n", inds[0], inds[1], inds[2], inds[3], inds[4], inds[5], data->indCount);
}

void VkgContext::add_triangle_indices(VKVG_IBO_INDEX_TYPE i0, VKVG_IBO_INDEX_TYPE i1, VKVG_IBO_INDEX_TYPE i2) {
	VKVG_IBO_INDEX_TYPE* inds = &data->indexCache[data->indCount];
	inds[0] = i0;
	inds[1] = i1;
	inds[2] = i2;
	data->indCount+=3;

	check_index_cache_size();
	vke_log(VKE_LOG_INFO_IBO, "Triangle IDX: %d %d %d (indCount=%d)\n", i0,i1,i2,data->indCount);
}

void VkgContext::add_triangle_indices_unchecked (VKVG_IBO_INDEX_TYPE i0, VKVG_IBO_INDEX_TYPE i1, VKVG_IBO_INDEX_TYPE i2) {
	VKVG_IBO_INDEX_TYPE* inds = &data->indexCache[data->indCount];
	inds[0] 		= i0;
	inds[1] 		= i1;
	inds[2] 		= i2;
	data->indCount += 3;
	vke_log(VKE_LOG_INFO_IBO, "Triangle IDX: %d %d %d (indCount=%d)\n", i0,i1,i2,data->indCount);
}

void VkgContext::vao_add_rectangle (float x, float y, float width, float height) {
	VkgVertex v[4] =
	{
		{{x,y},				data->curColor, {0,0,-1}},
		{{x,y+height},		data->curColor, {0,0,-1}},
		{{x+width,y},		data->curColor, {0,0,-1}},
		{{x+width,y+height},data->curColor, {0,0,-1}}
	};
	VKVG_IBO_INDEX_TYPE firstIdx = (VKVG_IBO_INDEX_TYPE)(data->vertCount - data->curVertOffset);
	VkgVertex* pVert = &data->vertexCache[data->vertCount];
	memcpy (pVert,v,4*sizeof(VkgVertex));
	data->vertCount+=4;

	check_vertex_cache_size();

	add_tri_indices_for_rect(firstIdx);
}

//start render pass if not yet started or update push const if requested
void VkgContext::ensure_renderpass_is_started () {
	vke_log(VKE_LOG_INFO, "ensure_renderpass_is_started\n");
	if (!data->cmdStarted)
		start_cmd_for_render_pass();
	else if (data->pushCstDirty)
		update_push_constants();
}

void VkgContext::create_cmd_buff () {
	data->cmdBuffers = VkeCommandBuffer(data->dev->vke, data->cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 2);
#if defined(DEBUG) && defined(ENABLE_VALIDATION)
	vke_device_set_object_name(data->pSurf->dev->vke, VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT, (uint64_t)data->cmd, "vkvgCtxCmd");
#endif
}

void VkgContext::clear_attachment () {
}

bool VkgContext::wait_ctx_flush_end () {
	vke_log(VKE_LOG_INFO, "CTX: _wait_flush_fence\n");
#ifdef VKVG_ENABLE_VK_TIMELINE_SEMAPHORE
	if (vke_timeline_wait (data->dev->vke, data->pSurf->timeline, data->timelineStep) == VK_SUCCESS)
		return true;
#else
	if (WaitForFences (data->dev->vke, 1, &data->flushFence, VK_TRUE, VKVG_FENCE_TIMEOUT) == VK_SUCCESS)
		return true;
#endif
	vke_log(VKE_LOG_DEBUG, "CTX: _wait_flush_fence timeout\n");
	data->status = VKE_STATUS_TIMEOUT;
	return false;
}

bool VkgContext::wait_and_submit_cmd () {
	if (!data->cmdStarted)//current cmd buff is empty, be aware that wait is also canceled!!
		return true;

	vke_log(VKE_LOG_INFO, "CTX: wait_and_submit_cmd\n");

#ifdef VKVG_ENABLE_VK_TIMELINE_SEMAPHORE
	VkgSurface surf = data->pSurf;
	VkgDevice dev = surf->dev;
	//vke_timeline_wait (dev->vke, surf->timeline, ct->timelineStep);
	if (data->pattern && data->pattern->type == VKVG_PATTERN_TYPE_SURFACE) {
		//add source surface timeline sync.
		VkgSurface source = (VkgSurface)data->pattern->data;

		io_sync(surf);
		io_sync(source);
		io_sync(dev);

		vke_cmd_submit_timelined2 (dev->gQueue, &data->cmd,
								  (VkSemaphore[2]) {surf->timeline,source->timeline},
								  (uint64_t[2]) {surf->timelineStep,source->timelineStep},
								  (uint64_t[2]) {surf->timelineStep+1,source->timelineStep+1});
		surf->timelineStep++;
		source->timelineStep++;
		data->timelineStep = surf->timelineStep;
		mtx_unlock(&dev->mutex);

		io_unsync(source);
		io_unsync(surf);
	} else {
		io_sync(surf);
		io_sync(dev);
		vke_cmd_submit_timelined (dev->gQueue, &data->cmd, surf->timeline, surf->timelineStep, surf->timelineStep+1);
		surf->timelineStep++;
		data->timelineStep = surf->timelineStep;
		io_unsync(dev);
		io_unsync(surf);
	}
#else

	if (!wait_ctx_flush_end ())
		return false;
	ResetFences (data->dev->vke, 1, &data->flushFence);
	_device_submit_cmd (data->dev, &data->cmd, data->flushFence);
#endif

	if (data->cmd == data->cmdBuffers[0])
		data->cmd = data->cmdBuffers[1];
	else
		data->cmd = data->cmdBuffers[0];

	ResetCommandBuffer (data->cmd, 0);
	data->cmdStarted = false;
	return true;
}

//pre flush vertices because of vbo or ibo too small, all vertices except last draw call are flushed
//this function expects a vertex offset > 0
void VkgContext::flush_vertices_caches_until_vertex_base() {
	wait_ctx_flush_end();

	memcpy(data->vertices.get_mapped_pointer(), data->vertexCache, data->curVertOffset * sizeof(VkgVertex));
	memcpy(data->indices .get_mapped_pointer(), data->indexCache,  data->curIndStart   * sizeof(VKVG_IBO_INDEX_TYPE));

	//copy remaining vertices and indices to caches starts
	//this could be optimized at the cost of additional offsets.
	data->vertCount -= data->curVertOffset;
	data->indCount -= data->curIndStart;
	memcpy(data->vertexCache, &data->vertexCache[data->curVertOffset], data->vertCount * sizeof (VkgVertex));
	memcpy(data->indexCache, &data->indexCache[data->curIndStart], data->indCount * sizeof (VKVG_IBO_INDEX_TYPE));

	data->curVertOffset = 0;
	data->curIndStart   = 0;
}

//copy vertex and index caches to the vbo and ibo vkbuffers used by gpu for drawing
//current running cmd has to be completed to free usage of those
void VkgContext::flush_vertices_caches() {
	if (!wait_ctx_flush_end ())
		return;

	memcpy(data->vertices.get_mapped_pointer(), data->vertexCache, data->vertCount * sizeof (VkgVertex));
	memcpy(data->indices.get_mapped_pointer(), data->indexCache, data->indCount * sizeof (VKVG_IBO_INDEX_TYPE));

	data->vertCount = data->indCount = data->curIndStart = data->curVertOffset = 0;
}

//this func expect cmdStarted to be true
void VkgContext::end_render_pass () {
	vke_log(VKE_LOG_INFO, "END RENDER PASS: data = %p;\n", data);
	CmdEndRenderPass	  (data->cmd);
#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	vke_cmd_label_end (data->cmd);
#endif
	data->renderPassBeginInfo->renderPass = data->dev->renderPass;
}

void VkgContext::check_vao_size () {
	if (data->vertCount > data->sizeVBO || data->indCount > data->sizeIBO) {
		//vbo or ibo buffers too small
		if (data->cmdStarted)
			//if cmd is started buffers, are already bound, so no resize is possible
			//instead we flush, and clear vbo and ibo caches
			flush_cmd_until_vx_base ();
		if (data->vertCount > data->sizeVBO)		
			resize_vbo(data->sizeVertices);
		if (data->indCount > data->sizeIBO)
			resize_ibo(data->sizeIndices);
	}
}

//stroke and non-zero draw call for solid color flush
void VkgContext::emit_draw_cmd_undrawn_vertices () {
	if (data->indCount == data->curIndStart)
		return;

	check_vao_size ();
	ensure_renderpass_is_started ();

#ifdef VKVG_WIRED_DEBUG
	if (vkg_wired_debug&vkg_wired_debug_mode_normal)
		CmdDrawIndexed(data->cmd, data->indCount - data->curIndStart, 1, data->curIndStart, (int32_t)data->curVertOffset, 0);
	if (vkg_wired_debug&vkg_wired_debug_mode_lines) {
		CmdBindPipeline(data->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, data->pSurf->dev->pipelineLineList);
		CmdDrawIndexed(data->cmd, data->indCount - data->curIndStart, 1, data->curIndStart, (int32_t)data->curVertOffset, 0);
	}
	if (vkg_wired_debug&vkg_wired_debug_mode_points) {
		CmdBindPipeline(data->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, data->pSurf->dev->pipelineWired);
		CmdDrawIndexed(data->cmd, data->indCount - data->curIndStart, 1, data->curIndStart, (int32_t)data->curVertOffset, 0);
	}
	if (vkg_wired_debug&vkg_wired_debug_mode_both)
		CmdBindPipeline(data->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, data->pSurf->dev->pipe_OVER);
#else
	CmdDrawIndexed(data->cmd, data->indCount - data->curIndStart, 1, data->curIndStart, (int32_t)data->curVertOffset, 0);
#endif
	vke_log(VKE_LOG_INFO, "RECORD DRAW CMD: data = %p; vertices = %d; indices = %d (vxOff = %d idxStart = %d idxTot = %d )\n",
		data, data->vertCount - data->curVertOffset,
		data->indCount - data->curIndStart, data->curVertOffset, data->curIndStart, data->indCount);

	data->curIndStart = data->indCount;
	data->curVertOffset = data->vertCount;
}
//preflush vertices with drawcommand already emited
void VkgContext::flush_cmd_until_vx_base () {
	end_render_pass ();
	if (data->curVertOffset > 0) {
		vke_log(VKE_LOG_INFO, "FLUSH UNTIL VX BASE CTX: data = %p; vertices = %d; indices = %d\n", data, data->vertCount, data->indCount);
		flush_vertices_caches_until_vertex_base ();
	}
	data->cmd.finish();
	wait_and_submit_cmd();
}
void VkgContext::flush_cmd_buff () {
	emit_draw_cmd_undrawn_vertices ();
	if (!data->cmdStarted)
		return;
	end_render_pass();
	vke_log(VKE_LOG_INFO, "FLUSH CTX: data = %p; vertices = %d; indices = %d\n", data, data->vertCount, data->indCount);
	flush_vertices_caches();
	data->cmd.finish();
	wait_and_submit_cmd();
}

//bind correct draw pipeline depending on current OPERATOR
void VkgContext::bind_draw_pipeline () {
	switch (data->curOperator) {
	case VKVG_OPERATOR_OVER:
		CmdBindPipeline(data->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, data->dev->pipe_OVER);
		break;
	case VKVG_OPERATOR_CLEAR:
		CmdBindPipeline(data->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, data->dev->pipe_CLEAR);
		break;
	case VKVG_OPERATOR_DIFFERENCE:
		CmdBindPipeline(data->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, data->dev->pipe_SUB);
		break;
	default:
		CmdBindPipeline(data->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, data->dev->pipe_OVER);
		break;
	}
}
#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
const float DBG_LAB_COLOR_RP[4]		= {0,0,1,1};
const float DBG_LAB_COLOR_FSQ[4]	= {1,0,0,1};
#endif

void VkgContext::start_cmd_for_render_pass () {
	vke_log(VKE_LOG_INFO, "START RENDER PASS: data = %p\n", data);
	data->cmd.cmd_begin(VkeCommandBufferUsage::one_time_submit);

	/// data->dev->threadAware seems irrelevant to this op so i am removing
	if (data->pSurf->img->layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
		VkeImage imgMs = data->pSurf->imgMS;
		if (imgMs != NULL)
			imgMs.set_layout(data->cmd, VK_IMAGE_ASPECT_COLOR_BIT,
						VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		data->pSurf->img.set_layout(data->cmd, VK_IMAGE_ASPECT_COLOR_BIT,
						VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		data->pSurf->stencil.set_layout(data->cmd, data->dev->stencilAspectFlag,
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
						VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
	}

#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	vke_cmd_label_start(data->cmd, "data render pass", DBG_LAB_COLOR_RP);
#endif

	CmdBeginRenderPass (data->cmd, &data->renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	VkViewport viewport = {0,0,(float)data->pSurf->width,(float)data->pSurf->height,0,1.f};
	CmdSetViewport(data->cmd, 0, 1, &viewport);

	CmdSetScissor(data->cmd, 0, 1, &data->bounds);

	VkDescriptorSet dss[] = {data->dsFont, data->dsSrc,data->dsGrad};
	CmdBindDescriptorSets(data->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, data->dev->pipelineLayout,
							0, 3, dss, 0, NULL);

	VkDeviceSize offsets[1] = { 0 };
	CmdBindVertexBuffers(data->cmd, 0, 1, &data->vertices.buffer, offsets);
	CmdBindIndexBuffer(data->cmd, data->indices.buffer, 0, VKVG_VK_INDEX_TYPE);

	update_push_constants	();

	bind_draw_pipeline ();
	CmdSetStencilCompareMask(data->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_CLIP_BIT);
	data->cmdStarted = true;
}
//compute inverse mat used in shader when context matrix has changed
//then trigger push constants command
void VkgContext::set_mat_inv_and_vkCmdPush () {
	data->pushConsts.matInv = data->pushConsts.mat;
	vkg_matrix_invert (&data->pushConsts.matInv);
	data->pushCstDirty = true;
}
void VkgContext::update_push_constants () {
	CmdPushConstants(data->cmd, data->dev->pipelineLayout,
					   VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constants),&data->pushConsts);
	data->pushCstDirty = false;
}
void VkgContext::update_cur_pattern (VkgPattern pat) {
	VkgPattern lastPat = data->pattern;
	data->pattern = pat;

	uint32_t newPatternType = VKVG_PATTERN_TYPE_SOLID;

	vke_log(VKE_LOG_INFO, "CTX: update_cur_pattern: %p -> %p\n", lastPat, pat);

	if (pat == NULL) {//solid color
		if (lastPat == NULL)//solid
			return;//solid to solid transition, no extra action requested
	} else
		newPatternType = pat->type;

	switch (newPatternType)	 {
	case VKVG_PATTERN_TYPE_SOLID:
		flush_cmd_buff				();
		if (!wait_ctx_flush_end ())
			return;
		if (lastPat->type == VKVG_PATTERN_TYPE_SURFACE)//unbind current source surface by replacing it with empty texture
			update_descriptor_set		(data->dev->emptyImg, data->dsSrc);
		break;
	case VKVG_PATTERN_TYPE_SURFACE:
	{
		emit_draw_cmd_undrawn_vertices();

		VkgSurface surf = (VkgSurface)pat->data;

		//flush data in two steps to add the src transitioning in the cmd buff
		if (data->cmdStarted) {//transition of img without appropriate dependencies in subpass must be done outside renderpass.
			end_render_pass ();
			flush_vertices_caches ();
		} else {
			data->cmd.begin(VkeCommandBufferUsage::one_time_submit);
			data->cmdStarted = true;
		}

		//transition source surface for sampling
		vke_image_set_layout (data->cmd, surf->img, VK_IMAGE_ASPECT_COLOR_BIT,
							  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
							  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

		data->cmd.finish();
		wait_and_submit_cmd	();
		if (!wait_ctx_flush_end ())
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

		update_descriptor_set (surf->img, data->dsSrc);

		if (pat->hasMatrix) {

		}

		data->pushConsts.source.width	= (float)surf->width;
		data->pushConsts.source.height	= (float)surf->height;
		break;
	}
	case VKVG_PATTERN_TYPE_LINEAR:
	case VKVG_PATTERN_TYPE_RADIAL:
		flush_cmd_buff ();
		if (!wait_ctx_flush_end ())
			return;

		if (lastPat && lastPat->type == VKVG_PATTERN_TYPE_SURFACE)
			update_descriptor_set (data->dev->emptyImg, data->dsSrc);

		vec4 bounds = {{(float)data->pSurf->width}, {(float)data->pSurf->height}, {0}, {0}};//store img bounds in unused source field
		data->pushConsts.source = bounds;

		//transform control point with current data matrix
		vkg_gradient grad = *(vkg_gradient*)pat->data;

		if (grad.count < 2) {
			data->status = VKE_STATUS_PATTERN_INVALID_GRADIENT;
			return;
		}
		vkg_matrix mat;
		if (pat->hasMatrix) {
			VkgPattern::get_matrix (pat, &mat);
			if (vkg_matrix_invert (&mat) != VKE_STATUS_SUCCESS)
				mat = VKVG_IDENTITY_MATRIX;
		}

		if (pat->hasMatrix)
			vkg_matrix_transform_point (&mat, &grad.cp[0].x, &grad.cp[0].y);
		vkg_matrix_transform_point (&data->pushConsts.mat, &grad.cp[0].x, &grad.cp[0].y);
		if (pat->type == VKVG_PATTERN_TYPE_LINEAR) {
			if (pat->hasMatrix)
				vkg_matrix_transform_point (&mat, &grad.cp[0].z, &grad.cp[0].w);
			vkg_matrix_transform_point (&data->pushConsts.mat, &grad.cp[0].z, &grad.cp[0].w);
		} else {
			if (pat->hasMatrix)
				vkg_matrix_transform_point (&mat, &grad.cp[1].x, &grad.cp[1].y);
			vkg_matrix_transform_point (&data->pushConsts.mat, &grad.cp[1].x, &grad.cp[1].y);

			//radii
			if (pat->hasMatrix) {
				vkg_matrix_transform_distance (&mat, &grad.cp[0].z, &grad.cp[0].w);
				vkg_matrix_transform_distance (&mat, &grad.cp[1].z, &grad.cp[0].w);
			}
			vkg_matrix_transform_distance (&data->pushConsts.mat, &grad.cp[0].z, &grad.cp[0].w);
			vkg_matrix_transform_distance (&data->pushConsts.mat, &grad.cp[1].z, &grad.cp[0].w);
		}

		memcpy (vke_buffer_get_mapped_pointer(&data->uboGrad) , &grad, sizeof(vkg_gradient));
		vke_buffer_flush(&data->uboGrad);
		break;
	}
	data->pushConsts.fsq_patternType = (data->pushConsts.fsq_patternType & FULLSCREEN_BIT) + newPatternType;
	data->pushCstDirty = true;
	if (lastPat)
		VkgPattern::drop(lastPat);
}
void VkgContext::update_descriptor_set (VkeImage img, VkDescriptorSet ds) {
	VkDescriptorImageInfo descSrcTex = vke_image_get_descriptor (img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkWriteDescriptorSet writeDescriptorSet = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = ds,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &descSrcTex
	};
	vkUpdateDescriptorSets(data->dev->vke->vkdev, 1, &writeDescriptorSet, 0, NULL);
}

void VkgContext::update_gradient_desc_set () {
	VkDescriptorBufferInfo dbi = {data->uboGrad.buffer, 0, VK_WHOLE_SIZE};
	VkWriteDescriptorSet writeDescriptorSet = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = data->dsGrad,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &dbi
	};
	vkUpdateDescriptorSets(data->dev->vke->vkdev, 1, &writeDescriptorSet, 0, NULL);
}
/*
 * Reset currently bound descriptor which image could be destroyed
 */
/*void _reset_src_descriptor_set () {
	VkgDevice dev = data->pSurf->dev;
	//VkDescriptorSet dss[] = {data->dsSrc};
	vkFreeDescriptorSets	(dev->vke->vkdev, data->descriptorPool, 1, &data->dsSrc);

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
															  .descriptorPool = data->descriptorPool,
															  .descriptorSetCount = 1,
															  .pSetLayouts = &dev->dslSrc };
	VK_CHECK_RESULT(vkAllocateDescriptorSets(dev->vke->vkdev, &descriptorSetAllocateInfo, &data->dsSrc));
}*/

void VkgContext::createDescriptorPool () {
	VkgDevice dev = data->dev;
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
	VK_CHECK_RESULT(vkCreateDescriptorPool (dev->vke->vkdev, &descriptorPoolCreateInfo, NULL, &data->descriptorPool));
}
void VkgContext::init_descriptor_sets () {
	VkgDevice dev = data->dev;
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
															  .descriptorPool = data->descriptorPool,
															  .descriptorSetCount = 1,
															  .pSetLayouts = &dev->dslFont
															};
	VK_CHECK_RESULT(vkAllocateDescriptorSets(dev->vke->vkdev, &descriptorSetAllocateInfo, &data->dsFont));
	descriptorSetAllocateInfo.pSetLayouts = &dev->dslSrc;
	VK_CHECK_RESULT(vkAllocateDescriptorSets(dev->vke->vkdev, &descriptorSetAllocateInfo, &data->dsSrc));
	descriptorSetAllocateInfo.pSetLayouts = &dev->dslGrad;
	VK_CHECK_RESULT(vkAllocateDescriptorSets(dev->vke->vkdev, &descriptorSetAllocateInfo, &data->dsGrad));
}
void VkgContext::release_context_resources () {
	VkDevice dev = data->dev->vke->vkdev;
	
#ifndef VKVG_ENABLE_VK_TIMELINE_SEMAPHORE
	vkDestroyFence (dev, data->flushFence, NULL);
#endif

	vkFreeCommandBuffers(dev, data->cmdPool, 2, data->cmdBuffers);
	vkDestroyCommandPool(dev, data->cmdPool, NULL);

	VkDescriptorSet dss[] = {data->dsFont, data->dsSrc, data->dsGrad};
	vkFreeDescriptorSets	(dev, data->descriptorPool, 3, dss);

	vkDestroyDescriptorPool (dev, data->descriptorPool,NULL);

	vke_buffer_reset (&data->uboGrad);
	vke_buffer_reset (&data->indices);
	vke_buffer_reset (&data->vertices);

	free(data->vertexCache);
	free(data->indexCache);

	vke_image_drop	  (data->fontCacheImg);
	//TODO:check this for source counter
	//vke_image_drop	  (data->source);

	free(data->pathes);
	free(data->points);

	free();
}
//populate vertice buff for stroke
bool VkgContext::build_vb_step(stroke_context* str, bool isCurve) {
	VkgVertex v = {{0},data->curColor, {0,0,-1}};
	vec2f p0 = data->points[str->cp];
	vec2f v0 = vec2f_sub(p0, data->points[str->iL]);
	vec2f v1 = vec2f_sub(data->points[str->iR], p0);
	float length_v0 = vec2f_len(v0);
	float length_v1 = vec2f_len(v1);
	if (length_v0 < FLT_EPSILON || length_v1 < FLT_EPSILON) {
		vke_log(VKE_LOG_STROKE, "vb_step discard, length<epsilon: l0:%f l1:%f\n", length_v0, length_v1);
		return false;
	}
	vec2f v0n = vec2f_div_s (v0, length_v0);
	vec2f v1n = vec2f_div_s (v1, length_v1);
	float dot = vec2f_dot (v0n, v1n);
	float det = v0n.x * v1n.y - v0n.y * v1n.x;
	if (EQUF(dot,1.0f)) {//colinear
		vke_log(VKE_LOG_STROKE, "vb_step discard, dot==1\n");
		return false;
	}

	if (EQUF(dot,-1.0f)) {//cusp (could draw line butt?)
		vec2f vPerp = vec2f_scale(vec2f_perp (v0n), str->hw);

		VKVG_IBO_INDEX_TYPE idx = (VKVG_IBO_INDEX_TYPE)(data->vertCount - data->curVertOffset);
	
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

	VKVG_IBO_INDEX_TYPE idx = (VKVG_IBO_INDEX_TYPE)(data->vertCount - data->curVertOffset);

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

	vkg_line_join join = data->lineJoin;

	if (isCurve) {
		if (dot < 0.8f)
			join = VKVG_LINE_JOIN_ROUND;
		else
			join = VKVG_LINE_JOIN_MITER;
	}

	if (join == VKVG_LINE_JOIN_MITER) {
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

		if (det<0) {
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

		if (join == VKVG_LINE_JOIN_BEVEL) {
			if (det<0) {
				add_triangle_indices(idx, idx+2, idx+1);
				add_triangle_indices(idx+2, idx+4, idx+0);
				add_triangle_indices(idx, idx+3, idx+4);
			} else {
				add_triangle_indices(idx, idx+2, idx+1);
				add_triangle_indices(idx+2, idx+3, idx+1);
				add_triangle_indices(idx+1, idx+3, idx+4);
			}
		} else if (join == VKVG_LINE_JOIN_ROUND) {
			if (!str->arcStep)
				str->arcStep = get_arc_step (str->hw);
			float a = acosf(vp.x);
			if (vp.y < 0)
				a = -a;

			if (det<0) {
				a+=M_PIF;
				float a1 = a + alpha;
				a-=str->arcStep;
				while (a > a1) {
					add_vertexf(cosf(a) * str->hw + p0.x, sinf(a) * str->hw + p0.y);
					a-=str->arcStep;
				}
			} else {
				float a1 = a + alpha;
				a+=str->arcStep;
				while (a < a1) {
					add_vertexf(cosf(a) * str->hw + p0.x, sinf(a) * str->hw + p0.y);
					a+=str->arcStep;
				}
			}
			VKVG_IBO_INDEX_TYPE p0Idx = (VKVG_IBO_INDEX_TYPE)(data->vertCount - data->curVertOffset);
			add_triangle_indices(idx, idx+2, idx+1);
			if (det < 0) {
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
	VkgVertex v = {{0},data->curColor,{0,0,-1}};

	VKVG_IBO_INDEX_TYPE firstIdx = (VKVG_IBO_INDEX_TYPE)(data->vertCount - data->curVertOffset);

	if (isStart) {
		vec2f vhw;
		vec2f_scale(vhw, n, str->hw);

		if (data->lineCap == VKVG_LINE_CAP_SQUARE)
			vec2f_sub(p0, p0, vhw);

		vec2f_perp(vhw, vhw);

		if (data->lineCap == VKVG_LINE_CAP_ROUND) {
			if (!str->arcStep)
				str->arcStep = get_arc_step (str->hw);

			float a = acosf(n.x) + M_PIF_2;
			if (n.y < 0)
				a = M_PIF-a;
			float a1 = a + M_PIF;

			a += str->arcStep;
			while (a < a1) {
				add_vertexf (cosf(a) * str->hw + p0.x, sinf(a) * str->hw + p0.y);
				a += str->arcStep;
			}
			VKVG_IBO_INDEX_TYPE p0Idx = (VKVG_IBO_INDEX_TYPE)(data->vertCount - data->curVertOffset);
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

		if (data->lineCap == VKVG_LINE_CAP_SQUARE)
			p0 = vec2f_add (p0, vhw);

		vhw = vec2f_perp (vhw);

		v.pos = vec2f_add (p0, vhw);
		add_vertex (v);
		v.pos = vec2f_sub (p0, vhw);
		add_vertex (v);

		firstIdx = (VKVG_IBO_INDEX_TYPE)(data->vertCount - data->curVertOffset);

		if (data->lineCap == VKVG_LINE_CAP_ROUND) {
			if (!str->arcStep)
				str->arcStep = get_arc_step (str->hw);

			float a = acosf(n.x)+ M_PIF_2;
			if (n.y < 0)
				a = M_PIF-a;
			float a1 = a - M_PIF;

			a -= str->arcStep;
			while ( a > a1) {
				add_vertexf (cosf(a) * str->hw + p0.x, sinf(a) * str->hw + p0.y);
				a -= str->arcStep;
			}

			VKVG_IBO_INDEX_TYPE p0Idx = (VKVG_IBO_INDEX_TYPE)(data->vertCount - data->curVertOffset - 1);
			for (VKVG_IBO_INDEX_TYPE p = firstIdx-1 ; p < p0Idx; p++)
				add_triangle_indices (p+1, p, firstIdx-2);
		}
	}
}
float VkgContext::draw_dashed_segment (stroke_context* str, dash_context* dc, bool isCurve) {
	//vec2f pL = data->points[str->iL];
	vec2f p = data->points[str->cp];
	vec2f pR = data->points[str->iR];

	if (!dc->dashOn)//we test in fact the next dash start, if dashOn = true => next segment is a void.
		build_vb_step (str, isCurve);

	vec2f d = vec2f_sub (pR, p);
	dc->normal = vec2f_norm (d);
	float segmentLength = vec2f_len(d);

	while (dc->curDashOffset < segmentLength) {
		vec2f p0 = vec2f_add (p, vec2f_scale(dc->normal, dc->curDashOffset));

		draw_stoke_cap (str, p0, dc->normal, dc->dashOn);
		dc->dashOn ^= true;
		dc->curDashOffset += data->dashes[dc->curDash];
		if (++dc->curDash == data->dashCount)
			dc->curDash = 0;
	}
	dc->curDashOffset -= segmentLength;
	dc->curDashOffset = fmodf(dc->curDashOffset, dc->totDashLength);
	return segmentLength;
}
void VkgContext::draw_segment (stroke_context* str, dash_context* dc, bool isCurve) {
	str->iR = str->cp + 1;
	if (data->dashCount > 0)
		draw_dashed_segment(str, dc, isCurve);
	else
		build_vb_step (str, isCurve);
	str->iL = str->cp++;
	if (data->vertCount - data->curVertOffset > VKVG_IBO_MAX / 3) {
		VkgVertex v0 = data->vertexCache[data->curVertOffset + str->firstIdx];
		VkgVertex v1 = data->vertexCache[data->curVertOffset + str->firstIdx + 1];
		emit_draw_cmd_undrawn_vertices();
		//repeat first 2 vertices for closed pathes
		str->firstIdx = (VKVG_IBO_INDEX_TYPE)(data->vertCount - data->curVertOffset);
		add_vertex(v0);
		add_vertex(v1);
		data->curVertOffset = data->vertCount;//prevent redrawing them at the start of the batch
	}
}

bool ptInTriangle(vec2f p, vec2f p0, vec2f p1, vec2f p2) {
	float dX = p.x-p2.x;
	float dY = p.y-p2.y;
	float dX21 = p2.x-p1.x;
	float dY12 = p1.y-p2.y;
	float D = dY12*(p0.x-p2.x) + dX21*(p0.y-p2.y);
	float s = dY12*dX + dX21*dY;
	float t = (p2.y-p0.y)*dX + (p0.x-p2.x)*dY;
	if (D<0)
		return (s<=0) && (t<=0) && (s+t>=D);
	return (s>=0) && (t>=0) && (s+t<=D);
}

void free_ctx_save (vkg_context_save* sav) {
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
	if (!current_path_is_empty ()) {
		//prevent adding the same point
		if (vec2f_equ (get_current_position (), p))
			return;
	}
	add_point (x, y);
	data->simpleConvex = false;
}
void VkgContext::elliptic_arc (float x1, float y1, float x2, float y2, bool largeArc, bool counterClockWise, float _rx, float _ry, float phi) {
	if (_rx==0||_ry==0) {
		if (current_path_is_empty())
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
	double lambda = powf (p1.x, 2) / powf (rx, 2) + powf (p1.y, 2) / powf (ry, 2);
	if (lambda > 1) {
		lambda = sqrtf (lambda);
		rx *= lambda;
		ry *= lambda;
	}

	p = (vec2f) {rx * p1.y / ry, -ry * p1.x / rx};

	vec2f cp = vec2f_scale (p, sqrtf (fabsf (
		(powf (rx,2) * powf (ry,2) - powf (rx,2) * powf (p1.y, 2) - powf (ry,2) * powf (p1.x, 2)) /
		(powf (rx,2) * powf (p1.y, 2) + powf (ry,2) * powf (p1.x, 2))
	)));

	if (largeArc == counterClockWise)
		vec2f_inv(cp);

	m = (mat2) {
		{cosf (phi),-sinf (phi)},
		{sinf (phi), cosf (phi)}
	};
	p = (vec2f) {(x1 + x2)/2, (y1 + y2)/2};
	vec2f c = vec2f_add (mat2_mult_vec2(m, cp) , p);

	vec2f u = vec2f_unit_x;
	vec2f v = {(p1.x-cp.x)/rx, (p1.y-cp.y)/ry};
	double sa = acosf (vec2f_dot (u, v) / (fabsf(vec2f_len(v)) * fabsf(vec2f_len(u))));
	if (isnan(sa))
		sa=M_PIF;
	if (u.x*v.y-u.y*v.x < 0)
		sa = -sa;

	u = v;
	v = (vec2f) {(-p1.x-cp.x)/rx, (-p1.y-cp.y)/ry};
	double delta_theta = acosf (vec2f_dot (u, v) / (fabsf(vec2f_len (v)) * fabsf(vec2f_len (u))));
	if (isnan(delta_theta))
		delta_theta=M_PIF;
	if (u.x*v.y-u.y*v.x < 0)
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

	float step = fmaxf(0.001f, fminf(M_PIF, get_arc_step(fminf (rx, ry))*0.1f));

	p = (vec2f) {
		rx * cosf (theta),
		ry * sinf (theta)
	};
	vec2f xy = vec2f_add (mat2_mult_vec2 (m, p), c);

	if (current_path_is_empty()) {
		set_curve_start ();
		add_point (xy.x, xy.y);
		if (!data->pathPtr)
			data->simpleConvex = true;
		else
			data->simpleConvex = false;
	} else {
		line_to(xy.x, xy.y);
		set_curve_start ();
		data->simpleConvex = false;
	}

	set_curve_start ();

	if (sa < ea) {
		theta += step;
		while (theta < ea) {
			p = (vec2f) {
				rx * cosf (theta),
				ry * sinf (theta)
			};
			xy = vec2f_add (mat2_mult_vec2 (m, p), c);
			add_point (xy.x, xy.y);
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
			add_point (xy.x, xy.y);
			theta -= step;
		}
	}
	p = (vec2f) {
		rx * cosf (ea),
		ry * sinf (ea)
	};
	xy = vec2f_add (mat2_mult_vec2 (m, p), c);
	add_point (xy.x, xy.y);
	set_curve_end();
}


//Even-Odd inside test with stencil buffer implementation.
void VkgContext::poly_fill (vec4* bounds) {
	//we anticipate the check for vbo buffer size, ibo is not used in poly_fill
	//the polyfill emit a single vertex for each point in the path.
	if (data->sizeVBO - VKVG_ARRAY_THRESHOLD <  data->vertCount + data->pointCount) {
		if (data->cmdStarted) {
			_end_render_pass ();
			if (data->vertCount > 0)
				_flush_vertices_caches ();
			
			wait_and_submit_cmd ();
			wait_ctx_flush_end ();
			if (data->sizeVBO - VKVG_ARRAY_THRESHOLD < data->pointCount) {
				_resize_vbo (data->pointCount + VKVG_ARRAY_THRESHOLD);
				resize_vertex_cache (data->sizeVBO);
			}
		} else {
			_resize_vbo (data->vertCount + data->pointCount + VKVG_ARRAY_THRESHOLD);
			resize_vertex_cache (data->sizeVBO);
		}

		start_cmd_for_render_pass();
	} else {
		ensure_vertex_cache_size (data->pointCount);
		ensure_renderpass_is_started ();
	}

	CmdBindPipeline (data->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, data->dev->pipelinePolyFill);

	VkgVertex v = {{0}, data->curColor, {0,0,-1}};
	uint32_t ptrPath = 0;
	uint32_t firstPtIdx = 0;

	while (ptrPath < data->pathPtr) {
		uint32_t pathPointCount = data->pathes[ptrPath] & PATH_ELT_MASK;
		if (pathPointCount > 2) {
			VKVG_IBO_INDEX_TYPE firstVertIdx = (VKVG_IBO_INDEX_TYPE)data->vertCount;

			for (uint32_t i = 0; i < pathPointCount; i++) {
				v.pos = data->points [i+firstPtIdx];
				data->vertexCache[data->vertCount++] = v;
				if (!bounds)
					continue;
				//bounds are computed here to scissor the painting operation
				//that speed up fill drastically.
				vkg_matrix_transform_point (&data->pushConsts.mat, &v.pos.x, &v.pos.y);

				if (v.pos.x < bounds->xMin)
					bounds->x_min = v.pos.x;
				if (v.pos.x > bounds->x_max)
					bounds->x_max = v.pos.x;
				if (v.pos.y < bounds->y_min)
					bounds->y_min = v.pos.y;
				if (v.pos.y > bounds->y_max)
					bounds->y_max = v.pos.y;
			}

			vke_log(VKE_LOG_INFO_PATH, "\tpoly fill: point count = %d; 1st vert = %d; vert count = %d\n", pathPointCount, firstVertIdx, data->vertCount - firstVertIdx);
			CmdDraw (data->cmd, pathPointCount, 1, firstVertIdx , 0);
		}
		firstPtIdx += pathPointCount;

		if (path_has_curves (ptrPath)) {
			//skip segments lengths used in stroke
			ptrPath++;
			uint32_t totPts = 0;
			while (totPts < pathPointCount)
				totPts += (data->pathes[ptrPath++] & PATH_ELT_MASK);
		} else
			ptrPath++;
	}
	data->curVertOffset = data->vertCount;
}

void fan_vertex2(VKVG_IBO_INDEX_TYPE v, VkgContext data) {
	VKVG_IBO_INDEX_TYPE i = (VKVG_IBO_INDEX_TYPE)v;
	switch (data->tesselator_idx_counter) {
	case 0:
		add_indice(i);
		data->tesselator_fan_start = i;
		data->tesselator_idx_counter ++;
		break;
	case 1:
	case 2:
		add_indice(i);
		data->tesselator_idx_counter ++;
		break;
	default:
		add_indice_for_fan (i);
		break;
	}
}

void strip_vertex2(VKVG_IBO_INDEX_TYPE v, VkgContext data) {
	VKVG_IBO_INDEX_TYPE i = (VKVG_IBO_INDEX_TYPE)v;
	if (data->tesselator_idx_counter < 3) {
		add_indice(i);
	} else
		add_indice_for_strip(i, data->tesselator_idx_counter % 2);
	data->tesselator_idx_counter ++;
}

void triangle_vertex2 (VKVG_IBO_INDEX_TYPE v, VkgContext data) {
	VKVG_IBO_INDEX_TYPE i = (VKVG_IBO_INDEX_TYPE)v;
	add_indice(i);
}
void skip_vertex2 (VKVG_IBO_INDEX_TYPE v, VkgContext data) {}
void begin2(GLenum which, void *poly_data)
{
	VkgContext data = (VkgContext)poly_data;
	switch (which) {
	case GL_TRIANGLES:
		data->vertex_cb = &triangle_vertex2;
		break;
	case GL_TRIANGLE_STRIP:
		data->tesselator_idx_counter = 0;
		data->vertex_cb = &strip_vertex2;
		break;
	case GL_TRIANGLE_FAN:
		data->tesselator_idx_counter = data->tesselator_fan_start = 0;
		data->vertex_cb = &fan_vertex2;
		break;
	default:
		fprintf(stderr, "ERROR, can't handle %d\n", (int)which);
		data->vertex_cb = &skip_vertex2;
	}
}

void combine2(const GLdouble newVertex[3],
			 const void *neighborVertex_s[4],
			 const GLfloat neighborWeight[4], void **outData, void *poly_data)
{
	VkgContext data = (VkgContext)poly_data;
	VkgVertex v = {{newVertex[0],newVertex[1]},data->curColor, {0,0,-1}};
	*outData = (void*)((unsigned long)(data->vertCount - data->curVertOffset));
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
	VkgContext data = (VkgContext)poly_data;
	data->vertex_cb(i, data);
}

void fill_non_zero () {
	VkgVertex v = {{0},data->curColor, {0,0,-1}};

	uint32_t ptrPath = 0;
	uint32_t firstPtIdx = 0;

	if (data->pathPtr == 1 && data->pathes[0] & PATH_IS_CONVEX_BIT) {
		//simple concave rectangle or circle
		VKVG_IBO_INDEX_TYPE firstVertIdx = (VKVG_IBO_INDEX_TYPE)(data->vertCount - data->curVertOffset);
		uint32_t pathPointCount = data->pathes[ptrPath] & PATH_ELT_MASK;

		ensure_vertex_cache_size(pathPointCount);
		ensure_index_cache_size((pathPointCount-2)*3);

		VKVG_IBO_INDEX_TYPE i = 0;
		while (i < 2) {
			v.pos = data->points [i++];
			set_vertex (data->vertCount++, v);
		}
		while (i < pathPointCount) {
			v.pos = data->points [i];
			set_vertex (data->vertCount++, v);
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

	gluTessBeginPolygon(tess, data);

	while (ptrPath < data->pathPtr) {
		uint32_t pathPointCount = data->pathes[ptrPath] & PATH_ELT_MASK;

		if (pathPointCount > 2) {
			VKVG_IBO_INDEX_TYPE firstVertIdx = (VKVG_IBO_INDEX_TYPE)(data->vertCount - data->curVertOffset);
			gluTessBeginContour(tess);

			VKVG_IBO_INDEX_TYPE i = 0;

			while (i < pathPointCount) {
				v.pos = data->points[i+firstPtIdx];
				double dp[] = {v.pos.x,v.pos.y,0};
				add_vertex(v);
				gluTessVertex(tess, dp, (void*)((unsigned long)firstVertIdx + i));
				i++;
			}
			gluTessEndContour(tess);

			//limit batch size here to 1/3 of the ibo index type ability
			//if (data->vertCount - data->curVertOffset > VKVG_IBO_MAX / 3)
			//	emit_draw_cmd_undrawn_vertices();
		}

		firstPtIdx += pathPointCount;
		if (path_has_curves (ptrPath)) {
			//skip segments lengths used in stroke
			ptrPath++;
			uint32_t totPts = 0;
			while (totPts < pathPointCount)
				totPts += (data->pathes[ptrPath++] & PATH_ELT_MASK);
		} else
			ptrPath++;
	}

	gluTessEndPolygon(tess);

	gluDeleteTess(tess);
}

void VkgContext::path_extents (bool transformed, float *x1, float *y1, float *x2, float *y2) {
	uint32_t ptrPath    = 0;
	uint32_t firstPtIdx = 0;
	float    x_min = FLT_MAX, y_min = FLT_MAX;
	float    x_max = FLT_MIN, y_max = FLT_MIN;
	///
	while (ptrPath < data->pathPtr) {
		uint32_t pathPointCount = data->pathes[ptrPath] & PATH_ELT_MASK;

		for (uint32_t i = firstPtIdx; i < firstPtIdx + pathPointCount; i++) {
			vec2f p = data->points[i];
			if (transformed) vkg_matrix_transform_point (&data->pushConsts.mat, &p.x, &p.y);
			if (p.x < x_min) x_min = p.x;
			if (p.x > x_max) x_max = p.x;
			if (p.y < y_min) y_min = p.y;
			if (p.y > y_max) y_max = p.y;
		}

		firstPtIdx += pathPointCount;
		if (path_has_curves (ptrPath)) {
			//skip segments lengths used in stroke
			ptrPath++;
			uint32_t totPts = 0;
			while (totPts < pathPointCount)
				totPts += (data->pathes[ptrPath++] & PATH_ELT_MASK);
		} else
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
	vke_cmd_label_start(data->cmd, "draw_full_screen_quad", DBG_LAB_COLOR_FSQ);
#endif
	if (scissor) {
		VkRect2D r = {
			{(int32_t)MAX(scissor->x_min, 0), (int32_t)MAX(scissor->y_min, 0)},
			{(int32_t)MAX(scissor->x_max - (int32_t)scissor->x_min + 1, 1), (int32_t)MAX(scissor->y_max - (int32_t)scissor->y_min + 1, 1)}
		};
		CmdSetScissor(data->cmd, 0, 1, &r);
	}

	uint32_t firstVertIdx = data->vertCount;
	ensure_vertex_cache_size (3);

	_add_vertexf_unchecked (-1, -1);
	_add_vertexf_unchecked ( 3, -1);
	_add_vertexf_unchecked (-1,  3);

	data->curVertOffset = data->vertCount;

	data->pushConsts.fsq_patternType |= FULLSCREEN_BIT;
	CmdPushConstants(data->cmd, data->dev->pipelineLayout,
					   VK_SHADER_STAGE_VERTEX_BIT, 24, 4,&data->pushConsts.fsq_patternType);
	CmdDraw (data->cmd,3,1,firstVertIdx,0);
	data->pushConsts.fsq_patternType &= ~FULLSCREEN_BIT;
	CmdPushConstants(data->cmd, data->dev->pipelineLayout,
					   VK_SHADER_STAGE_VERTEX_BIT, 24, 4,&data->pushConsts.fsq_patternType);
	if (scissor)
		CmdSetScissor(data->cmd, 0, 1, &data->bounds);

#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	vke_cmd_label_end (data->cmd);
#endif
}

void VkgContext::select_font_face (const char* name) {
	if (strcmp(data->selectedFontName, name) == 0)
		return;
	strcpy (data->selectedFontName, name);
	data->currentFont = NULL;
	data->currentFontSize = NULL;
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
	data->lineWidth		= 1;
	data->miterLimit		= 10;
	data->curOperator	= VKVG_OPERATOR_OVER;
	data->curFillRule	= VKVG_FILL_RULE_NON_ZERO;
	data->bounds = (VkRect2D) {{0,0},{data->pSurf->width,data->pSurf->height}};
	data->pushConsts = (push_constants) {
			{.a = 1},
			{(float)data->pSurf->width,(float)data->pSurf->height},
			VKVG_PATTERN_TYPE_SOLID,
			1.0f,
			VKVG_IDENTITY_MATRIX,
			VKVG_IDENTITY_MATRIX
	};
	data->clearRect = (VkClearRect) {{{0},{data->pSurf->width, data->pSurf->height}},0,1};
	data->renderPassBeginInfo.framebuffer = data->pSurf->fb;
	data->renderPassBeginInfo.renderArea.extent.width = data->pSurf->width;
	data->renderPassBeginInfo.renderArea.extent.height = data->pSurf->height;
	data->renderPassBeginInfo.pClearValues = clearValues;

	io_sync(data->pSurf);
	if (data->pSurf->newSurf)
		data->renderPassBeginInfo.renderPass = data->dev->renderPass_ClearAll;
	else
		data->renderPassBeginInfo.renderPass = data->dev->renderPass_ClearStencil;
	data->pSurf->newSurf = false;

	io_unsync(data->pSurf);
	io_grab(vkg_surface, data->pSurf);

	if (data->dev->samples == VK_SAMPLE_COUNT_1_BIT)
		data->renderPassBeginInfo.clearValueCount = 2;
	else
		data->renderPassBeginInfo.clearValueCount = 3;

	data->selectedCharSize	= 10 << 6;
	data->currentFont		= NULL;
	data->selectedFontName[0]= 0;
	data->pattern			= NULL;
	data->curColor			= 0xff000000;//opaque black
	data->cmdStarted			= false;
	data->curClipState		= vkg_clip_state_none;
	data->vertCount			= data->indCount = 0;
#ifdef VKVG_ENABLE_VK_TIMELINE_SEMAPHORE
	data->timelineStep		= 0;
#endif
}


void VkgContext::clear_context () {
	//free saved context stack elmt
	vkg_context_save* next = data->pSavedCtxs;
	data->pSavedCtxs = NULL;
	while (next != NULL) {
		vkg_context_save* cur = next;
		next = cur->pNext;
		free_ctx_save (cur);
	}
	//free additional stencil use in save/restore process
	if (data->savedStencils) {
		uint8_t curSaveStencil = data->curSavBit / 6;
		for (int i=curSaveStencil;i>0;i--)
			vke_image_drop(data->savedStencils[i-1]);
		free(data->savedStencils);
		data->savedStencils = NULL;
		data->curSavBit = 0;
	}

	//remove context from double linked list of context in device
	/*if (data->dev->lastCtx == data) {
		data->dev->lastCtx = data->pPrev;
		if (data->pPrev != NULL)
			data->pPrev->pNext = NULL;
	} else if (data->pPrev == NULL) {
		//first elmt, and it's not last one so pnext is not null
		data->pNext->pPrev = NULL;
	} else {
		data->pPrev->pNext = data->pNext;
		data->pNext->pPrev = data->pPrev;
	}*/
	if (data->dashCount > 0) {
		free(data->dashes);
		data->dashCount = 0;
	}
}

VkgContext::~VkgContext() {
	vke_log(VKE_LOG_INFO, "DESTROY Context: data = %p (status:%d); surf = %p\n", data, data->status, data->pSurf);
	vkg_flush ();
	vke_log(VKE_LOG_DBG_ARRAYS, "END\tctx = %p; pathes:%d pts:%d vch:%d vbo:%d ich:%d ibo:%d\n", data, data->sizePathes, data->sizePoints, data->sizeVertices, data->sizeVBO, data->sizeIndices, data->sizeIBO);

	if (data->pattern)
		VkgPattern::drop(data->pattern);

	_clear_context ();

#if VKVG_DBG_STATS
	if (data->dev->threadAware)
		mtx_lock (&data->dev->mutex);
	
	vkg_debug_stats* dbgstats = &data->dev->debug_stats;
	if (dbgstats->sizePoints < data->sizePoints)
		dbgstats->sizePoints = data->sizePoints;
	if (dbgstats->sizePathes < data->sizePathes)
		dbgstats->sizePathes = data->sizePathes;
	if (dbgstats->sizeVertices < data->sizeVertices)
		dbgstats->sizeVertices = data->sizeVertices;
	if (dbgstats->sizeIndices < data->sizeIndices)
		dbgstats->sizeIndices = data->sizeIndices;
	if (dbgstats->sizeVBO < data->sizeVBO)
		dbgstats->sizeVBO = data->sizeVBO;
	if (dbgstats->sizeIBO < data->sizeIBO)
		dbgstats->sizeIBO = data->sizeIBO;

	if (data->dev->threadAware)
		mtx_unlock (&data->dev->mutex);
#endif

	if (!data->status && data->dev->cachedContextCount < VKVG_MAX_CACHED_CONTEXT_COUNT) {
		_device_store_context ();
		return;
	}

	release_context_resources ();
}

VkgContext::VkgContext(VkgSurface surf)
{
	VkgDevice dev = surf->dev;
	VkgContext data = NULL;

	if (_device_try_get_cached_context (dev, &data) ) {
		data->pSurf = surf;

		if (!surf || surf->status) {
			data->status = VKE_STATUS_INVALID_SURFACE;
			return data;
		}

		_init_ctx ();
		update_descriptor_set (surf->dev->emptyImg, data->dsSrc);
		clear_path	();
		data->cmd = data->cmdBuffers[0];//current recording buffer
		data->status = VKE_STATUS_SUCCESS;
		return data;
	}

	data = io_new(vkg_context);
	
	vke_log(VKE_LOG_INFO, "CREATE Context: data = %p; surf = %p\n", data, surf);

	if (!data)
		return (VkgContext)&_no_mem_status;

	data->pSurf = surf;

	if (!surf || surf->status) {
		data->status = VKE_STATUS_INVALID_SURFACE;
		return data;
	}

	data->sizePoints		= VKVG_PTS_SIZE;
	data->sizeVertices	= data->sizeVBO = VKVG_VBO_SIZE;
	data->sizeIndices	= data->sizeIBO = VKVG_IBO_SIZE;
	data->sizePathes		= VKVG_PATHES_SIZE;
	data->renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;

	data->dev = surf->dev;

	_init_ctx ();

	data->points			= (vec2f*)malloc (VKVG_VBO_SIZE * sizeof(vec2f));
	data->pathes			= (uint32_t*)malloc (VKVG_PATHES_SIZE * sizeof(uint32_t));
	data->vertexCache	= (VkgVertex*)malloc (data->sizeVertices * sizeof(VkgVertex));
	data->indexCache		= (VKVG_IBO_INDEX_TYPE*)malloc (data->sizeIndices * sizeof(VKVG_IBO_INDEX_TYPE));

	if (!data->points || !data->pathes || !data->vertexCache || !data->indexCache) {
		dev->status = VKE_STATUS_NO_MEMORY;
		if (data->points)
			free(data->points);
		if (data->pathes)
			free(data->pathes);
		if (data->vertexCache)
			free(data->vertexCache);
		if (data->indexCache)
			free(data->indexCache);
		return NULL;
	}

	//for context to be thread safe, command pool and descriptor pool have to be created in the thread of the context.
	data->cmdPool	= vke_cmd_pool_create (dev->vke, dev->gQueue->familyIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

#ifndef VKVG_ENABLE_VK_TIMELINE_SEMAPHORE
	data->flushFence	= vke_fence_create_signaled (data->dev->vke);
#endif

	create_vertices_buff	();
	create_gradient_buff	();
	create_cmd_buff		();
	createDescriptorPool	();
	init_descriptor_sets	();
	_font_cache_update_context_descset ();
	update_descriptor_set	(surf->dev->emptyImg, data->dsSrc);
	update_gradient_desc_set();

	clear_path				();

	data->cmd = data->cmdBuffers[0];//current recording buffer

	data->references = 1;
	data->status = VKE_STATUS_SUCCESS;

	vke_log(VKE_LOG_DBG_ARRAYS, "INIT\tctx = %p; pathes:%llu pts:%llu vch:%d vbo:%d ich:%d ibo:%d\n", data, (uint64_t)data->sizePathes, (uint64_t)data->sizePoints, data->sizeVertices, data->sizeVBO, data->sizeIndices, data->sizeIBO);

#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	vke_device_set_object_name(dev->vke, VK_OBJECT_TYPE_COMMAND_POOL, (uint64_t)data->cmdPool, "CTX Cmd Pool");
	vke_device_set_object_name(dev->vke, VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)data->cmdBuffers[0], "CTX Cmd Buff A");
	vke_device_set_object_name(dev->vke, VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)data->cmdBuffers[1], "CTX Cmd Buff B");
#ifndef VKVG_ENABLE_VK_TIMELINE_SEMAPHORE
	vke_device_set_object_name(dev->vke, VK_OBJECT_TYPE_FENCE, (uint64_t)data->flushFence, "CTX Flush Fence");
#endif
	vke_device_set_object_name(dev->vke, VK_OBJECT_TYPE_DESCRIPTOR_POOL, (uint64_t)data->descriptorPool, "CTX Descriptor Pool");
	vke_device_set_object_name(dev->vke, VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)data->dsSrc, "CTX DescSet SOURCE");
	vke_device_set_object_name(dev->vke, VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)data->dsFont, "CTX DescSet FONT");
	vke_device_set_object_name(dev->vke, VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)data->dsGrad, "CTX DescSet GRADIENT");

	vke_device_set_object_name(dev->vke, VK_OBJECT_TYPE_BUFFER, (uint64_t)data->indices.buffer, "CTX Index Buff");
	vke_device_set_object_name(dev->vke, VK_OBJECT_TYPE_BUFFER, (uint64_t)data->vertices.buffer, "CTX Vertex Buff");
#endif

	return data;
}
void VkgContext::flush () {
	flush_cmd_buff		();
	wait_ctx_flush_end	();
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
	CmdBindPipeline(data->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, data->pSurf->dev->pipelineLineList);
	CmdDrawIndexed(data->cmd, data->indCount-data->curIndStart, 1, data->curIndStart, 0, 1);
	flush_cmd_buff();
#endif
*/
}

void VkgContext::set_opacity (float opacity) {
	if (EQUF(data->pushConsts.opacity, opacity))
		return;

	emit_draw_cmd_undrawn_vertices ();
	data->pushConsts.opacity = opacity;
	data->pushCstDirty = true;
}
float VkgContext::get_opacity () {
	return data->pushConsts.opacity;
}
VkeStatus VkgContext::status () {
	return data->status;
}

void VkgContext::new_sub_path () {
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: new_sub_path:\n");
	finish_path();
}

void VkgContext::new_path () {
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: new_path:\n");

	clear_path();
}
void VkgContext::close_path () {
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: close_path:\n");

	if (data->pathes[data->pathPtr] & PATH_CLOSED_BIT) //already closed
		return;
	//check if at least 3 points are present
	if (data->pathes[data->pathPtr] < 3)
		return;

	//prevent closing on the same point
	if (vec2f_equ(data->points[data->pointCount-1],
				 data->points[data->pointCount - data->pathes[data->pathPtr]])) {
		if (data->pathes[data->pathPtr] < 4)//ensure enough points left for closing
			return;
		remove_last_point();
	}

	data->pathes[data->pathPtr] |= PATH_CLOSED_BIT;

	finish_path();
}

void VkgContext::rel_line_to (float dx, float dy) {
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: rel_line_to: %f, %f\n", dx, dy);

	if (current_path_is_empty())
		add_point(0, 0);
	vec2f cp = get_current_position();
	line_to(cp.x + dx, cp.y + dy);
}

void VkgContext::line_to (float x, float y) {
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: line_to: %f, %f\n", x, y);
	line_to(x, y);
}

void VkgContext::arc (float xc, float yc, float radius, float a1, float a2) {
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: arc: %f,%f %f %f %f\n", xc, yc, radius, a1, a2);

	while (a2 < a1)//positive arc must have a1<a2
		a2 += 2.f*M_PIF;

	if (a2 - a1 > 2.f * M_PIF) //limit arc to 2PI
		a2 = a1 + 2.f * M_PIF;

	vec2f v = {cosf(a1)*radius + xc, sinf(a1)*radius + yc};

	float step = get_arc_step(radius);
	float a = a1;

	if (current_path_is_empty()) {
		set_curve_start ();
		add_point (v.x, v.y);
		if (!data->pathPtr)
			data->simpleConvex = true;
		else
			data->simpleConvex = false;
	} else {
		line_to(v.x, v.y);
		set_curve_start ();
		data->simpleConvex = false;
	}

	a+=step;

	if (EQUF(a2, a1))
		return;

	while(a < a2) {
		v.x = cosf(a)*radius + xc;
		v.y = sinf(a)*radius + yc;
		add_point (v.x, v.y);
		a+=step;
	}

	if (EQUF(a2-a1,M_PIF*2.f)) {//if arc is complete circle, last point is the same as the first one
		set_curve_end();
		close_path();
		return;
	}
	a = a2;
	v.x = cosf(a)*radius + xc;
	v.y = sinf(a)*radius + yc;
	add_point (v.x, v.y);
	set_curve_end();
}
void VkgContext::arc_negative (float xc, float yc, float radius, float a1, float a2) {
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: %f,%f %f %f %f\n", xc, yc, radius, a1, a2);
	while (a2 > a1)
		a2 -= 2.f*M_PIF;
	if (a1 - a2 > a1 + 2.f * M_PIF) //limit arc to 2PI
		a2 = a1 - 2.f * M_PIF;

	vec2f v = {cosf(a1)*radius + xc, sinf(a1)*radius + yc};

	float step = get_arc_step(radius);
	float a = a1;

	if (current_path_is_empty()) {
		set_curve_start ();
		add_point (v.x, v.y);
		if (!data->pathPtr)
			data->simpleConvex = true;
		else
			data->simpleConvex = false;
	} else {
		line_to(v.x, v.y);
		set_curve_start ();
		data->simpleConvex = false;
	}

	a-=step;

	if (EQUF(a2, a1))
		return;

	while(a > a2) {
		v.x = cosf(a)*radius + xc;
		v.y = sinf(a)*radius + yc;
		add_point (data,v.x,v.y);
		a-=step;
	}

	if (EQUF(a1-a2,M_PIF*2.f)) {//if arc is complete circle, last point is the same as the first one
		set_curve_end();
		close_path();
		return;
	}

	a = a2;
	//vec2f lastP = v;
	v.x = cosf(a)*radius + xc;
	v.y = sinf(a)*radius + yc;
	//if (!vec2f_equ (v,lastP))
		add_point (v.x, v.y);
	set_curve_end();
}

void VkgContext::rel_move_to (float x, float y) {
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: rel_mote_to: %f, %f\n", x, y);
	if (current_path_is_empty())
		add_point(0, 0);
	vec2f cp = get_current_position();
	finish_path();
	add_point (cp.x + x, cp.y + y);
}

void VkgContext::move_to (float x, float y) {
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: move_to: %f,%f\n", x, y);
	finish_path();
	add_point (x, y);
}

bool VkgContext::has_current_point () {
	return !current_path_is_empty();
}

void VkgContext::get_current_point (float* x, float* y) {
	if (current_path_is_empty()) {
		*x = *y = 0;
		return;
	}
	vec2f cp = get_current_position();
	*x = cp.x;
	*y = cp.y;
}

void VkgContext::curve_to (float x1, float y1, float x2, float y2, float x3, float y3) {
	//prevent running recursive_bezier when all 4 curve points are equal
	if (EQUF(x1,x2) && EQUF(x2,x3) && EQUF(y1,y2) && EQUF(y2,y3)) {
		if (current_path_is_empty() || (EQUF(get_current_position().x,x1) && EQUF(get_current_position().y,y1)))
			return;
	}
	data->simpleConvex = false;
	set_curve_start ();
	if (current_path_is_empty())
		add_point(x1, y1);

	vec2f cp = get_current_position();

	//compute dyn distanceTolerance depending on current scale
	float sx = 1, sy = 1;
	data->pushConsts.mat.matrix_get_scale (&sx, &sy);
	float distanceTolerance = fabs(0.25f / fmaxf(sx,sy));
	recursive_bezier (distanceTolerance, cp.x, cp.y, x1, y1, x2, y2, x3, y3, 0);
	/*cp.x = x3;
	cp.y = y3;
	if (!vec2f_equ(data->points[data->pointCount-1],cp))*/
		add_point(data,x3,y3);
	set_curve_end ();
}

const double quadraticFact = 2.0/3.0;
void VkgContext::quadratic_to (float x1, float y1, float x2, float y2) {
	float x0, y0;
	if (current_path_is_empty()) {
		x0 = x1;
		y0 = y1;
	} else
		vkg_get_current_point (&x0, &y0);
	_curve_to (data,
					x0 + (x1 - x0) * quadraticFact,
					y0 + (y1 - y0) * quadraticFact,
					x2 + (x1 - x2) * quadraticFact,
					y2 + (y1 - y2) * quadraticFact,
					x2, y2);
}

void VkgContext::quadratic_to (float x1, float y1, float x2, float y2) {
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: quadratic_to: %f, %f, %f, %f\n", x1, y1, x2, y2);
	quadratic_to(x1, y1, x2, y2);
}

void VkgContext::rel_quadratic_to (float x1, float y1, float x2, float y2) {
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: rel_quadratic_to: %f, %f, %f, %f\n", x1, y1, x2, y2);
	vec2f cp = get_current_position();
	quadratic_to (cp.x + x1, cp.y + y1, cp.x + x2, cp.y + y2);
}

void VkgContext::curve_to (float x1, float y1, float x2, float y2, float x3, float y3) {
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: curve_to %f,%f %f,%f %f,%f:\n", x1, y1, x2, y2, x3, y3);
	_curve_to (x1, y1, x2, y2, x3, y3);
}

void VkgContext::rel_curve_to (float x1, float y1, float x2, float y2, float x3, float y3) {
	if (current_path_is_empty()) {
		data->status = VKE_STATUS_NO_CURRENT_POINT;
		return;
	}
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: rel curve_to %f,%f %f,%f %f,%f:\n", x1, y1, x2, y2, x3, y3);
	vec2f cp = get_current_position();
	_curve_to (cp.x + x1, cp.y + y1, cp.x + x2, cp.y + y2, cp.x + x3, cp.y + y3);
}

void VkgContext::fill_rectangle (float x, float y, float w, float h) {
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: fill_rectangle:\n");
	vao_add_rectangle (data,x,y,w,h);
	//_record_draw_cmd();
}

VkeStatus VkgContext::rectangle (float x, float y, float w, float h) {
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: rectangle: %f,%f,%f,%f\n", x, y, w, h);
	finish_path ();
	if (w <= 0 || h <= 0)
		return VKE_STATUS_INVALID_RECT;
	add_point (x, y);
	add_point (x + w, y);
	add_point (x + w, y + h);
	add_point (x, y + h);	
	data->pathes[data->pathPtr] |= (PATH_CLOSED_BIT|PATH_IS_CONVEX_BIT);
	finish_path();
	return VKE_STATUS_SUCCESS;
}

VkeStatus VkgContext::rounded_rectangle (float x, float y, float w, float h, float radius) {
	vke_log(VKE_LOG_INFO_CMD, "CMD: rounded_rectangle:\n");
	finish_path ();
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
	close_path();

	return VKE_STATUS_SUCCESS;
}

void VkgContext::rounded_rectangle2 (float x, float y, float w, float h, float rx, float ry) {
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

	close_path();
}

void VkgContext::path_extents (float *x1, float *y1, float *x2, float *y2) {
	finish_path();
	if (!data->pathPtr) {//no path
		*x1 = *x2 = *y1 = *y2 = 0;
		return;
	}
	path_extents(false, x1, y1, x2, y2);
}

vkg_clip_state VkgContext::get_previous_clip_state () {
	if (!data->pSavedCtxs)//no clip saved => clear
		return vkg_clip_state_clear;
	return data->pSavedCtxs->clippingState;
}

static const VkClearAttachment clearStencil		   = {VK_IMAGE_ASPECT_STENCIL_BIT, 1, {{{0}}}};
static const VkClearAttachment clearColorAttach	   = {VK_IMAGE_ASPECT_COLOR_BIT,   0, {{{0}}}};

void VkgContext::reset_clip () {
	emit_draw_cmd_undrawn_vertices();
	if (!data->cmdStarted) {
		//if command buffer is not already started and in a renderpass, we use the renderpass
		//with the loadop clear for stencil
		data->renderPassBeginInfo.renderPass = data->dev->renderPass_ClearStencil;
		//force run of one renderpass (even empty) to perform clear load op
		start_cmd_for_render_pass();
		return;
	}
	vkCmdClearAttachments(data->cmd, 1, &clearStencil, 1, &data->clearRect);
}

void VkgContext::reset_clip() {
	if (data->curClipState == vkg_clip_state_clear)
		return;
	if (_get_previous_clip_state() == vkg_clip_state_clear)
		data->curClipState = vkg_clip_state_none;
	else
		data->curClipState = vkg_clip_state_clear;

	_reset_clip ();
}

void VkgContext::clear () {
	if (_get_previous_clip_state() == vkg_clip_state_clear)
		data->curClipState = vkg_clip_state_none;
	else
		data->curClipState = vkg_clip_state_clear;

	emit_draw_cmd_undrawn_vertices();
	if (!data->cmdStarted) {
		data->renderPassBeginInfo.renderPass = data->dev->renderPass_ClearAll;
		start_cmd_for_render_pass();
		return;
	}
	VkClearAttachment ca[2] = {clearColorAttach, clearStencil};
	vkCmdClearAttachments(data->cmd, 2, ca, 1, &data->clearRect);
}

void VkgContext::clip_preserve () {
	finish_path();

	if (!data->pathPtr)//nothing to clip
		return;

	emit_draw_cmd_undrawn_vertices();

	vke_log(VKE_LOG_INFO, "CLIP: data = %p; path cpt = %d;\n", data, data->pathPtr / 2);

	ensure_renderpass_is_started();

#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	vke_cmd_label_start(data->cmd, "clip", DBG_LAB_COLOR_CLIP);
#endif

	if (data->curFillRule == VKVG_FILL_RULE_EVEN_ODD) {
		poly_fill				(NULL);
		CmdBindPipeline			(data->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, data->dev->pipelineClipping);
	} else {
		CmdBindPipeline			(data->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, data->dev->pipelineClipping);
		CmdSetStencilReference	(data->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_FILL_BIT);
		CmdSetStencilCompareMask(data->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_CLIP_BIT);
		CmdSetStencilWriteMask	(data->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_FILL_BIT);
		fill_non_zero();
		emit_draw_cmd_undrawn_vertices();
	}
	CmdSetStencilReference	(data->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_CLIP_BIT);
	CmdSetStencilCompareMask(data->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_FILL_BIT);
	CmdSetStencilWriteMask	(data->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_ALL_BIT);

	draw_full_screen_quad (NULL);

	bind_draw_pipeline ();
	CmdSetStencilCompareMask (data->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_CLIP_BIT);

#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	vke_cmd_label_end (data->cmd);
#endif

	data->curClipState = vkg_clip_state_clip;
}

void VkgContext::fill_preserve (VkgContext data) {
	finish_path();

	if (!data->pathPtr)//nothing to fill
		return;

	vke_log(VKE_LOG_INFO, "FILL: data = %p; path cpt = %d;\n", data, data->subpathCount);

	 if (data->curFillRule == VKVG_FILL_RULE_EVEN_ODD) {
		 emit_draw_cmd_undrawn_vertices();
		vec4 bounds = {FLT_MAX,FLT_MAX,FLT_MIN,FLT_MIN};
		poly_fill				(&bounds);
		bind_draw_pipeline		();
		CmdSetStencilCompareMask(data->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_FILL_BIT);
		draw_full_screen_quad	(&bounds);
		CmdSetStencilCompareMask(data->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_CLIP_BIT);
		return;
	}

	if (data->vertCount - data->curVertOffset + data->pointCount > VKVG_IBO_MAX)
		emit_draw_cmd_undrawn_vertices();//limit draw call to addressable vx with choosen index type

	if (data->pattern)//if not solid color, source img or gradient has to be bound
		ensure_renderpass_is_started();
	fill_non_zero();
}

void VkgContext::stroke_preserve (VkgContext data)
{
	finish_path();

	if (!data->pathPtr)//nothing to stroke
		return;

	vke_log(VKE_LOG_INFO, "STROKE: data = %p; path ptr = %d;\n", data, data->pathPtr);

	stroke_context str = {0};
	str.hw = data->lineWidth * 0.5f;
	str.lhMax = data->miterLimit * data->lineWidth;
	uint32_t ptrPath = 0;

	while (ptrPath < data->pathPtr) {
		uint32_t ptrSegment = 0, lastSegmentPointIdx = 0;
		uint32_t firstPathPointIdx = str.cp;
		uint32_t pathPointCount = data->pathes[ptrPath]&PATH_ELT_MASK;
		uint32_t lastPathPointIdx = str.cp + pathPointCount - 1;

		dash_context dc = {0};

		if (path_has_curves (data,ptrPath)) {
			ptrSegment = 1;
			lastSegmentPointIdx = str.cp + (data->pathes[ptrPath+ptrSegment]&PATH_ELT_MASK)-1;
		}

		str.firstIdx = (VKVG_IBO_INDEX_TYPE)(data->vertCount - data->curVertOffset);

		//vke_log(VKE_LOG_INFO_PATH, "\tPATH: points count=%10d end point idx=%10d", data->pathes[ptrPath]&PATH_ELT_MASK, lastPathPointIdx);

		if (data->dashCount > 0) {
			//init dash stroke
			dc.dashOn = true;
			dc.curDash = 0;	//current dash index
			dc.totDashLength = 0;//limit offset to total length of dashes
			for (uint32_t i=0;i<data->dashCount;i++)
				dc.totDashLength += data->dashes[i];
			if (dc.totDashLength == 0) {
				data->status = VKE_STATUS_INVALID_DASH;
				return;
			}
			dc.curDashOffset = fmodf(data->dashOffset, dc.totDashLength);	//cur dash offset between defined path point and last dash segment(on/off) start
			str.iL = lastPathPointIdx;
		} else if (path_is_closed(data,ptrPath)) {
			str.iL = lastPathPointIdx;
		} else {
			draw_stoke_cap(&str, data->points[str.cp], vec2f_line_norm(data->points[str.cp], data->points[str.cp+1]), true);
			str.iL = str.cp++;
		}

		if (path_has_curves (data,ptrPath)) {
			while (str.cp < lastPathPointIdx) {

				bool curved = data->pathes [ptrPath + ptrSegment] & PATH_HAS_CURVES_BIT;
				if (lastSegmentPointIdx == lastPathPointIdx)//last segment of path, dont draw end point here
					lastSegmentPointIdx--;
				while (str.cp <= lastSegmentPointIdx)
					draw_segment(&str, &dc, curved);

				ptrSegment ++;
				uint32_t cptSegPts = data->pathes [ptrPath + ptrSegment]&PATH_ELT_MASK;
				lastSegmentPointIdx = str.cp + cptSegPts - 1;
				if (lastSegmentPointIdx == lastPathPointIdx && cptSegPts == 1) {
					//single point last segment
					ptrSegment++;
					break;
				}
			}
		} else while (str.cp < lastPathPointIdx)
			draw_segment(&str, &dc, false);

		if (data->dashCount > 0) {
			if (path_is_closed(data,ptrPath)) {
				str.iR = firstPathPointIdx;

				draw_dashed_segment(&str, &dc, false);

				str.iL++;
				str.cp++;
			}
			if (!dc.dashOn) {
				//finishing last dash that is already started, draw end caps but not too close to start
				//the default gap is the next void
				int32_t prevDash = (int32_t)dc.curDash-1;
				if (prevDash < 0)
					dc.curDash = data->dashCount-1;
				float m = fminf (data->dashes[prevDash] - dc.curDashOffset, data->dashes[dc.curDash]);
				vec2f p = vec2f_sub(data->points[str.iR], vec2f_scale(dc.normal, m));
				draw_stoke_cap (&str, p, dc.normal, false);
			}
		} else if (path_is_closed(data,ptrPath)) {
			str.iR = firstPathPointIdx;
			bool inverse = build_vb_step (&str, false);

			VKVG_IBO_INDEX_TYPE* inds = &data->indexCache [data->indCount-6];
			VKVG_IBO_INDEX_TYPE ii = str.firstIdx;
			if (inverse) {
				inds[1] = ii+1;
				inds[4] = ii+1;
				inds[5] = ii;
			} else {
				inds[1] = ii;
				inds[4] = ii;
				inds[5] = ii+1;
			}
			str.cp++;
		} else
			draw_stoke_cap (&str, data->points[str.cp], vec2f_line_norm(data->points[str.cp-1], data->points[str.cp]), false);

		str.cp = firstPathPointIdx + pathPointCount;

		if (ptrSegment > 0)
			ptrPath += ptrSegment;
		else
			ptrPath++;

		//limit batch size here to 1/3 of the ibo index type ability
		if (data->vertCount - data->curVertOffset > VKVG_IBO_MAX / 3)
			emit_draw_cmd_undrawn_vertices ();
	}
}

void VkgContext::clip() {
	_clip_preserve();
	clear_path();
}

void VkgContext::stroke() {
	stroke_preserve();
	clear_path();
}

void VkgContext::fill() {
	fill_preserve();
	clear_path();
}

void VkgContext::paint() {
	finish_path ();
	if (data->pathPtr) {
		fill();
		return;
	}
	ensure_renderpass_is_started();
	draw_full_screen_quad (NULL);
}

void VkgContext::set_source_color(uint32_t c) {
	data->curColor = c;
	update_cur_pattern (NULL);
}

void VkgContext::set_source_rgb(float r, float g, float b) {
	data->curColor = CreateRgbaf(r,g,b,1);
	update_cur_pattern (NULL);
}

void VkgContext::set_source_rgba(float r, float g, float b, float a) {
	data->curColor = CreateRgbaf(r,g,b,a);
	update_cur_pattern (NULL);
}

void VkgContext::set_source_surface(VkgSurface surf, float x, float y) {
	data->pushConsts.source.x = x;
	data->pushConsts.source.y = y;
	update_cur_pattern (VkgPattern(surf));
	data->pushCstDirty = true;
}

void VkgContext::set_source(VkgPattern pat) {
	update_cur_pattern (pat);
	VkgPattern::grab	(pat);
}

void VkgContext::set_line_width (float width) {
	data->lineWidth = width;
}

void VkgContext::set_miter_limit(float limit) {
	data->miterLimit = limit;
}

void VkgContext::set_line_cap(vkg_line_cap cap) {
	data->lineCap = cap;
}

void VkgContext::set_line_join(vkg_line_join join) {
	data->lineJoin = join;
}

void VkgContext::set_operator(vkg_operator op) {
	if (op == data->curOperator)
		return;
	emit_draw_cmd_undrawn_vertices();//draw call with different ops cant be combined, so emit draw cmd for previous vertices.
	data->curOperator = op;
	if (data->cmdStarted)
		bind_draw_pipeline ();
}

void VkgContext::set_fill_rule(vkg_fill_rule fr) {
#ifndef __APPLE__
	data->curFillRule = fr;
#endif
}

vkg_fill_rule VkgContext::get_fill_rule() {
	return data->curFillRule;
}

float VkgContext::get_line_width() {
	return data->lineWidth;
}

void VkgContext::set_dash(const float* dashes, uint32_t num_dashes, float offset) {
	if (data->dashCount > 0)
		free (data->dashes);
	data->dashCount = num_dashes;
	data->dashOffset = offset;
	if (data->dashCount == 0)
		return;
	data->dashes = (float*)malloc (sizeof(float) * data->dashCount);
	memcpy (data->dashes, dashes, sizeof(float) * data->dashCount);
}

void VkgContext::get_dash(const float* dashes, uint32_t* num_dashes, float* offset) {
	*num_dashes = data->dashCount;
	*offset = data->dashOffset;
	if (data->dashCount == 0 || dashes == NULL)
		return;
	memcpy ((float*)dashes, data->dashes, sizeof(float) * data->dashCount);
}

vkg_line_cap VkgContext::get_line_cap() {
	return data->lineCap;
}

vkg_line_join VkgContext::get_line_join() {
	return data->lineJoin;
}

vkg_operator VkgContext::get_operator() {
	return data->curOperator;
}

VkgPattern VkgContext::get_source() {
	return data->pattern;
}

void VkgContext::select_font_face(const char* name) {
	select_font_face (name);
}

void VkgContext::load_font_from_path(const char* path, const char* name) {
	vkg_font_identity* fid = _font_cache_add_font_identity(path, name);
	if (!_font_cache_load_font_file_in_memory (fid)) {
		data->status = VKE_STATUS_FILE_NOT_FOUND;
		return;
	}
	select_font_face (name);
}

void VkgContext::load_font_from_memory(unsigned char* fontBuffer, long fontBufferByteSize, const char* name) {
	vkg_font_identity* fid = _font_cache_add_font_identity (NULL, name);
	fid->fontBuffer = fontBuffer;
	fid->fontBufSize = fontBufferByteSize;
	select_font_face (name);
}

void VkgContext::set_font_size(uint32_t size) {
#ifdef VKVG_USE_FREETYPE
	long newSize = size << 6;
#else
	long newSize = size;
#endif
	if (data->selectedCharSize == newSize)
		return;
	data->selectedCharSize = newSize;
	data->currentFont = NULL;
	data->currentFontSize = NULL;
}

void VkgContext::set_text_direction(vkg_context* data, vkg_direction direction) {

}

void VkgContext::show_text(const char* text) {
	vke_log(VKE_LOG_INFO_CMD, "CMD: show_text:\n");
	//ensure_renderpass_is_started();
	_font_cache_show_text (text);
	//_flush_undrawn_vertices ();
}

VkgText VkgContext::text_run_create(const char* text) {
	return font_cache_create_text_run(text, -1);
}

VkgText VkgContext::text_run_create_with_length (const char* text, uint32_t length) {
	vkg_text_run *tr = io_new(vkg_text_run);
	font_cache_create_text_run(text, length, tr);
	return tr;
}

uint32_t VkgContext::text_run_get_glyph_count(VkgText textRun) {
	return textRun->glyph_count;
}

void VkgContext::text_run_get_glyph_position(VkgText textRun,
									   uint32_t index,
									   vkg_glyph_info_t* pGlyphInfo) {
	if (index >= textRun->glyph_count) {
		*pGlyphInfo = (vkg_glyph_info_t) {0};
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
	_font_cache_show_text_run(textRun);
}

void VkgContext::text_run_get_extents (VkgText textRun, vkg_text_extents* extents) {
	*extents = textRun->extents;
}

void VkgContext::text_extents (const char* text, vkg_text_extents* extents) {
	_font_cache_text_extents(text, -1, extents);
}

void VkgContext::font_extents (vkg_font_extents* extents) {
	_font_cache_font_extents(extents);
}

void VkgContext::save () {
	vke_log(VKE_LOG_INFO, "SAVE CONTEXT: data = %p\n", data);

	VkgDevice dev = data->dev;
	vkg_context_save* sav = (vkg_context_save*)calloc(1,sizeof(vkg_context_save));

	flush_cmd_buff ();
	if (!wait_ctx_flush_end ()) {
		free (sav);
		return;
	}

	if (data->curClipState == vkg_clip_state_clip) {
		sav->clippingState = vkg_clip_state_clip_saved;

		uint8_t curSaveStencil = data->curSavBit / 6;

		if (data->curSavBit > 0 && data->curSavBit % 6 == 0) {//new save/restore stencil image have to be created
			VkeImage* savedStencilsPtr = NULL;
			if (savedStencilsPtr)
				savedStencilsPtr = (VkeImage*)::realloc(data->savedStencils, curSaveStencil * sizeof(VkeImage));
			else
				savedStencilsPtr = (VkeImage*)malloc(curSaveStencil * sizeof(VkeImage));
			if (savedStencilsPtr == NULL) {
				free(sav);
				data->status = VKE_STATUS_NO_MEMORY;
				return;
			}
			data->savedStencils = savedStencilsPtr;
			VkeImage savStencil = vke_image_ms_create ((VkeDevice)dev, dev->stencilFormat, dev->samples, data->pSurf->width, data->pSurf->height,
									VKE_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
			data->savedStencils[curSaveStencil-1] = savStencil;

			data->cmd.begin(VkeCommandBufferUsage::one_time_submit);
			data->cmdStarted = true;

	#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
			vke_cmd_label_start(data->cmd, "new save/restore stencil", DBG_LAB_COLOR_SAV);
	#endif

			vke_image_set_layout (data->cmd, data->pSurf->stencil, dev->stencilAspectFlag,
								  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
								  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
			vke_image_set_layout (data->cmd, savStencil, dev->stencilAspectFlag,
								  VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								  VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

			VkImageCopy cregion = { .srcSubresource = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 0, 1},
									.dstSubresource = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 0, 1},
									.extent = {data->pSurf->width,data->pSurf->height,1}};
			vkCmdCopyImage(data->cmd,
						   vke_image_get_vkimage(data->pSurf->stencil),VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						   vke_image_get_vkimage(savStencil),		 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						   1, &cregion);

			vke_image_set_layout (data->cmd, data->pSurf->stencil, dev->stencilAspectFlag,
								  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
								  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);

	#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
			vke_cmd_label_end (data->cmd);
	#endif

			VK_CHECK_RESULT(vkEndCommandBuffer(data->cmd));
			wait_and_submit_cmd();
		}

		uint8_t curSaveBit = 1 << (data->curSavBit % 6 + 2);

		start_cmd_for_render_pass ();

	#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
		vke_cmd_label_start(data->cmd, "save rp", DBG_LAB_COLOR_SAV);
	#endif

		CmdBindPipeline			(data->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, data->dev->pipelineClipping);

		CmdSetStencilReference	(data->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_CLIP_BIT|curSaveBit);
		CmdSetStencilCompareMask(data->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_CLIP_BIT);
		CmdSetStencilWriteMask	(data->cmd, VK_STENCIL_FRONT_AND_BACK, curSaveBit);

		draw_full_screen_quad (NULL);

		bind_draw_pipeline ();
		CmdSetStencilCompareMask(data->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_CLIP_BIT);

	#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
		vke_cmd_label_end (data->cmd);
	#endif
		data->curSavBit++;
	} else if (data->curClipState == vkg_clip_state_none)
		sav->clippingState = (_get_previous_clip_state() & 0x03);
	else
		sav->clippingState = vkg_clip_state_clear;

	sav->dashOffset = data->dashOffset;
	sav->dashCount	= data->dashCount;
	if (data->dashCount > 0) {
		sav->dashes = (float*)malloc (sizeof(float) * data->dashCount);
		memcpy (sav->dashes, data->dashes, sizeof(float) * data->dashCount);
	}
	sav->lineWidth	= data->lineWidth;
	sav->miterLimit	= data->miterLimit;
	sav->curOperator= data->curOperator;
	sav->lineCap	= data->lineCap;
	sav->lineWidth	= data->lineWidth;
	sav->curFillRule= data->curFillRule;

	sav->selectedCharSize = data->selectedCharSize;
	strcpy (sav->selectedFontName, data->selectedFontName);

	sav->currentFont  = data->currentFont;
	sav->textDirection= data->textDirection;
	sav->pushConsts	  = data->pushConsts;
	if (data->pattern) {
		sav->pattern = data->pattern;//TODO:pattern sav must be imutable (copy?)
		io_grab(vkg_pattern, data->pattern);
	} else
		sav->curColor = data->curColor;

	sav->pNext		= data->pSavedCtxs;
	data->pSavedCtxs = sav;

}
void VkgContext::restore () {
	if (data->pSavedCtxs == NULL) {
		data->status = VKE_STATUS_INVALID_RESTORE;
		return;
	}

	vke_log(VKE_LOG_INFO, "RESTORE CONTEXT: data = %p\n", data);

	vkg_context_save* sav = data->pSavedCtxs;
	data->pSavedCtxs = sav->pNext;

	flush_cmd_buff ();
	if (!wait_ctx_flush_end ())
		return;

	data->pushConsts	  = sav->pushConsts;
	data->pushCstDirty = true;

	if (data->curClipState) {//!=none
		if (data->curClipState == vkg_clip_state_clip && sav->clippingState == vkg_clip_state_clear) {
			_reset_clip ();
		} else {

			uint8_t curSaveBit = 1 << ((data->curSavBit-1) % 6 + 2);

			start_cmd_for_render_pass ();

#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
			vke_cmd_label_start(data->cmd, "restore rp", DBG_LAB_COLOR_SAV);
#endif

			CmdBindPipeline			(data->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, data->dev->pipelineClipping);

			CmdSetStencilReference	(data->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_CLIP_BIT|curSaveBit);
			CmdSetStencilCompareMask(data->cmd, VK_STENCIL_FRONT_AND_BACK, curSaveBit);
			CmdSetStencilWriteMask	(data->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_CLIP_BIT);

			draw_full_screen_quad (NULL);

			bind_draw_pipeline ();
			CmdSetStencilCompareMask (data->cmd, VK_STENCIL_FRONT_AND_BACK, STENCIL_CLIP_BIT);

#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
			vke_cmd_label_end (data->cmd);
#endif

			flush_cmd_buff ();
			if (!wait_ctx_flush_end ())
				return;
		}
	}
	if (sav->clippingState == vkg_clip_state_clip_saved) {
		data->curSavBit--;

		uint8_t curSaveStencil = data->curSavBit / 6;
		if (data->curSavBit > 0 && data->curSavBit % 6 == 0) {//addtional save/restore stencil image have to be copied back to surf stencil first
			VkeImage savStencil = data->savedStencils[curSaveStencil-1];

			data->cmd.begin(VkeCommandBufferUsage::one_time_submit);
			data->cmdStarted = true;

#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
			vke_cmd_label_start(data->cmd, "additional stencil copy while restoring", DBG_LAB_COLOR_SAV);
#endif

			vke_image_set_layout (data->cmd, data->pSurf->stencil, data->dev->stencilAspectFlag,
								  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
			vke_image_set_layout (data->cmd, savStencil, data->dev->stencilAspectFlag,
								  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
								  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

			VkImageCopy cregion = { .srcSubresource = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 0, 1},
									.dstSubresource = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 0, 1},
									.extent = {data->pSurf->width,data->pSurf->height,1}};
			vkCmdCopyImage(data->cmd,
						   vke_image_get_vkimage (savStencil),		 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						   vke_image_get_vkimage (data->pSurf->stencil),VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						   1, &cregion);
			vke_image_set_layout (data->cmd, data->pSurf->stencil, data->dev->stencilAspectFlag,
								  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
								  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);

#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
			vke_cmd_label_end (data->cmd);
#endif

			VK_CHECK_RESULT(vkEndCommandBuffer(data->cmd));
			wait_and_submit_cmd ();
			if (!wait_ctx_flush_end ())
				return;
			vke_image_drop(savStencil);
		}
	}

	data->curClipState = vkg_clip_state_none;

	data->dashOffset = sav->dashOffset;
	if (data->dashCount > 0)
		free (data->dashes);
	data->dashCount	= sav->dashCount;
	if (data->dashCount > 0) {
		data->dashes = (float*)malloc (sizeof(float) * data->dashCount);
		memcpy (data->dashes, sav->dashes, sizeof(float) * data->dashCount);
	}

	data->lineWidth	= sav->lineWidth;
	data->miterLimit	= sav->miterLimit;
	data->curOperator= sav->curOperator;
	data->lineCap	= sav->lineCap;
	data->lineJoin	= sav->lineJoint;
	data->curFillRule= sav->curFillRule;

	data->selectedCharSize = sav->selectedCharSize;
	strcpy (data->selectedFontName, sav->selectedFontName);

	data->currentFont  = sav->currentFont;
	data->textDirection= sav->textDirection;

	if (sav->pattern) {
		if (sav->pattern != data->pattern)
			update_cur_pattern (data, sav->pattern);
		else
			VkgPattern::drop(sav->pattern);
	} else {
		data->curColor = sav->curColor;
		update_cur_pattern (data, NULL);
	}

	free_ctx_save(sav);
}

void VkgContext::translate (float dx, float dy) {
	vke_log(VKE_LOG_INFO_CMD, "CMD: translate: %f, %f\n", dx, dy);
	emit_draw_cmd_undrawn_vertices();
	vkg_matrix_translate (&data->pushConsts.mat, dx, dy);
	set_mat_inv_and_vkCmdPush ();
}

void VkgContext::scale (float sx, float sy) {
	vke_log(VKE_LOG_INFO_CMD, "CMD: scale: %f, %f\n", sx, sy);
	emit_draw_cmd_undrawn_vertices();
	vkg_matrix_scale (&data->pushConsts.mat, sx, sy);
	set_mat_inv_and_vkCmdPush ();
}

void VkgContext::rotate (float radians) {
	vke_log(VKE_LOG_INFO_CMD, "CMD: rotate: %f\n", radians);
	emit_draw_cmd_undrawn_vertices();
	vkg_matrix_rotate (&data->pushConsts.mat, radians);
	set_mat_inv_and_vkCmdPush ();
}

void VkgContext::transform (const vkg_matrix* matrix) {
	vke_log(VKE_LOG_INFO_CMD, "CMD: transform: %f, %f, %f, %f, %f, %f\n", matrix->xx, matrix->yx, matrix->xy, matrix->yy, matrix->x0, matrix->y0);
	emit_draw_cmd_undrawn_vertices();
	vkg_matrix res;
	vkg_matrix_multiply (&res, &data->pushConsts.mat, matrix);
	data->pushConsts.mat = res;
	set_mat_inv_and_vkCmdPush ();
}

void VkgContext::identity_matrix () {
	vke_log(VKE_LOG_INFO_CMD, "CMD: identity_matrix:\n");
	emit_draw_cmd_undrawn_vertices();
	vkg_matrix im = VKVG_IDENTITY_MATRIX;
	data->pushConsts.mat = im;
	set_mat_inv_and_vkCmdPush ();
}

void VkgContext::set_matrix (const vkg_matrix* matrix) {
	vke_log(VKE_LOG_INFO_CMD, "CMD: set_matrix: %f, %f, %f, %f, %f, %f\n", matrix->xx, matrix->yx, matrix->xy, matrix->yy, matrix->x0, matrix->y0);
	emit_draw_cmd_undrawn_vertices();
	data->pushConsts.mat = (*matrix);
	set_mat_inv_and_vkCmdPush ();
}

void VkgContext::get_matrix (vkg_matrix* const matrix) {
	*matrix = data->pushConsts.mat;
}

void VkgContext::elliptic_arc_to (float x2, float y2, bool largeArc, bool sweepFlag, float rx, float ry, float phi) {
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: elliptic_arc_to: x2:%10.5f y2:%10.5f large:%d sweep:%d rx:%10.5f ry:%10.5f phi:%10.5f \n", x2,y2,largeArc,sweepFlag,rx,ry,phi);
	float x1, y1;
	vkg_get_current_point(data, &x1, &y1);
	_elliptic_arc(data, x1, y1, x2, y2, largeArc, sweepFlag, rx, ry, phi);
}

void VkgContext::rel_elliptic_arc_to (float x2, float y2, bool largeArc, bool sweepFlag, float rx, float ry, float phi) {
	vke_log(VKE_LOG_INFO_CMD, "\tCMD: rel_elliptic_arc_to: x2:%10.5f y2:%10.5f large:%d sweep:%d rx:%10.5f ry:%10.5f phi:%10.5f \n", x2,y2,largeArc,sweepFlag,rx,ry,phi);

	float x1, y1;
	vkg_get_current_point(data, &x1, &y1);
	_elliptic_arc(data, x1, y1, x2+x1, y2+y1, largeArc, sweepFlag, rx, ry, phi);
}

void VkgContext::ellipse (float radiusX, float radiusY, float x, float y, float rotationAngle) {
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

	finish_path();
	add_point (data, bottomCenterX, bottomCenterY);

	_curve_to (data, bottomRightX, bottomRightY, topRightX, topRightY, topCenterX, topCenterY);
	_curve_to (data, topLeftX, topLeftY, bottomLeftX, bottomLeftY, bottomCenterX, bottomCenterY);

	data->pathes[data->pathPtr] |= PATH_CLOSED_BIT;
	finish_path();
}

VkgSurface VkgContext::get_target () {
	return data->pSurf;
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
}
