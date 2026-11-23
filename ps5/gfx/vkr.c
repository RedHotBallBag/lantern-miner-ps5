/*
** Lantern Miner PS5 renderer. See vkr.h.
**
** Structure follows the proven pieces of PS5 Vulkan's radv_smoke.c (Mihawk-99, GPL-3.0-or-later): commands loaded
** through vkGetDeviceProcAddr, host-visible staging buffers, VK_KHR_synchronization2 barriers, dynamic rendering.
*/
#include "vkr.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* stb_truetype without libm: no pow/fmod/cos/acos (SDF and curve-flattening paths), floor/ceil written out. */
static inline int vkr_ifloor(double x) { int i = (int)x; return ((double)i > x) ? i - 1 : i; }
static inline int vkr_iceil(double x) { int i = (int)x; return ((double)i < x) ? i + 1 : i; }
static inline double vkr_fabs(double x) { return x < 0 ? -x : x; }
static inline double vkr_unused1(double x) { (void)x; __builtin_trap(); }
static inline double vkr_unused2(double x, double y) { (void)x; (void)y; __builtin_trap(); }
#define STBTT_ifloor(x) vkr_ifloor(x)
#define STBTT_iceil(x) vkr_iceil(x)
#define STBTT_sqrt(x) __builtin_sqrt(x)
#define STBTT_fabs(x) vkr_fabs(x)
#define STBTT_pow(x, y) vkr_unused2((x), (y))
#define STBTT_fmod(x, y) vkr_unused2((x), (y))
#define STBTT_cos(x) vkr_unused1(x)
#define STBTT_acos(x) vkr_unused1(x)
#define STBTT_assert(x) ((void)0)
#define STB_TRUETYPE_IMPLEMENTATION
#include "third_party/stb_truetype.h"

#include "spv_shaders.h" /* generated: spv_ui_vert, spv_ui_frag (+ _size) */

/* ------------------------------------------------------------------ dispatch */

#define DEVICE_COMMANDS(X)                                                                                         \
    X(CreateBuffer) X(DestroyBuffer) X(GetBufferMemoryRequirements) X(AllocateMemory) X(FreeMemory)                \
    X(BindBufferMemory) X(MapMemory) X(CreateImage) X(DestroyImage) X(GetImageMemoryRequirements)                  \
    X(BindImageMemory) X(CreateImageView) X(DestroyImageView) X(CreateSampler) X(DestroySampler)                   \
    X(CreateShaderModule) X(DestroyShaderModule) X(CreateDescriptorSetLayout) X(DestroyDescriptorSetLayout)        \
    X(CreatePipelineLayout) X(DestroyPipelineLayout) X(CreateGraphicsPipelines) X(DestroyPipeline)                 \
    X(CreateDescriptorPool) X(DestroyDescriptorPool) X(AllocateDescriptorSets) X(UpdateDescriptorSets)             \
    X(CreateCommandPool) X(DestroyCommandPool) X(AllocateCommandBuffers) X(BeginCommandBuffer)                     \
    X(EndCommandBuffer) X(CreateFence) X(DestroyFence) X(WaitForFences) X(ResetFences) X(QueueSubmit)              \
    X(CmdPipelineBarrier2) X(CmdCopyBufferToImage) X(CmdBeginRendering) X(CmdEndRendering) X(CmdBindPipeline)      \
    X(CmdBindDescriptorSets) X(CmdBindVertexBuffers) X(CmdDraw) X(CmdSetViewport) X(CmdSetScissor)                 \
    X(CmdPushConstants)

struct vkfn {
#define DECL(name) PFN_vk##name name;
    DEVICE_COMMANDS(DECL)
#undef DECL
};

/* ------------------------------------------------------------------- types */

#define MAX_TEX 32
#define TEX_FIRST_IMAGE 2
#define MAX_ITEMS 2048
#define MAX_VERTS (1u << 18)
#define MAX_SIZES 5
#define N_ASCII 95
#define N_EXTRA 12
#define N_GLYPHS (N_ASCII + N_EXTRA)

enum { TEX_WHITE, TEX_FONT };

/* characters beyond ASCII the UI uses: middle dot, multiplication sign, dashes, curly quotes, ellipsis, bullet, arrows */
static int const k_extra[N_EXTRA] = {0xB7, 0xD7, 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2026, 0x2022, 0x25B6, 0x2665};

/* Baked sizes (virtual units) per font; text of any other size uses the nearest one, scaled. */
static struct { int n; float s[MAX_SIZES]; } const k_font_sizes[VKR_FONT_COUNT] = {
    [VKR_DISPLAY] = {4, {30, 44, 64, 96}},
    [VKR_DISPLAY_SEMI] = {3, {26, 34, 44}},
    [VKR_SANS] = {5, {18, 22, 27, 33, 42}},
    [VKR_SANS_BOLD] = {4, {20, 26, 33, 42}},
    [VKR_MONO] = {3, {18, 23, 29}},
    [VKR_MONO_BOLD] = {4, {18, 24, 32, 44}},
};

