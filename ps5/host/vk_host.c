/*
** Desktop harness: lavapipe (software Vulkan) + offscreen render target + PNG writer. Development only.
*/
#include "vk_host.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, char const *name);

struct vk_host {
    struct vkr_ctx ctx;
    struct vkr *r;
    VkImage image;
    VkImageView view;
    VkDeviceMemory image_mem;
    VkBuffer readback;
    VkDeviceMemory readback_mem;
    void *readback_map;
    VkCommandPool pool;
    VkCommandBuffer cmd;
    VkFence fence;
    PFN_vkQueueSubmit QueueSubmit;
    PFN_vkWaitForFences WaitForFences;
    PFN_vkResetFences ResetFences;
    PFN_vkBeginCommandBuffer Begin;
    PFN_vkEndCommandBuffer End;
    PFN_vkResetCommandBuffer Reset;
    PFN_vkCmdCopyImageToBuffer CopyToBuffer;
};

static void
hlog(char const *fmt, ...)
{
    va_list a;
    va_start(a, fmt);
    vfprintf(stderr, fmt, a);
    fputc('\n', stderr);
    va_end(a);
}

static uint32_t
mem_type(VkPhysicalDeviceMemoryProperties const *m, uint32_t bits, VkMemoryPropertyFlags want)
{
    for (uint32_t i = 0; i < m->memoryTypeCount; i++)
        if ((bits & (1u << i)) && (m->memoryTypes[i].propertyFlags & want) == want)
            return i;
    return UINT32_MAX;
}

#define GI(inst, name) ((PFN_vk##name)vkGetInstanceProcAddr(inst, "vk" #name))

