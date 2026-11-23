/*
** Desktop harness for the renderer (development only): a software Vulkan device (lavapipe) and an offscreen target
** whose pixels are read back and written as PNG. Lets every screen be looked at without a console.
*/
#pragma once

#include "../gfx/vkr.h"

struct vk_host;

struct vk_host *vk_host_create(uint32_t width, uint32_t height, char const *font_dir);
struct vkr *vk_host_renderer(struct vk_host *h);
/* Records the frame (the callback fills the renderer's draw list), submits, waits, reads back, writes `png_path`. */
bool vk_host_render(struct vk_host *h, void (*draw)(struct vkr *r, void *user), void *user, char const *png_path);