struct tex {
    bool used;
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    VkDescriptorSet set;
    uint32_t w, h;
};

struct buf {
    VkBuffer buffer;
    VkDeviceMemory memory;
    void *map;
    VkDeviceSize size;
};

struct uvtx {
    float x, y, u, v;
    uint32_t color;
    float aux[4];
    float extra;
};

struct ui_pc {
    float target[2];
    float gamma, pad;
};

struct item {
    int tex;
    bool additive;
    uint32_t first, count;
};

struct fontdata {
    unsigned char *file;
    stbtt_fontinfo info;
    stbtt_packedchar chars[MAX_SIZES][N_GLYPHS];
    float ascent[MAX_SIZES]; /* device pixels, at the baked size */
    float px[MAX_SIZES];     /* baked size in device pixels */
    bool ok;
};

struct vkr {
    struct vkr_ctx ctx;
    struct vkfn f;
    VkSampler sampler_linear;
    VkDescriptorSetLayout set_layout;
    VkPipelineLayout layout;
    VkPipeline pipe_alpha, pipe_add;
    VkDescriptorPool pool;
    VkCommandPool cmd_pool;
    VkCommandBuffer init_cmd;
    VkFence init_fence;
    struct tex tex[MAX_TEX];
    int ntex;
    struct buf stage; /* init-time uploads (font atlas, images) */
    struct buf vbuf;
    struct fontdata fonts[VKR_FONT_COUNT];
    int atlas_dim;
    float scale;
    float gamma;

    struct uvtx *verts; /* == vbuf.map */
    uint32_t nverts;
    struct item items[MAX_ITEMS];
    int nitems;
    int cur_tex;
    bool cur_add;
    uint32_t seg_first;
};

#define LOG(r, ...) do { if ((r)->ctx.log) (r)->ctx.log(__VA_ARGS__); } while (0)

/* ---------------------------------------------------------------- helpers */

static uint32_t
memory_type(struct vkr const *r, uint32_t bits, VkMemoryPropertyFlags wanted)
{
    for (uint32_t i = 0; i < r->ctx.mem.memoryTypeCount; i++)
        if ((bits & (1u << i)) && (r->ctx.mem.memoryTypes[i].propertyFlags & wanted) == wanted)
            return i;
    return UINT32_MAX;
}

static bool
buf_create(struct vkr *r, VkDeviceSize size, VkBufferUsageFlags usage, struct buf *out)
{
    VkBufferCreateInfo bi = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = usage,
                             .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
    VkMemoryRequirements req;
    memset(out, 0, sizeof(*out));
    out->size = size;
    if (r->f.CreateBuffer(r->ctx.dev, &bi, NULL, &out->buffer) != VK_SUCCESS)
        return false;
    r->f.GetBufferMemoryRequirements(r->ctx.dev, out->buffer, &req);
    uint32_t type = memory_type(r, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkMemoryAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = req.size, .memoryTypeIndex = type};
    return type != UINT32_MAX && r->f.AllocateMemory(r->ctx.dev, &ai, NULL, &out->memory) == VK_SUCCESS &&
           r->f.BindBufferMemory(r->ctx.dev, out->buffer, out->memory, 0) == VK_SUCCESS &&
           r->f.MapMemory(r->ctx.dev, out->memory, 0, VK_WHOLE_SIZE, 0, &out->map) == VK_SUCCESS;
}

static void
barrier_image(struct vkr *r, VkCommandBuffer cmd, VkImage image, VkImageLayout from, VkImageLayout to)
{
    VkImageMemoryBarrier2 b = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
        .oldLayout = from,
        .newLayout = to,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };
    VkDependencyInfo dep = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &b};
    r->f.CmdPipelineBarrier2(cmd, &dep);
}

static bool
submit_wait(struct vkr *r, VkCommandBuffer cmd)
{
    VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmd};
    r->f.ResetFences(r->ctx.dev, 1, &r->init_fence);
    if (r->f.QueueSubmit(r->ctx.queue, 1, &si, r->init_fence) != VK_SUCCESS)
        return false;
    return r->f.WaitForFences(r->ctx.dev, 1, &r->init_fence, VK_TRUE, UINT64_C(20000000000)) == VK_SUCCESS;
}