struct vk_host *
vk_host_create(uint32_t width, uint32_t height, char const *font_dir)
{
    struct vk_host *h = calloc(1, sizeof(*h));
    VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "lantern-host", .apiVersion = VK_API_VERSION_1_3};
    VkInstanceCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app};
    VkInstance inst;
    if (GI(NULL, CreateInstance)(&ici, NULL, &inst) != VK_SUCCESS) {
        hlog("FAIL vkCreateInstance");
        return NULL;
    }
    uint32_t n = 8;
    VkPhysicalDevice phys[8];
    GI(inst, EnumeratePhysicalDevices)(inst, &n, phys);
    /* the software device if present */
    VkPhysicalDevice pd = phys[0];
    for (uint32_t i = 0; i < n; i++) {
        VkPhysicalDeviceProperties p;
        GI(inst, GetPhysicalDeviceProperties)(phys[i], &p);
        if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
            pd = phys[i];
    }
    VkPhysicalDeviceProperties props;
    GI(inst, GetPhysicalDeviceProperties)(pd, &props);
    hlog("device: %s", props.deviceName);
    VkQueueFamilyProperties fam[8];
    uint32_t nf = 8, family = UINT32_MAX;
    GI(inst, GetPhysicalDeviceQueueFamilyProperties)(pd, &nf, fam);
    for (uint32_t i = 0; i < nf; i++)
        if (fam[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            family = i;
            break;
        }
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qi = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = family, .queueCount = 1, .pQueuePriorities = &prio};
    VkPhysicalDeviceVulkan13Features f13 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, .synchronization2 = VK_TRUE, .dynamicRendering = VK_TRUE};
    VkDeviceCreateInfo dci = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .pNext = &f13, .queueCreateInfoCount = 1, .pQueueCreateInfos = &qi};
    VkDevice dev;
    if (GI(inst, CreateDevice)(pd, &dci, NULL, &dev) != VK_SUCCESS) {
        hlog("FAIL vkCreateDevice");
        return NULL;
    }
    PFN_vkGetDeviceProcAddr gdpa = GI(inst, GetDeviceProcAddr);
    h->ctx = (struct vkr_ctx){
        .instance = inst, .phys = pd, .dev = dev, .family = family, .get_device_proc = gdpa,
        .color_format = VK_FORMAT_B8G8R8A8_UNORM, .width = width, .height = height,
        .scale = (float)height / 1080.0f, .final_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .log = hlog,
    };
    {
        static char paths[VKR_FONT_COUNT][512];
        static char const *const names[VKR_FONT_COUNT] = {"Fraunces-Bold.ttf", "Fraunces-SemiBold.ttf", "DMSans-Medium.ttf",
                                                          "DMSans-Bold.ttf", "JetBrainsMono-Medium.ttf", "JetBrainsMono-Bold.ttf"};
        for (int i = 0; i < VKR_FONT_COUNT; i++) {
            snprintf(paths[i], sizeof(paths[i]), "%s/%s", font_dir, names[i]);
            h->ctx.font_path[i] = paths[i];
        }
    }
    GI(inst, GetPhysicalDeviceMemoryProperties)(pd, &h->ctx.mem);
    ((PFN_vkGetDeviceQueue)gdpa(dev, "vkGetDeviceQueue"))(dev, family, 0, &h->ctx.queue);

    h->QueueSubmit = (PFN_vkQueueSubmit)gdpa(dev, "vkQueueSubmit");
    h->WaitForFences = (PFN_vkWaitForFences)gdpa(dev, "vkWaitForFences");
    h->ResetFences = (PFN_vkResetFences)gdpa(dev, "vkResetFences");
    h->Begin = (PFN_vkBeginCommandBuffer)gdpa(dev, "vkBeginCommandBuffer");
    h->End = (PFN_vkEndCommandBuffer)gdpa(dev, "vkEndCommandBuffer");
    h->Reset = (PFN_vkResetCommandBuffer)gdpa(dev, "vkResetCommandBuffer");
    h->CopyToBuffer = (PFN_vkCmdCopyImageToBuffer)gdpa(dev, "vkCmdCopyImageToBuffer");

    /* offscreen target + readback buffer */
    VkImageCreateInfo ii = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D, .format = h->ctx.color_format,
        .extent = {width, height, 1}, .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT, .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
    VkMemoryRequirements req;
    ((PFN_vkCreateImage)gdpa(dev, "vkCreateImage"))(dev, &ii, NULL, &h->image);
    ((PFN_vkGetImageMemoryRequirements)gdpa(dev, "vkGetImageMemoryRequirements"))(dev, h->image, &req);
    VkMemoryAllocateInfo mai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = req.size,
                                .memoryTypeIndex = mem_type(&h->ctx.mem, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};
    ((PFN_vkAllocateMemory)gdpa(dev, "vkAllocateMemory"))(dev, &mai, NULL, &h->image_mem);
    ((PFN_vkBindImageMemory)gdpa(dev, "vkBindImageMemory"))(dev, h->image, h->image_mem, 0);
    VkImageViewCreateInfo vi = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = h->image, .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = h->ctx.color_format, .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    ((PFN_vkCreateImageView)gdpa(dev, "vkCreateImageView"))(dev, &vi, NULL, &h->view);

    VkBufferCreateInfo bi = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = (VkDeviceSize)width * height * 4,
                             .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT, .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
    ((PFN_vkCreateBuffer)gdpa(dev, "vkCreateBuffer"))(dev, &bi, NULL, &h->readback);
    ((PFN_vkGetBufferMemoryRequirements)gdpa(dev, "vkGetBufferMemoryRequirements"))(dev, h->readback, &req);
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = mem_type(&h->ctx.mem, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    ((PFN_vkAllocateMemory)gdpa(dev, "vkAllocateMemory"))(dev, &mai, NULL, &h->readback_mem);
    ((PFN_vkBindBufferMemory)gdpa(dev, "vkBindBufferMemory"))(dev, h->readback, h->readback_mem, 0);
    ((PFN_vkMapMemory)gdpa(dev, "vkMapMemory"))(dev, h->readback_mem, 0, VK_WHOLE_SIZE, 0, &h->readback_map);

    VkCommandPoolCreateInfo cpi = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, .queueFamilyIndex = family};
    ((PFN_vkCreateCommandPool)gdpa(dev, "vkCreateCommandPool"))(dev, &cpi, NULL, &h->pool);
    VkCommandBufferAllocateInfo cai = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = h->pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1};
    ((PFN_vkAllocateCommandBuffers)gdpa(dev, "vkAllocateCommandBuffers"))(dev, &cai, &h->cmd);
    VkFenceCreateInfo fci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    ((PFN_vkCreateFence)gdpa(dev, "vkCreateFence"))(dev, &fci, NULL, &h->fence);

    h->r = vkr_create(&h->ctx);
    if (!h->r)
        return NULL;
    return h;
}

