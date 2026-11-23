/*
** Lantern Miner PS5 renderer: an immediate-mode 2D drawer on plain Vulkan (rectangles, gradients, triangles, circles,
** lines, additive radial glow, images, text). Everything is drawn in "virtual units" on a 1920x1080 canvas, scaled by
** ctx.scale to the real target (4K output = 2.0, so one game pixel of the 960x540 world is exactly 4 device pixels).
**
** Derived from the emberSX renderer. Written for what the PS5 RADV driver runs: dynamic rendering, no MSAA, no image
** queries in shaders, small push constants.
*/
#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <stdbool.h>
#include <stdint.h>

#define VKR_RGBA(r, g, b, a) ((uint32_t)(r) | ((uint32_t)(g) << 8) | ((uint32_t)(b) << 16) | ((uint32_t)(a) << 24))
#define VKR_RGB(r, g, b) VKR_RGBA(r, g, b, 255)

enum vkr_font {
    VKR_DISPLAY,      /* Fraunces Bold */
    VKR_DISPLAY_SEMI, /* Fraunces SemiBold */
    VKR_SANS,         /* DM Sans Medium */
    VKR_SANS_BOLD,    /* DM Sans Bold */
    VKR_MONO,         /* JetBrains Mono Medium */
    VKR_MONO_BOLD,    /* JetBrains Mono Bold */
    VKR_FONT_COUNT
};

struct vkr_ctx {
    VkInstance instance;
    VkPhysicalDevice phys;
    VkDevice dev;
    VkQueue queue;
    uint32_t family;
    PFN_vkGetDeviceProcAddr get_device_proc;
    VkPhysicalDeviceMemoryProperties mem;
    VkFormat color_format;       /* format of the target images */
    uint32_t width, height;      /* target size in pixels */
    float scale;                 /* target pixels per virtual unit (height / 1080) */
    VkImageLayout final_layout;  /* layout the target is left in (PRESENT_SRC_KHR or TRANSFER_SRC_OPTIMAL) */
    char const *font_path[VKR_FONT_COUNT];
    void (*log)(char const *fmt, ...);
};

struct vkr;

struct vkr *vkr_create(struct vkr_ctx const *ctx);

/* Frame: begin, queue draws in back-to-front order, then record everything into `cmd` (already begun). */
void vkr_begin_frame(struct vkr *r);
bool vkr_end_frame(struct vkr *r, VkCommandBuffer cmd, VkImage image, VkImageView view);
float vkr_scale(struct vkr const *r);
void vkr_set_gamma(struct vkr *r, float gamma);

/* true = following draws blend additively ("lighter"), false = normal alpha blending. */
void vkr_blend(struct vkr *r, bool additive);

void vkr_rect(struct vkr *r, float x, float y, float w, float h, uint32_t color);
void vkr_rect_grad(struct vkr *r, float x, float y, float w, float h, uint32_t top, uint32_t bottom);
void vkr_rect_gradh(struct vkr *r, float x, float y, float w, float h, uint32_t left, uint32_t right);
void vkr_tri(struct vkr *r, float x0, float y0, float x1, float y1, float x2, float y2, uint32_t color);
/* Convex polygon as a fan; xy = x0,y0,x1,y1,... */
void vkr_poly(struct vkr *r, float const *xy, int n, uint32_t color);
void vkr_rrect(struct vkr *r, float x, float y, float w, float h, float radius, uint32_t color);
void vkr_rrect_grad(struct vkr *r, float x, float y, float w, float h, float radius, uint32_t top, uint32_t bottom);
void vkr_shadow(struct vkr *r, float x, float y, float w, float h, float radius, float blur, uint32_t color);
void vkr_ring(struct vkr *r, float x, float y, float w, float h, float radius, float thickness, uint32_t color);
void vkr_circle(struct vkr *r, float cx, float cy, float radius, uint32_t color);
void vkr_circle_ring(struct vkr *r, float cx, float cy, float radius, float thickness, uint32_t color);
/* A line from (x0,y0) to (x1,y1) with round caps. */
void vkr_line(struct vkr *r, float x0, float y0, float x1, float y1, float thickness, uint32_t color);
/* Radial glow: alpha of `color` at the centre, falling to `mid` (fraction of it) at half the radius and 0 at the edge. */
void vkr_glow(struct vkr *r, float cx, float cy, float radius, uint32_t color, float mid);
/* Darkening toward the edges: transparent inside r0, `alpha` of `color` at r1 and beyond (centre cx,cy in virtual units). */
void vkr_vignette(struct vkr *r, float x, float y, float w, float h, float cx, float cy, float r0, float r1, uint32_t color, float alpha);
/* Horizontal scanline overlay over a rectangle: dark lines every `period` target pixels. */
void vkr_scanlines(struct vkr *r, float x, float y, float w, float h, float period_px, uint32_t color, float strength);

/* Runtime images: RGBA 8-bit, tightly packed. Create and destroy only between frames. Returns a handle or -1. */
int vkr_image_create(struct vkr *r, uint32_t w, uint32_t h, uint32_t const *rgba);
void vkr_image_destroy(struct vkr *r, int id);
void vkr_image(struct vkr *r, int id, float x, float y, float w, float h, float radius, uint32_t color);
void vkr_image_uv(struct vkr *r, int id, float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t color);

/* y is the top of the line box. The nearest baked size is used and scaled to `size`. UTF-8 (Latin punctuation set). */
void vkr_text(struct vkr *r, enum vkr_font font, float size, float x, float y, uint32_t color, char const *text);
float vkr_text_width(struct vkr *r, enum vkr_font font, float size, char const *text);

/* Virtual canvas size. */
#define VKR_W 1920.0f
#define VKR_H 1080.0f