/* Creates a sampled texture with a descriptor set. `pixels` (optional, tightly packed) is uploaded now. */
static int
tex_create_at(struct vkr *r, int slot, uint32_t w, uint32_t h, VkFormat format, void const *pixels, size_t bytes)
{
    if (slot < 0 || slot >= MAX_TEX)
        return -1;
    struct tex *t = &r->tex[slot];
    VkImageCreateInfo ii = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D, .format = format,
        .extent = {w, h, 1}, .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL, .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkMemoryRequirements req;
    t->w = w;
    t->h = h;
    if (r->f.CreateImage(r->ctx.dev, &ii, NULL, &t->image) != VK_SUCCESS)
        return -1;
    r->f.GetImageMemoryRequirements(r->ctx.dev, t->image, &req);
    uint32_t type = memory_type(r, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkMemoryAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = req.size, .memoryTypeIndex = type};
    if (type == UINT32_MAX || r->f.AllocateMemory(r->ctx.dev, &ai, NULL, &t->memory) != VK_SUCCESS ||
        r->f.BindImageMemory(r->ctx.dev, t->image, t->memory, 0) != VK_SUCCESS)
        return -1;
    VkImageViewCreateInfo vi = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = t->image, .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format, .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };
    if (r->f.CreateImageView(r->ctx.dev, &vi, NULL, &t->view) != VK_SUCCESS)
        return -1;
    VkDescriptorSetAllocateInfo di = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = r->pool,
                                      .descriptorSetCount = 1, .pSetLayouts = &r->set_layout};
    if (t->set == VK_NULL_HANDLE && r->f.AllocateDescriptorSets(r->ctx.dev, &di, &t->set) != VK_SUCCESS)
        return -1;
    VkDescriptorImageInfo img = {.sampler = r->sampler_linear, .imageView = t->view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet wr = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = t->set, .dstBinding = 0,
                               .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &img};
    r->f.UpdateDescriptorSets(r->ctx.dev, 1, &wr, 0, NULL);

    if (pixels) {
        if (bytes > r->stage.size)
            return -1;
        memcpy(r->stage.map, pixels, bytes);
        VkCommandBufferBeginInfo bi = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
        r->f.BeginCommandBuffer(r->init_cmd, &bi);
        barrier_image(r, r->init_cmd, t->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkBufferImageCopy cp = {.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, .imageExtent = {w, h, 1}};
        r->f.CmdCopyBufferToImage(r->init_cmd, r->stage.buffer, t->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cp);
        barrier_image(r, r->init_cmd, t->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        r->f.EndCommandBuffer(r->init_cmd);
        if (!submit_wait(r, r->init_cmd))
            return -1;
    }
    t->used = true;
    return slot;
}

static int
tex_create(struct vkr *r, uint32_t w, uint32_t h, VkFormat format, void const *pixels, size_t bytes)
{
    if (r->ntex >= MAX_TEX)
        return -1;
    int id = tex_create_at(r, r->ntex, w, h, format, pixels, bytes);
    if (id >= 0)
        r->ntex++;
    return id;
}

static VkShaderModule
shader_module(struct vkr *r, uint32_t const *code, size_t bytes)
{
    VkShaderModuleCreateInfo si = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = bytes, .pCode = code};
    VkShaderModule m = VK_NULL_HANDLE;
    if (r->f.CreateShaderModule(r->ctx.dev, &si, NULL, &m) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    return m;
}

/* ------------------------------------------------------------------ fonts */

static bool
font_load(struct vkr *r, int fi, stbtt_pack_context *pc, int dim)
{
    struct fontdata *fd = &r->fonts[fi];
    char const *path = r->ctx.font_path[fi];
    FILE *f = path ? fopen(path, "rb") : NULL;
    long len;
    if (!f) {
        LOG(r, "FAIL font %s", path ? path : "(null)");
        return false;
    }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    fd->file = malloc((size_t)len);
    if (!fd->file || fread(fd->file, 1, (size_t)len, f) != (size_t)len) {
        fclose(f);
        return false;
    }
    fclose(f);
    if (!stbtt_InitFont(&fd->info, fd->file, 0))
        return false;
    for (int i = 0; i < k_font_sizes[fi].n; i++) {
        float px = k_font_sizes[fi].s[i] * r->scale;
        stbtt_pack_range ranges[2];
        memset(ranges, 0, sizeof(ranges));
        ranges[0].font_size = px;
        ranges[0].first_unicode_codepoint_in_range = 32;
        ranges[0].num_chars = N_ASCII;
        ranges[0].chardata_for_range = fd->chars[i];
        ranges[1].font_size = px;
        ranges[1].array_of_unicode_codepoints = (int *)k_extra;
        ranges[1].num_chars = N_EXTRA;
        ranges[1].chardata_for_range = fd->chars[i] + N_ASCII;
        if (!stbtt_PackFontRanges(pc, fd->file, 0, ranges, 2)) {
            LOG(r, "FAIL packing font %d size %.0f px (atlas %d too small)", fi, px, dim);
            return false;
        }
        int asc, desc, gap;
        stbtt_GetFontVMetrics(&fd->info, &asc, &desc, &gap);
        fd->ascent[i] = asc * stbtt_ScaleForPixelHeight(&fd->info, px);
        fd->px[i] = px;
    }
    fd->ok = true;
    return true;
}

static bool
build_font_atlas(struct vkr *r)
{
    int dim = r->scale > 1.25f ? 4096 : 2048;
    size_t bytes = (size_t)dim * dim;
    unsigned char *atlas = calloc(1, bytes);
    stbtt_pack_context pc;
    bool ok = true;
    if (!atlas)
        return false;
    r->atlas_dim = dim;
    if (!stbtt_PackBegin(&pc, atlas, dim, dim, 0, 1, NULL)) {
        free(atlas);
        return false;
    }
    stbtt_PackSetOversampling(&pc, 1, 1);
    for (int i = 0; i < VKR_FONT_COUNT && ok; i++)
        ok = font_load(r, i, &pc, dim);
    stbtt_PackEnd(&pc);
    if (ok) {
        if (bytes > r->stage.size) {
            LOG(r, "FAIL staging buffer smaller than the font atlas");
            ok = false;
        } else {
            ok = tex_create(r, (uint32_t)dim, (uint32_t)dim, VK_FORMAT_R8_UNORM, atlas, bytes) == TEX_FONT;
        }
    }
    free(atlas);
    return ok;
}

/* ------------------------------------------------------------------- init */

struct vkr *
vkr_create(struct vkr_ctx const *ctx)
{
    struct vkr *r = calloc(1, sizeof(*r));
    if (!r)
        return NULL;
    r->ctx = *ctx;
    r->scale = ctx->scale;
    r->gamma = 1.0f;
#define LOADF(name)                                                                                               \
    r->f.name = (PFN_vk##name)ctx->get_device_proc(ctx->dev, "vk" #name);                                          \
    if (!r->f.name) {                                                                                             \
        LOG(r, "FAIL missing vk" #name);                                                                          \
        return NULL;                                                                                              \
    }
    DEVICE_COMMANDS(LOADF)
#undef LOADF
    VkDevice dev = ctx->dev;

    VkSamplerCreateInfo sci = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .magFilter = VK_FILTER_LINEAR, .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST, .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .maxLod = 0.0f, .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
    };
    VkDescriptorSetLayoutBinding binding = {.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                            .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT};
    VkDescriptorSetLayoutCreateInfo dli = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 1, .pBindings = &binding};
    VkDescriptorPoolSize psize = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_TEX};
    VkDescriptorPoolCreateInfo dpi = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = MAX_TEX, .poolSizeCount = 1, .pPoolSizes = &psize};
    VkCommandPoolCreateInfo cpi = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                   .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, .queueFamilyIndex = ctx->family};
    VkFenceCreateInfo fci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (r->f.CreateSampler(dev, &sci, NULL, &r->sampler_linear) != VK_SUCCESS ||
        r->f.CreateDescriptorSetLayout(dev, &dli, NULL, &r->set_layout) != VK_SUCCESS ||
        r->f.CreateDescriptorPool(dev, &dpi, NULL, &r->pool) != VK_SUCCESS ||
        r->f.CreateCommandPool(dev, &cpi, NULL, &r->cmd_pool) != VK_SUCCESS ||
        r->f.CreateFence(dev, &fci, NULL, &r->init_fence) != VK_SUCCESS) {
        LOG(r, "FAIL basic objects");
        return NULL;
    }
    VkCommandBufferAllocateInfo cai = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = r->cmd_pool,
                                       .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1};
    if (r->f.AllocateCommandBuffers(dev, &cai, &r->init_cmd) != VK_SUCCESS) {
        LOG(r, "FAIL init command buffer");
        return NULL;
    }
    if (!buf_create(r, 4096u * 4096u, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &r->stage) ||
        !buf_create(r, (VkDeviceSize)MAX_VERTS * sizeof(struct uvtx), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &r->vbuf)) {
        LOG(r, "FAIL buffers");
        return NULL;
    }
    r->verts = r->vbuf.map;

    /* texture 0: white pixel; 1: font atlas */
    uint32_t white = 0xFFFFFFFFu;
    if (tex_create(r, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, &white, 4) != TEX_WHITE || !build_font_atlas(r)) {
        LOG(r, "FAIL textures");
        return NULL;
    }

    /* pipelines */
    VkShaderModule uv = shader_module(r, spv_ui_vert, spv_ui_vert_size);
    VkShaderModule uf = shader_module(r, spv_ui_frag, spv_ui_frag_size);
    if (!uv || !uf) {
        LOG(r, "FAIL shader modules");
        return NULL;
    }
    VkPushConstantRange upr = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(struct ui_pc)};
    VkPipelineLayoutCreateInfo pl = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1,
                                     .pSetLayouts = &r->set_layout, .pushConstantRangeCount = 1, .pPushConstantRanges = &upr};
    if (r->f.CreatePipelineLayout(dev, &pl, NULL, &r->layout) != VK_SUCCESS) {
        LOG(r, "FAIL pipeline layout");
        return NULL;
    }

    VkVertexInputBindingDescription vb = {0, sizeof(struct uvtx), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription va[5] = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32_SFLOAT, 8},
        {2, 0, VK_FORMAT_R8G8B8A8_UNORM, 16},
        {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 20},
        {4, 0, VK_FORMAT_R32_SFLOAT, 36},
    };
    VkPipelineVertexInputStateCreateInfo vi_ui = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1, .pVertexBindingDescriptions = &vb, .vertexAttributeDescriptionCount = 5, .pVertexAttributeDescriptions = va};
    VkPipelineInputAssemblyStateCreateInfo ia_list = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkPipelineViewportStateCreateInfo vp = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};
    VkPipelineRasterizationStateCreateInfo rs = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE, .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, .lineWidth = 1.0f};
    VkPipelineMultisampleStateCreateInfo ms = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
    VkPipelineColorBlendAttachmentState cba[2] = {
        {
            .blendEnable = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA, .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE, .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        },
        {
            .blendEnable = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA, .dstColorBlendFactor = VK_BLEND_FACTOR_ONE, .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO, .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE, .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        },
    };
    VkPipelineColorBlendStateCreateInfo cb0 = {.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1, .pAttachments = &cba[0]};
    VkPipelineColorBlendStateCreateInfo cb1 = cb0;
    cb1.pAttachments = &cba[1];
    VkDynamicState dyn[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo ds = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = 2, .pDynamicStates = dyn};
    VkPipelineRenderingCreateInfo ri = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, .colorAttachmentCount = 1, .pColorAttachmentFormats = &ctx->color_format};

    VkPipelineShaderStageCreateInfo us[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = uv, .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = uf, .pName = "main"}};
    VkGraphicsPipelineCreateInfo gp[2] = {
        {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .pNext = &ri, .stageCount = 2, .pStages = us,
         .pVertexInputState = &vi_ui, .pInputAssemblyState = &ia_list, .pViewportState = &vp, .pRasterizationState = &rs,
         .pMultisampleState = &ms, .pColorBlendState = &cb0, .pDynamicState = &ds, .layout = r->layout},
        {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .pNext = &ri, .stageCount = 2, .pStages = us,
         .pVertexInputState = &vi_ui, .pInputAssemblyState = &ia_list, .pViewportState = &vp, .pRasterizationState = &rs,
         .pMultisampleState = &ms, .pColorBlendState = &cb1, .pDynamicState = &ds, .layout = r->layout},
    };
    VkPipeline pipes[2];
    VkResult pr = r->f.CreateGraphicsPipelines(dev, VK_NULL_HANDLE, 2, gp, NULL, pipes);
    if (pr != VK_SUCCESS) {
        LOG(r, "FAIL vkCreateGraphicsPipelines (%d)", pr);
        return NULL;
    }
    r->pipe_alpha = pipes[0];
    r->pipe_add = pipes[1];
    LOG(r, "PASS renderer: %ux%u, scale %.2f, atlas %d", ctx->width, ctx->height, r->scale, r->atlas_dim);
    return r;
}

