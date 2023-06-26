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

#include <vkg/vkg.h>
#include <vkg/internal.hh>

#define VKVG_RECORDING_INIT_BUFFER_SIZE_TRESHOLD	64
#define VKVG_RECORDING_INIT_BUFFER_SIZE				1024
#define VKVG_RECORDING_INIT_COMMANDS_COUNT			64

vkg_recording* _new_recording () {
	vkg_recording*        rec = io_new(vkg_recording); /// i've been waiting for you obi rec.  the circular dependency is now complete now i am the main
	rec->commandsReservedCount = VKVG_RECORDING_INIT_COMMANDS_COUNT;
	rec->bufferReservedSize	   = VKVG_RECORDING_INIT_BUFFER_SIZE;
	rec->commands 			   = (vkg_record*)malloc(rec->commandsReservedCount * sizeof(vkg_record));
	rec->buffer 			   = malloc (rec->bufferReservedSize);
	return rec;
}

void _destroy_recording (vkg_recording* rec) {
	if (!rec)
		return;
	for (uint32_t i=0; i<rec->commandsCount; i++) {
		if (rec->commands[i].cmd == VKVG_CMD_SET_SOURCE)
			VkgPattern::drop((VkgPattern)(rec->buffer + rec->commands[i].dataOffset));
		else if (rec->commands[i].cmd == VKVG_CMD_SET_SOURCE_SURFACE)
			vkg_surface_drop((VkgSurface)(rec->buffer + rec->commands[i].dataOffset + 2 * sizeof(float)));
	}
	free(rec->commands);
	free(rec->buffer);
	free(rec);
}

void _start_recording (VkgContext ctx) {
	if (ctx->recording)
		_destroy_recording(ctx->recording);
	ctx->recording = _new_recording();
}

vkg_recording* _stop_recording (VkgContext ctx) {
	vkg_recording* rec = ctx->recording;
	if (!rec)
		return NULL;
	if (!rec->commandsCount) {
		_destroy_recording(rec);
		ctx->recording = NULL;
		return NULL;
	}
	/*rec->buffer = realloc(rec->buffer, rec->bufferSize);
	rec->commands = (vkg_record_t*)realloc(rec->commands, rec->commandsCount * sizeof (vkg_record_t));*/
	ctx->recording = NULL;
	return rec;
}

void* _ensure_recording_buffer (vkg_recording* rec, size_t size) {
	if (rec->bufferReservedSize >= rec->bufferSize - VKVG_RECORDING_INIT_BUFFER_SIZE_TRESHOLD - size) {
		rec->bufferReservedSize += VKVG_RECORDING_INIT_BUFFER_SIZE;
		rec->buffer = realloc(rec->buffer, rec->bufferReservedSize);
	}
	return rec->buffer + rec->bufferSize;
}

void* _advance_recording_buffer_unchecked (vkg_recording* rec, size_t size) {
	rec->bufferSize += size;
	return rec->buffer + rec->bufferSize;
}

#define STORE_FLOATS(floatcount)										\
	for (i=0; i<floatcount; i++) {										\
		buff = _ensure_recording_buffer (rec, sizeof(float));			\
		*(float*)buff = (float)va_arg(args, double);					\
		buff = _advance_recording_buffer_unchecked (rec, sizeof(float));\
	}
#define STORE_BOOLS(count)												\
	for (i=0; i<count; i++) {											\
		buff = _ensure_recording_buffer (rec, sizeof(bool));			\
		*(bool*)buff = (bool)va_arg(args, int);							\
		_advance_recording_buffer_unchecked (rec, sizeof(bool));		\
	}
#define STORE_UINT32(count)												\
	for (i=0; i<count; i++) {											\
		buff = _ensure_recording_buffer (rec, sizeof(uint32_t));		\
		*(uint32_t*)buff = (uint32_t)va_arg(args, uint32_t);				\
		buff = _advance_recording_buffer_unchecked (rec, sizeof(uint32_t));	\
	}

