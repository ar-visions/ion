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
#ifndef VKENGINE_H
#define VKENGINE_H

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>

#undef APIENTRY

#include <vk/vk.hpp>
#include <vkh/vkh.h>

//using namespace ion; /// bad form but its important

#define FENCE_TIMEOUT UINT16_MAX

//console colors for debug output on stdout with debug utils or debug report
#ifdef __unix__
	#define KNRM  "\x1b[0m"
	#define KRED  "\x1B[41m\x1B[37m"
	#define KGRN  "\x1B[42m\x1B[30m"
	#define KYEL  "\x1B[43m\x1B[30m"
	#define KBLU  "\x1B[44m\x1B[30m"
#else
	#define KNRM  ""
	#define KRED  ""
	#define KGRN  ""
	#define KYEL  ""
	#define KBLU  ""
	#define KMAG  ""
	#define KCYN  ""
	#define KWHT  ""
#endif

struct GLFWwindow;

typedef void (*FnKey)   (ion::mx&, int, int, int, int);
typedef void (*FnButton)(ion::mx&, int, int, int);
typedef void (*FnMove)  (ion::mx&, double, double);
typedef void (*FnScroll)(ion::mx&, double, double);
typedef void (*FnChar)  (ion::mx&, unsigned int);

/// merging app & engine makes sense.
/// also reducing down what is in vkvg: redundancy in copies of vkh device data and its props (replaced with methods)
typedef struct _vk_engine_t {
	size_t								refs;
	ion::Window							vk_gpu;
	ion::Device							vk_device;
	VkhPhyInfo							pi;
	ion::Vulkan							vk;
	VkInstance							instance;
	uint32_t 							version;
	VkPhysicalDeviceMemoryProperties	memory_properties;
	VkPhysicalDeviceProperties			gpu_props;
	VkhDevice							vkh;
	struct GLFWwindow*					window;
	VkhPresenter						renderer;
	VkSampleCountFlagBits				max_samples;
	VmaAllocator						allocator;

	FnKey								fn_key;
	FnButton							fn_button;
	FnMove								fn_move;
	FnScroll							fn_scroll;
	FnChar								fn_char;

	ion::mx								user; // this is so we have a user identifier and type
	///
} vk_engine_t;

vk_engine_t*   vkengine_create(
	uint32_t version_major, uint32_t version_minor, const char* app_name,
	VkPhysicalDeviceType preferedGPU, VkPresentModeKHR presentMode, VkSampleCountFlagBits max_samples, /// max_samples is a soft max for users of the app; it must also be checked against the hardware sample max
	uint32_t width, uint32_t height, int dpi_index, ion::mx &user);

void 				vkengine_dump_available_layers   	();
bool 				vkengine_try_get_phyinfo 			(VkhPhyInfo* phys, uint32_t phyCount, VkPhysicalDeviceType gpuType, VkhPhyInfo* phy);
void 				vkengine_drop						(VkEngine e);
VkEngine 			vkengine_grab						(VkEngine e);
bool 				vkengine_should_close				(VkEngine e);
bool 				vkengine_poll_events				(VkEngine e);
void 				vkengine_close						(VkEngine e);
void 				vkengine_dump_Infos					(VkEngine e);
void 				vkengine_set_title					(VkEngine e, const char* title);
VkInstance			vkengine_get_instance				(VkEngine e);
VkDevice			vkengine_get_device					(VkEngine e);
VkPhysicalDevice	vkengine_get_physical_device		(VkEngine e);
VkQueue				vkengine_get_queue					(VkEngine e);
uint32_t			vkengine_get_queue_fam_idx			(VkEngine e);
void 				vkengine_get_queues_properties     	(VkEngine e, VkQueueFamilyProperties** qFamProps, uint32_t* count);
void 				vkengine_key_callback 				(VkEngine e, FnKey fn_key);
void 				vkengine_button_callback 			(VkEngine e, FnButton button);
void 				vkengine_move_callback 				(VkEngine e, FnMove move);
void 				vkengine_scroll_callback 			(VkEngine e, FnScroll scroll);
void 				vkengine_char_callback 				(VkEngine e, FnChar chr);
void 				vkengine_wait_idle					(VkEngine e);
void 				vkengine_dpi_scale					(VkEngine e, float *sx, float *sy, int monitor_index);

#endif