/* ------------------------------------------------------------- draw list */

void
vkr_begin_frame(struct vkr *r)
{
    r->nverts = 0;
    r->nitems = 0;
    r->cur_tex = TEX_WHITE;
    r->cur_add = false;
    r->seg_first = 0;
}

static void
close_segment(struct vkr *r)
{
    if (r->nverts > r->seg_first && r->nitems < MAX_ITEMS) {
        struct item *it = &r->items[r->nitems++];
        it->tex = r->cur_tex;
        it->additive = r->cur_add;
        it->first = r->seg_first;
        it->count = r->nverts - r->seg_first;
    }
    r->seg_first = r->nverts;
}

static void
use_tex(struct vkr *r, int id)
{
    if (id != r->cur_tex) {
        close_segment(r);
        r->cur_tex = id;
    }
}

void
vkr_blend(struct vkr *r, bool additive)
{
    if (additive != r->cur_add) {
        close_segment(r);
        r->cur_add = additive;
    }
}

static void
quad(struct vkr *r, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1,
     uint32_t c_tl, uint32_t c_tr, uint32_t c_bl, uint32_t c_br, float const aux[4], float extra)
{
    if (r->nverts + 6 > MAX_VERTS)
        return;
    struct uvtx *v = &r->verts[r->nverts];
    float const X[6] = {x0, x1, x0, x1, x1, x0};
    float const Y[6] = {y0, y0, y1, y0, y1, y1};
    float const U[6] = {u0, u1, u0, u1, u1, u0};
    float const V[6] = {v0, v0, v1, v0, v1, v1};
    uint32_t const C[6] = {c_tl, c_tr, c_bl, c_tr, c_br, c_bl};
    for (int i = 0; i < 6; i++) {
        v[i].x = X[i];
        v[i].y = Y[i];
        v[i].u = U[i];
        v[i].v = V[i];
        v[i].color = C[i];
        memcpy(v[i].aux, aux, sizeof(v[i].aux));
        v[i].extra = extra;
    }
    r->nverts += 6;
}