void _record (vkg_recording* rec,...) {
	va_list args;
	va_start(args, rec);

	uint32_t cmd = va_arg(args, uint32_t);

	if (rec->commandsCount == rec->commandsReservedCount) {
		rec->commandsReservedCount += VKVG_RECORDING_INIT_COMMANDS_COUNT;
		rec->commands = (vkg_record_t*)realloc(rec->commands, rec->commandsReservedCount * sizeof (vkg_record_t));
	}
	vkg_record_t* r = &rec->commands[rec->commandsCount++];
	r->cmd = cmd;
	r->dataOffset = rec->bufferSize;

	char* buff;
	int i = 0;

	if (cmd & VKVG_CMD_PATH_COMMANDS) {
		if ((cmd & VKVG_CMD_PATHPROPS_COMMANDS) == VKVG_CMD_PATHPROPS_COMMANDS) {
			switch (r->cmd) {
			case VKVG_CMD_SET_LINE_WIDTH:
			case VKVG_CMD_SET_MITER_LIMIT:
				STORE_FLOATS(1);
				break;
			case VKVG_CMD_SET_LINE_JOIN:
				STORE_UINT32(1);
				break;
			case VKVG_CMD_SET_LINE_CAP:
				STORE_UINT32(1);
				break;
			case VKVG_CMD_SET_OPERATOR:
				STORE_UINT32(1);
				break;
			case VKVG_CMD_SET_FILL_RULE:
				STORE_UINT32(1);
				break;
			case VKVG_CMD_SET_DASH:
				break;
			}
		} else {
			switch (cmd) {
			case VKVG_CMD_MOVE_TO:
			case VKVG_CMD_LINE_TO:
			case VKVG_CMD_REL_MOVE_TO:
			case VKVG_CMD_REL_LINE_TO:
				STORE_FLOATS(2);
				break;
			case VKVG_CMD_RECTANGLE:
			case VKVG_CMD_QUADRATIC_TO:
			case VKVG_CMD_REL_QUADRATIC_TO:
				STORE_FLOATS(4);
				break;
			case VKVG_CMD_ARC:
			case VKVG_CMD_ARC_NEG:
				STORE_FLOATS(5);
				break;
			case VKVG_CMD_CURVE_TO:
			case VKVG_CMD_REL_CURVE_TO:
				STORE_FLOATS(6);
				break;
			case VKVG_CMD_ELLIPTICAL_ARC_TO:
			case VKVG_CMD_REL_ELLIPTICAL_ARC_TO:
				STORE_FLOATS(5);
				STORE_BOOLS(2);
				break;
			case VKVG_CMD_NEW_PATH:
			case VKVG_CMD_NEW_SUB_PATH:
			case VKVG_CMD_CLOSE_PATH:
				break;
			}
		}
	} else if (!(r->cmd & VKVG_CMD_DRAW_COMMANDS)) {
		if (r->cmd & VKVG_CMD_TRANSFORM_COMMANDS) {
			switch (r->cmd) {
			case VKVG_CMD_TRANSLATE:
			case VKVG_CMD_SCALE:
				STORE_FLOATS(2);
				break;
			case VKVG_CMD_ROTATE:
				STORE_FLOATS(1);
				break;
			case VKVG_CMD_IDENTITY_MATRIX:
				break;
			case VKVG_CMD_SET_MATRIX:
			case VKVG_CMD_TRANSFORM:
				{
					buff = _ensure_recording_buffer (rec, sizeof(vkg_matrix));
					vkg_matrix* mat = (vkg_matrix*)va_arg(args, vkg_matrix*);
					memcpy(buff, mat, sizeof(vkg_matrix));
					buff = _advance_recording_buffer_unchecked (rec, sizeof(vkg_matrix));

				}
				break;
			}
		} else if (r->cmd & VKVG_CMD_PATTERN_COMMANDS) {
			switch (r->cmd) {
			case VKVG_CMD_SET_SOURCE_RGBA:
				STORE_FLOATS(4);
				break;
			case VKVG_CMD_SET_SOURCE_RGB:
				STORE_FLOATS(3);
				break;
			case VKVG_CMD_SET_SOURCE_COLOR:
				STORE_UINT32(1);
				break;
			case VKVG_CMD_SET_SOURCE:
				{
					buff = _ensure_recording_buffer (rec, sizeof(VkgPattern));
					VkgPattern pat = (VkgPattern)va_arg(args, VkgPattern);
					io_grab(vkg_pattern, pat);
					VkgPattern* pPat = (VkgPattern*)buff;
					*pPat = pat;
					_advance_recording_buffer_unchecked (rec, sizeof(VkgPattern));
				}
				break;
			case VKVG_CMD_SET_SOURCE_SURFACE:
				STORE_FLOATS(2);
				{
					buff = _ensure_recording_buffer (rec, sizeof(VkgSurface));
					VkgSurface surf = (VkgSurface)va_arg(args, VkgSurface);
					vkg_surface_reference(surf);
					*(VkgSurface*)buff = surf;
					_advance_recording_buffer_unchecked (rec, sizeof(VkgSurface));
				}
				break;
			}
		} else if (r->cmd & VKVG_CMD_TEXT_COMMANDS) {
			char* txt;
			int txtLen;
			switch (r->cmd) {
			case VKVG_CMD_SET_FONT_SIZE:
				STORE_UINT32(1);
				break;
			case VKVG_CMD_SHOW_TEXT:
			case VKVG_CMD_SET_FONT_FACE:
				txt = (char*)va_arg(args, char*);
				txtLen = strlen(txt);
				buff = _ensure_recording_buffer (rec, txtLen * sizeof(char));
				strcpy(buff, txt);
				_advance_recording_buffer_unchecked (rec, txtLen * sizeof(char));
				break;
			case VKVG_CMD_SET_FONT_PATH:
				break;
			}
		}
	}
	va_end(args);
}
void _replay_command (VkgContext ctx, VkgRecording rec, uint32_t index) {
	vkg_record_t* r = &rec->commands[index];
	float* floats = (float*)(rec->buffer + r->dataOffset);
	uint32_t* uints = (uint32_t*)floats;
	if (r->cmd&VKVG_CMD_PATH_COMMANDS) {
		if ((r->cmd&VKVG_CMD_RELATIVE_COMMANDS)==VKVG_CMD_RELATIVE_COMMANDS) {
			switch (r->cmd) {
			case VKVG_CMD_REL_MOVE_TO:
				vkg_rel_move_to(ctx, floats[0], floats[1]);
				return;
			case VKVG_CMD_REL_LINE_TO:
				vkg_rel_line_to(ctx, floats[0], floats[1]);
				return;
			case VKVG_CMD_REL_CURVE_TO:
				vkg_rel_curve_to (ctx, floats[0], floats[1], floats[2], floats[3], floats[4], floats[5]);
				return;
			case VKVG_CMD_REL_QUADRATIC_TO:
				vkg_rel_quadratic_to (ctx, floats[0], floats[1], floats[2], floats[3]);
				return;
			case VKVG_CMD_REL_ELLIPTICAL_ARC_TO:
				{
					bool* flags = (bool*)&floats[5];
					vkg_rel_elliptic_arc_to (ctx, floats[0], floats[1], flags[0], flags[1], floats[2], floats[3], floats[4]);
				}
				return;
			}
		}else if ((r->cmd&VKVG_CMD_PATHPROPS_COMMANDS)==VKVG_CMD_PATHPROPS_COMMANDS) {
			switch (r->cmd) {
			case VKVG_CMD_SET_LINE_WIDTH:
				vkg_set_line_width (ctx, floats[0]);
				return;
			case VKVG_CMD_SET_MITER_LIMIT:
				vkg_set_miter_limit (ctx, floats[0]);
				return;
			case VKVG_CMD_SET_LINE_JOIN:
				vkg_set_line_join (ctx, (vkg_line_join)uints[0]);
				return;
			case VKVG_CMD_SET_LINE_CAP:
				vkg_set_line_cap (ctx, (vkg_line_cap)uints[0]);
				return;
			case VKVG_CMD_SET_OPERATOR:
				vkg_set_operator (ctx, (vkg_operator)uints[0]);
				return;
			case VKVG_CMD_SET_FILL_RULE:
				vkg_set_fill_rule (ctx, (vkg_fill_rule)uints[0]);
				return;
			case VKVG_CMD_SET_DASH:
				vkg_set_dash(ctx, &floats[2],  uints[0], floats[1]);
				return;
			}
		} else {
			switch (r->cmd) {
			case VKVG_CMD_NEW_PATH:
				vkg_new_path (ctx);
				return;
			case VKVG_CMD_NEW_SUB_PATH:
				vkg_new_sub_path (ctx);
				return;
			case VKVG_CMD_CLOSE_PATH:
				vkg_close_path (ctx);
				return;
			case VKVG_CMD_RECTANGLE:
				vkg_rectangle (ctx, floats[0], floats[1], floats[2], floats[3]);
				return;
			case VKVG_CMD_ARC:
				vkg_arc (ctx, floats[0], floats[1], floats[2], floats[3], floats[4]);
				return;
			case VKVG_CMD_ARC_NEG:
				vkg_arc (ctx, floats[0], floats[1], floats[2], floats[3], floats[4]);
				return;
			/*case VKVG_CMD_ELLIPSE:
				vkg_ellipse (ctx, floats[0], floats[1], floats[2], floats[3], floats[4]);
				break;*/
			case VKVG_CMD_MOVE_TO:
				vkg_move_to(ctx, floats[0], floats[1]);
				return;
			case VKVG_CMD_LINE_TO:
				vkg_line_to(ctx, floats[0], floats[1]);
				return;
			case VKVG_CMD_CURVE_TO:
				vkg_curve_to (ctx, floats[0], floats[1], floats[2], floats[3], floats[4], floats[5]);
				return;
			case VKVG_CMD_ELLIPTICAL_ARC_TO:
				{
					bool* flags = (bool*)&floats[5];
					vkg_elliptic_arc_to (ctx, floats[0], floats[1], flags[0], flags[1], floats[2], floats[3], floats[4]);
				}
				return;
			case VKVG_CMD_QUADRATIC_TO:
				vkg_quadratic_to (ctx, floats[0], floats[1], floats[2], floats[3]);
				return;
			}
		}
	} else if (r->cmd & VKVG_CMD_DRAW_COMMANDS) {
		switch (r->cmd) {
		case VKVG_CMD_PAINT:
			vkg_paint (ctx);
			return;
		case VKVG_CMD_FILL:
			vkg_fill (ctx);
			return;
		case VKVG_CMD_STROKE:
			vkg_stroke (ctx);
			return;
		case VKVG_CMD_CLIP:
			vkg_clip (ctx);
			return;
		case VKVG_CMD_CLEAR:
			vkg_clear (ctx);
			return;
		case VKVG_CMD_FILL_PRESERVE:
			vkg_fill_preserve (ctx);
			return;
		case VKVG_CMD_STROKE_PRESERVE:
			vkg_stroke_preserve (ctx);
			return;
		case VKVG_CMD_CLIP_PRESERVE:
			vkg_clip_preserve (ctx);
			return;
		}
	} else if (r->cmd & VKVG_CMD_TRANSFORM_COMMANDS) {
		switch (r->cmd) {
		case VKVG_CMD_TRANSLATE:
			vkg_translate (ctx, floats[0], floats[1]);
			return;
		case VKVG_CMD_SCALE:
			vkg_scale (ctx, floats[0], floats[1]);
			return;
		case VKVG_CMD_ROTATE:
			vkg_rotate (ctx, floats[0]);
			return;
		case VKVG_CMD_IDENTITY_MATRIX:
			vkg_identity_matrix (ctx);
			return;
		case VKVG_CMD_TRANSFORM:
			{
				vkg_matrix* mat = (vkg_matrix*)&floats[0];
				vkg_transform (ctx, mat);
			}
			return;
		case VKVG_CMD_SET_MATRIX:
			{
				vkg_matrix* mat = (vkg_matrix*)&floats[0];
				vkg_set_matrix (ctx, mat);
			}
			return;
		}
	} else if (r->cmd & VKVG_CMD_PATTERN_COMMANDS) {
		switch (r->cmd) {
		case VKVG_CMD_SET_SOURCE_RGB:
			vkg_set_source_rgb (ctx, floats[0], floats[1], floats[2]);
			return;
		case VKVG_CMD_SET_SOURCE_RGBA:
			vkg_set_source_rgba (ctx, floats[0], floats[1], floats[2], floats[3]);
			return;
		case VKVG_CMD_SET_SOURCE_COLOR:
			vkg_set_source_color (ctx, uints[0]);
			return;
		case VKVG_CMD_SET_SOURCE:
			{
				VkgPattern pat = *((VkgPattern*)(rec->buffer + r->dataOffset));
				vkg_set_source (ctx, pat);
			}
			return;
		case VKVG_CMD_SET_SOURCE_SURFACE:
			{
				VkgSurface surf = *((VkgSurface*)&floats[2]);
				vkg_set_source_surface (ctx, surf, floats[0], floats[1]);
			}
			return;
		}
	} else if (r->cmd & VKVG_CMD_TEXT_COMMANDS) {
		char* txt = (char*)floats;
		switch (r->cmd) {
		case VKVG_CMD_SET_FONT_SIZE:
			vkg_set_font_size (ctx, uints[0]);
			return;
		case VKVG_CMD_SET_FONT_FACE:
			vkg_select_font_face (ctx, txt);
			return;
		/*case VKVG_CMD_SET_FONT_PATH:
			vkg_load_font_from_path (ctx, txt);
			return;	*/
		case VKVG_CMD_SHOW_TEXT:
			vkg_show_text (ctx, txt);
			return;
		}
	} else {
		switch (r->cmd) {
		case VKVG_CMD_SAVE:
			vkg_save (ctx);
			return;
		case VKVG_CMD_RESTORE:
			vkg_restore (ctx);
			return;
		}
	}
	vke_log(VKE_LOG_ERR, "[REPLAY] unimplemented command: %.4x\n", r->cmd);
}