struct vkr *
vk_host_renderer(struct vk_host *h)
{
    return h->r;
}

static bool
write_png(char const *path, uint8_t const *bgra, uint32_t w, uint32_t h)
{
    size_t row = (size_t)w * 3 + 1, rawlen = row * h;
    uint8_t *raw = malloc(rawlen);
    for (uint32_t y = 0; y < h; y++) {
        uint8_t *d = raw + y * row;
        d[0] = 0;
        for (uint32_t x = 0; x < w; x++) {
            uint8_t const *s = bgra + ((size_t)y * w + x) * 4;
            d[1 + x * 3 + 0] = s[2];
            d[1 + x * 3 + 1] = s[1];
            d[1 + x * 3 + 2] = s[0];
        }
    }
    uLongf clen = compressBound((uLong)rawlen);
    uint8_t *comp = malloc(clen);
    if (compress2(comp, &clen, raw, (uLong)rawlen, 6) != Z_OK)
        return false;
    FILE *f = fopen(path, "wb");
    if (!f)
        return false;
    static uint8_t const sig[8] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    fwrite(sig, 1, 8, f);
#define BE32(v) (uint8_t[4]){(uint8_t)((v) >> 24), (uint8_t)((v) >> 16), (uint8_t)((v) >> 8), (uint8_t)(v)}
    uint8_t ihdr[13];
    memcpy(ihdr, BE32(w), 4);
    memcpy(ihdr + 4, BE32(h), 4);
    ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = ihdr[11] = ihdr[12] = 0;
    struct { char const *tag; uint8_t const *data; uint32_t len; } chunks[3] = {{"IHDR", ihdr, 13}, {"IDAT", comp, (uint32_t)clen}, {"IEND", NULL, 0}};
    for (int i = 0; i < 3; i++) {
        fwrite(BE32(chunks[i].len), 1, 4, f);
        fwrite(chunks[i].tag, 1, 4, f);
        if (chunks[i].len)
            fwrite(chunks[i].data, 1, chunks[i].len, f);
        uLong crc = crc32(0, (Bytef const *)chunks[i].tag, 4);
        if (chunks[i].len)
            crc = crc32(crc, chunks[i].data, chunks[i].len);
        fwrite(BE32((uint32_t)crc), 1, 4, f);
    }
    fclose(f);
    free(raw);
    free(comp);
    return true;
}

bool
vk_host_render(struct vk_host *h, void (*draw)(struct vkr *r, void *user), void *user, char const *png_path)
{
    VkCommandBufferBeginInfo bi = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    vkr_begin_frame(h->r);
    draw(h->r, user);
    h->Reset(h->cmd, 0);
    h->Begin(h->cmd, &bi);
    vkr_end_frame(h->r, h->cmd, h->image, h->view);
    VkBufferImageCopy cp = {.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, .imageExtent = {h->ctx.width, h->ctx.height, 1}};
    h->CopyToBuffer(h->cmd, h->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, h->readback, 1, &cp);
    h->End(h->cmd);
    VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &h->cmd};
    h->ResetFences(h->ctx.dev, 1, &h->fence);
    if (h->QueueSubmit(h->ctx.queue, 1, &si, h->fence) != VK_SUCCESS)
        return false;
    if (h->WaitForFences(h->ctx.dev, 1, &h->fence, VK_TRUE, UINT64_C(60000000000)) != VK_SUCCESS)
        return false;
    return png_path ? write_png(png_path, h->readback_map, h->ctx.width, h->ctx.height) : true;
}