static void
tri(struct vkr *r, float x0, float y0, float x1, float y1, float x2, float y2, uint32_t color, float const aux[4])
{
    if (r->nverts + 3 > MAX_VERTS)
        return;
    struct uvtx *v = &r->verts[r->nverts];
    float const X[3] = {x0, x1, x2};
    float const Y[3] = {y0, y1, y2};
    for (int i = 0; i < 3; i++) {
        v[i].x = X[i];
        v[i].y = Y[i];
        v[i].u = v[i].v = 0.0f;
        v[i].color = color;
        memcpy(v[i].aux, aux, sizeof(v[i].aux));
        v[i].extra = 0.0f;
    }
    r->nverts += 3;
}

/* A rectangle drawn by the SDF shader: the quad is `pad` larger on each side; uv are centre-relative pixels. */
static void
sdf_quad(struct vkr *r, float x, float y, float w, float h, float radius, float pad, int mode, uint32_t top, uint32_t bottom, float extra)
{
    float s = r->scale;
    float cx = (x + w * 0.5f) * s, cy = (y + h * 0.5f) * s, hx = w * 0.5f * s, hy = h * 0.5f * s;
    float aux[4] = {hx, hy, radius * s, (float)mode};
    float p = pad * s + 1.0f;
    use_tex(r, TEX_WHITE);
    quad(r, cx - hx - p, cy - hy - p, cx + hx + p, cy + hy + p, -hx - p, -hy - p, hx + p, hy + p, top, top, bottom, bottom, aux, extra);
}