#ifdef VKVG_RECORDING
void vkg_start_recording (VkgContext ctx) {
	if (ctx->status)
		return;
	_start_recording(ctx);
}
VkgRecording vkg_stop_recording (VkgContext ctx) {
	if (ctx->status)
		return NULL;
	return _stop_recording (ctx);
}
uint32_t vkg_recording_get_count (VkgRecording rec) {
	if (!rec)
		return 0;
	return rec->commandsCount;
}
void* vkg_recording_get_data (VkgRecording rec) {
	if (!rec)
		return 0;
	return rec->buffer;
}
void vkg_recording_get_command (VkgRecording rec, uint32_t cmdIndex, uint32_t* cmd, void** dataOffset) {
	if (!rec)
		return;
	if (cmdIndex < rec->commandsCount) {
		*cmd = rec->commands[cmdIndex].cmd;
		*dataOffset = (void*)rec->commands[cmdIndex].dataOffset;
	} else {
		*cmd = 0;
		*dataOffset = NULL;
	}

}
void vkg_replay (VkgContext ctx, VkgRecording rec) {
	if (!rec)
		return;
	for (uint32_t i=0; i<rec->commandsCount; i++)
		_replay_command(ctx, rec, i);
}
void vkg_replay_command (VkgContext ctx, VkgRecording rec, uint32_t cmdIndex) {
	if (!rec)
		return;
	if (cmdIndex < rec->commandsCount)
		_replay_command(ctx, rec, cmdIndex);
}
void vkg_recording_destroy(VkgRecording rec) {
	if (!rec)
		return;
	_destroy_recording(rec);
}
#endif