void
vkr_rect(struct vkr *r, float x, float y, float w, float h, uint32_t color)
{
    float s = r->scale;
    float const aux[4] = {0, 0, 0, 7.0f};
    use_tex(r, TEX_WHITE);
    quad(r, x * s, y * s, (x + w) * s, (y + h) * s, 0, 0, 0, 0, color, color, color, color, aux, 0.0f);
}

void
vkr_rect_grad(struct vkr *r, float x, float y, float w, float h, uint32_t top, uint32_t bottom)
{
    float s = r->scale;
    float const aux[4] = {0, 0, 0, 7.0f};
    use_tex(r, TEX_WHITE);
    quad(r, x * s, y * s, (x + w) * s, (y + h) * s, 0, 0, 0, 0, top, top, bottom, bottom, aux, 0.0f);
}

void
vkr_rect_gradh(struct vkr *r, float x, float y, float w, float h, uint32_t left, uint32_t right)
{
    float s = r->scale;
    float const aux[4] = {0, 0, 0, 7.0f};
    use_tex(r, TEX_WHITE);
    quad(r, x * s, y * s, (x + w) * s, (y + h) * s, 0, 0, 0, 0, left, right, left, right, aux, 0.0f);
}

void
vkr_tri(struct vkr *r, float x0, float y0, float x1, float y1, float x2, float y2, uint32_t color)
{
    float s = r->scale;
    float const aux[4] = {0, 0, 0, 7.0f};
    use_tex(r, TEX_WHITE);
    tri(r, x0 * s, y0 * s, x1 * s, y1 * s, x2 * s, y2 * s, color, aux);
}

void
vkr_poly(struct vkr *r, float const *xy, int n, uint32_t color)
{
    for (int i = 1; i + 1 < n; i++)
        vkr_tri(r, xy[0], xy[1], xy[i * 2], xy[i * 2 + 1], xy[i * 2 + 2], xy[i * 2 + 3], color);
}

void
vkr_rrect(struct vkr *r, float x, float y, float w, float h, float radius, uint32_t color)
{
    sdf_quad(r, x, y, w, h, radius, 0.0f, 2, color, color, 0.0f);
}

void
vkr_rrect_grad(struct vkr *r, float x, float y, float w, float h, float radius, uint32_t top, uint32_t bottom)
{
    sdf_quad(r, x, y, w, h, radius, 0.0f, 2, top, bottom, 0.0f);
}

void
vkr_shadow(struct vkr *r, float x, float y, float w, float h, float radius, float blur, uint32_t color)
{
    sdf_quad(r, x, y, w, h, radius, blur * 1.2f, 3, color, color, blur * r->scale);
}

void
vkr_ring(struct vkr *r, float x, float y, float w, float h, float radius, float thickness, uint32_t color)
{
    sdf_quad(r, x, y, w, h, radius, 0.0f, 4, color, color, thickness * r->scale);
}

void
vkr_circle(struct vkr *r, float cx, float cy, float radius, uint32_t color)
{
    sdf_quad(r, cx - radius, cy - radius, radius * 2, radius * 2, radius, 0.0f, 2, color, color, 0.0f);
}

void
vkr_circle_ring(struct vkr *r, float cx, float cy, float radius, float thickness, uint32_t color)
{
    sdf_quad(r, cx - radius, cy - radius, radius * 2, radius * 2, radius, 0.0f, 4, color, color, thickness * r->scale);
}

void
vkr_line(struct vkr *r, float x0, float y0, float x1, float y1, float thickness, uint32_t color)
{
    float s = r->scale;
    float cx = (x0 + x1) * 0.5f * s, cy = (y0 + y1) * 0.5f * s;
    float hx = (x1 - x0) * 0.5f * s, hy = (y1 - y0) * 0.5f * s;
    float t = thickness * s;
    float ex = (hx < 0 ? -hx : hx) + t + 1.0f, ey = (hy < 0 ? -hy : hy) + t + 1.0f;
    float aux[4] = {hx, hy, t, 5.0f};
    use_tex(r, TEX_WHITE);
    quad(r, cx - ex, cy - ey, cx + ex, cy + ey, -ex, -ey, ex, ey, color, color, color, color, aux, 0.0f);
}

void
vkr_glow(struct vkr *r, float cx, float cy, float radius, uint32_t color, float mid)
{
    float s = r->scale;
    float rad = radius * s;
    float aux[4] = {rad, 0, 0, 8.0f};
    use_tex(r, TEX_WHITE);
    quad(r, cx * s - rad, cy * s - rad, cx * s + rad, cy * s + rad, -rad, -rad, rad, rad, color, color, color, color, aux, mid);
}

void
vkr_scanlines(struct vkr *r, float x, float y, float w, float h, float period_px, uint32_t color, float strength)
{
    float s = r->scale;
    float aux[4] = {period_px, 0, 0, 9.0f};
    use_tex(r, TEX_WHITE);
    quad(r, x * s, y * s, (x + w) * s, (y + h) * s, 0, 0, 0, 0, color, color, color, color, aux, strength);
}

void
vkr_vignette(struct vkr *r, float x, float y, float w, float h, float cx, float cy, float r0, float r1, uint32_t color, float alpha)
{
    float s = r->scale;
    float aux[4] = {r0 * s, r1 * s, 0, 10.0f};
    use_tex(r, TEX_WHITE);
    quad(r, x * s, y * s, (x + w) * s, (y + h) * s, (x - cx) * s, (y - cy) * s, (x + w - cx) * s, (y + h - cy) * s,
         color, color, color, color, aux, alpha);
}

/* ------------------------------------------------------------ runtime images */

int
vkr_image_create(struct vkr *r, uint32_t w, uint32_t h, uint32_t const *rgba)
{
    /* The caller guarantees no frame is in flight (the platform waits for the frame's fence before the next one). */
    for (int i = TEX_FIRST_IMAGE; i < MAX_TEX; i++) {
        if (!r->tex[i].used) {
            if (i >= r->ntex)
                r->ntex = i + 1;
            int id = tex_create_at(r, i, w, h, VK_FORMAT_R8G8B8A8_UNORM, rgba, (size_t)w * h * 4);
            if (id < 0)
                vkr_image_destroy(r, i);
            return id;
        }
    }
    return -1;
}

void
vkr_image_destroy(struct vkr *r, int id)
{
    struct tex *t;
    if (id < TEX_FIRST_IMAGE || id >= MAX_TEX)
        return;
    t = &r->tex[id];
    if (t->view) r->f.DestroyImageView(r->ctx.dev, t->view, NULL);
    if (t->image) r->f.DestroyImage(r->ctx.dev, t->image, NULL);
    if (t->memory) r->f.FreeMemory(r->ctx.dev, t->memory, NULL);
    t->view = VK_NULL_HANDLE;
    t->image = VK_NULL_HANDLE;
    t->memory = VK_NULL_HANDLE;
    t->used = false; /* the descriptor set stays allocated and is rewritten by the next image */
}

void
vkr_image(struct vkr *r, int id, float x, float y, float w, float h, float radius, uint32_t color)
{
    float s = r->scale;
    float cx = (x + w * 0.5f) * s, cy = (y + h * 0.5f) * s, hx = w * 0.5f * s, hy = h * 0.5f * s;
    float aux[4] = {hx, hy, radius * s, radius > 0.0f ? 6.0f : 0.0f};
    if (id < TEX_FIRST_IMAGE || id >= MAX_TEX || !r->tex[id].used)
        return;
    use_tex(r, id);
    quad(r, cx - hx, cy - hy, cx + hx, cy + hy, 0.0f, 0.0f, 1.0f, 1.0f, color, color, color, color, aux, 0.0f);
}

void
vkr_image_uv(struct vkr *r, int id, float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t color)
{
    float s = r->scale;
    float aux[4] = {0, 0, 0, 0.0f};
    if (id < TEX_FIRST_IMAGE || id >= MAX_TEX || !r->tex[id].used)
        return;
    use_tex(r, id);
    quad(r, x * s, y * s, (x + w) * s, (y + h) * s, u0, v0, u1, v1, color, color, color, color, aux, 0.0f);
}

float
vkr_scale(struct vkr const *r)
{
    return r->scale;
}

void
vkr_set_gamma(struct vkr *r, float gamma)
{
    r->gamma = gamma < 0.5f ? 0.5f : gamma > 2.0f ? 2.0f : gamma;
}

/* ------------------------------------------------------------------- text */

static int
size_index(enum vkr_font font, float size)
{
    int best = 0;
    float bd = 1e9f;
    for (int i = 0; i < k_font_sizes[font].n; i++) {
        float d = k_font_sizes[font].s[i] > size ? k_font_sizes[font].s[i] - size : size - k_font_sizes[font].s[i];
        if (d < bd) {
            bd = d;
            best = i;
        }
    }
    return best;
}

/* Decodes one UTF-8 code point and returns its glyph slot (ASCII, the extra set, or '?'). */
static int
next_glyph(unsigned char const **p)
{
    unsigned c = **p;
    unsigned cp;
    if (c == 0)
        return -1;
    (*p)++;
    if (c < 0x80) {
        cp = c;
    } else if ((c & 0xE0) == 0xC0 && ((*p)[0] & 0xC0) == 0x80) {
        cp = ((c & 0x1F) << 6) | ((*p)[0] & 0x3F);
        (*p) += 1;
    } else if ((c & 0xF0) == 0xE0 && ((*p)[0] & 0xC0) == 0x80 && ((*p)[1] & 0xC0) == 0x80) {
        cp = ((c & 0x0F) << 12) | (((*p)[0] & 0x3F) << 6) | ((*p)[1] & 0x3F);
        (*p) += 2;
    } else {
        while (((**p) & 0xC0) == 0x80)
            (*p)++;
        cp = '?';
    }
    if (cp >= 32 && cp < 32 + N_ASCII)
        return (int)cp - 32;
    for (int i = 0; i < N_EXTRA; i++)
        if ((unsigned)k_extra[i] == cp)
            return N_ASCII + i;
    return '?' - 32;
}

float
vkr_text_width(struct vkr *r, enum vkr_font font, float size, char const *text)
{
    struct fontdata *fd = &r->fonts[font];
    int si = size_index(font, size);
    float f = size * r->scale / fd->px[si];
    float x = 0;
    unsigned char const *p = (unsigned char const *)text;
    if (!fd->ok)
        return 0;
    for (int g; (g = next_glyph(&p)) >= 0;)
        x += fd->chars[si][g].xadvance * f;
    return x / r->scale;
}

void
vkr_text(struct vkr *r, enum vkr_font font, float size, float x, float y, uint32_t color, char const *text)
{
    struct fontdata *fd = &r->fonts[font];
    int si = size_index(font, size);
    float f = size * r->scale / fd->px[si];
    float xpos = x * r->scale;
    float base = y * r->scale + fd->ascent[si] * f;
    float dim = (float)r->atlas_dim;
    unsigned char const *p = (unsigned char const *)text;
    float const aux[4] = {0, 0, 0, 1.0f};
    if (!fd->ok)
        return;
    use_tex(r, TEX_FONT);
    for (int g; (g = next_glyph(&p)) >= 0;) {
        stbtt_packedchar const *c = &fd->chars[si][g];
        quad(r, xpos + c->xoff * f, base + c->yoff * f, xpos + c->xoff2 * f, base + c->yoff2 * f,
             c->x0 / dim, c->y0 / dim, c->x1 / dim, c->y1 / dim, color, color, color, color, aux, 0.0f);
        xpos += c->xadvance * f;
    }
}

/* ---------------------------------------------------------------- recording */

bool
vkr_end_frame(struct vkr *r, VkCommandBuffer cmd, VkImage image, VkImageView view)
{
    close_segment(r);

    barrier_image(r, cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo att = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .imageView = view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE, .clearValue = {.color = {.float32 = {0.03f, 0.025f, 0.04f, 1.0f}}},
    };
    VkRenderingInfo rin = {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO, .renderArea = {{0, 0}, {r->ctx.width, r->ctx.height}},
                           .layerCount = 1, .colorAttachmentCount = 1, .pColorAttachments = &att};
    r->f.CmdBeginRendering(cmd, &rin);
    VkViewport vp = {0, 0, (float)r->ctx.width, (float)r->ctx.height, 0.0f, 1.0f};
    VkRect2D sc = {{0, 0}, {r->ctx.width, r->ctx.height}};
    r->f.CmdSetViewport(cmd, 0, 1, &vp);
    r->f.CmdSetScissor(cmd, 0, 1, &sc);

    VkDeviceSize zero = 0;
    r->f.CmdBindVertexBuffers(cmd, 0, 1, &r->vbuf.buffer, &zero);
    int bound = -1; /* 0 alpha pipeline, 1 additive pipeline */
    struct ui_pc upc = {{(float)r->ctx.width, (float)r->ctx.height}, r->gamma, 0.0f};
    for (int i = 0; i < r->nitems; i++) {
        struct item const *it = &r->items[i];
        int want = it->additive ? 1 : 0;
        if (bound != want) {
            r->f.CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, want ? r->pipe_add : r->pipe_alpha);
            if (bound < 0)
                r->f.CmdPushConstants(cmd, r->layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(upc), &upc);
            bound = want;
        }
        r->f.CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r->layout, 0, 1, &r->tex[it->tex].set, 0, NULL);
        r->f.CmdDraw(cmd, it->count, 1, it->first, 0);
    }
    r->f.CmdEndRendering(cmd);
    barrier_image(r, cmd, image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, r->ctx.final_layout);
    return true;
}
