/*
 * Lantern Miner PS5 console entry point (title PPSA58001): Vulkan display swapchain, DualSense, audio output, main loop.
 *
 * Vulkan instance/device/display setup adapted from PS5 Vulkan's radv_smoke.c (Copyright (C) 2026 Mihawk-99,
 * GPL-3.0-or-later) and verified on this console by the spike build. Audio follows PS5SX2's ProsperoAudio.cpp and input
 * follows launchbum's pad.cpp (both read on this machine). Private build: never distribute.
 */

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "../game/app.h"

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_icdGetInstanceProcAddr(VkInstance instance, char const *name);

int sceKernelDebugOutText(int channel, char const *text);
int ps5_klog_capture_stderr(char const *prefix);

/* SDK imports used here (the link recipe checks every one is exported by a module the title loads) */
int sceSystemServiceHideSplashScreen(void);
int sceSystemServiceLoadExec(char const *path, char *const argv[]);
int scePadInit(void);
int scePadOpen(int32_t user_id, int32_t type, int32_t index, void const *param);
int scePadReadState(int32_t handle, void *data);
int sceUserServiceInitialize(void const *params);
int sceUserServiceGetInitialUser(int32_t *user_id);
int sceAudioOutInit(void);
int sceAudioOutOpen(int user_id, int type, int index, unsigned len, unsigned freq, unsigned param);
int sceAudioOutOutput(int handle, void const *data);
struct pad_vibration { uint8_t large, small; };
int scePadSetVibration(int32_t handle, struct pad_vibration const *param);
int scePadSetVibrationMode(int32_t handle, int32_t mode);
#define PAD_VIBRATION_COMPATIBLE 2

#define OUT_W 3840
#define OUT_H 2160
#define DATA_DIR "/data/lantern-miner"
#define FALLBACK_DIR "/app0/data"

/* -------------------------------------------------------------------- log */

static FILE *g_log;
static pthread_mutex_t g_log_lock = PTHREAD_MUTEX_INITIALIZER;

static atomic_bool g_logging = true;

static void
report(char const *fmt, ...)
{
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    pthread_mutex_lock(&g_log_lock);
    {
        char k[600];
        snprintf(k, sizeof(k), "[LanternMiner] %s\n", line);
        sceKernelDebugOutText(0, k);
    }
    if (g_log && atomic_load(&g_logging)) {
        fprintf(g_log, "%s\n", line);
        fflush(g_log);
    }
    pthread_mutex_unlock(&g_log_lock);
}

static double
now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* ------------------------------------------------------------------- input */

struct pad_data {
    uint32_t buttons;
    uint8_t lx, ly, rx, ry, l2, r2, pad0, pad1;
    uint8_t rest[256];
};

static struct {
    int handle, user;
    uint32_t prev;
    double down_since[4], last_repeat[4];
    double last_try;
} g_pad = {.handle = -1, .user = -1, .last_try = -10.0};

/* A PS5 title's pad starts in haptics mode, where the two motor values are accepted but nothing is felt. */
static void
pad_rumble_mode(void)
{
    if (g_pad.handle >= 0)
        report("pad: rumble mode -> %x", scePadSetVibrationMode(g_pad.handle, PAD_VIBRATION_COMPATIBLE));
}

static void
pad_init(void)
{
    int32_t user = -1;
    sceUserServiceInitialize(NULL);
    sceUserServiceGetInitialUser(&user);
    g_pad.user = user;
    scePadInit();
    g_pad.handle = user >= 0 ? scePadOpen(user, 0, 0, NULL) : -1;
    report("pad: user %d handle %d", user, g_pad.handle);
    pad_rumble_mode();
}

static void
pad_poll(struct app_input *in, double now)
{
    static uint32_t const dirs[4] = {PAD_UP, PAD_DOWN, PAD_LEFT, PAD_RIGHT};
    struct pad_data d;
    uint32_t held;
    in->pressed = 0;
    if (g_pad.handle < 0) {
        in->connected = false;
        in->held = 0;
        if (g_pad.user >= 0 && now - g_pad.last_try > 1.0) {
            g_pad.last_try = now;
            g_pad.handle = scePadOpen(g_pad.user, 0, 0, NULL);
            pad_rumble_mode();
        }
        return;
    }
    memset(&d, 0, sizeof(d));
    d.lx = d.ly = d.rx = d.ry = 128;
    if (scePadReadState(g_pad.handle, &d) != 0) {
        in->connected = false;
        in->held = 0;
        g_pad.prev = 0;
        return;
    }
    in->connected = true;
    held = d.buttons & ~(PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT);
    /* left stick as a D-pad: a platformer wants a light touch to run (about 50% deflection) */
    if ((d.buttons & PAD_UP) || d.ly < 64) held |= PAD_UP;
    if ((d.buttons & PAD_DOWN) || d.ly > 192) held |= PAD_DOWN;
    if ((d.buttons & PAD_LEFT) || d.lx < 64) held |= PAD_LEFT;
    if ((d.buttons & PAD_RIGHT) || d.lx > 192) held |= PAD_RIGHT;
    in->pressed = held & ~g_pad.prev;
    for (int i = 0; i < 4; i++) {
        if (!(held & dirs[i]))
            continue;
        if (in->pressed & dirs[i]) {
            g_pad.down_since[i] = g_pad.last_repeat[i] = now;
        } else if (now - g_pad.down_since[i] >= 0.38 && now - g_pad.last_repeat[i] >= 0.07) {
            in->pressed |= dirs[i];
            g_pad.last_repeat[i] = now;
        }
    }
    in->held = held;
    g_pad.prev = held;
}

/* ------------------------------------------------------------------- audio */

static struct app *g_app;
static atomic_bool g_quit;

static void *
audio_thread(void *arg)
{
    enum { GRAIN = 256 };
    int handle;
    static int16_t pcm[GRAIN * 2];
    static float out[GRAIN * 2] __attribute__((aligned(64)));
    (void)arg;
    sceAudioOutInit();
    /* user 255 = system, type 0 = main, param 4 = float stereo (as PS5SX2 opens it) */
    handle = sceAudioOutOpen(255, 0, 0, GRAIN, 48000, 4);
    report("audio: sceAudioOutOpen -> %x", handle);
    if (handle < 0)
        return NULL;
    while (!atomic_load(&g_quit)) {
        app_audio_pull(g_app, pcm, GRAIN);
        for (int i = 0; i < GRAIN * 2; i++)
            out[i] = (float)pcm[i] * (1.0f / 32768.0f);
        sceAudioOutOutput(handle, out); /* blocks until the previous grain has played */
    }
    return NULL;
}

/* ------------------------------------------------------------------ vulkan */

#define INSTANCE_COMMANDS(X)                                                                                       \
    X(EnumeratePhysicalDevices) X(GetPhysicalDeviceProperties2) X(GetPhysicalDeviceQueueFamilyProperties)          \
    X(GetPhysicalDeviceMemoryProperties) X(CreateDevice) X(GetDeviceProcAddr) X(GetPhysicalDeviceDisplayPropertiesKHR) \
    X(GetDisplayModePropertiesKHR) X(CreateDisplayPlaneSurfaceKHR) X(GetPhysicalDeviceSurfaceSupportKHR)

#define DEVICE_COMMANDS(X)                                                                                         \
    X(GetDeviceQueue) X(QueueSubmit) X(DeviceWaitIdle) X(CreateFence) X(WaitForFences) X(ResetFences)              \
    X(CreateCommandPool) X(AllocateCommandBuffers) X(BeginCommandBuffer) X(EndCommandBuffer) X(ResetCommandBuffer) \
    X(CreateSwapchainKHR) X(GetSwapchainImagesKHR) X(AcquireNextImageKHR) X(QueuePresentKHR) X(CreateSemaphore)    \
    X(CreateImageView)

#define DECLARE(name) static PFN_vk##name vk##name;
INSTANCE_COMMANDS(DECLARE)
DEVICE_COMMANDS(DECLARE)
#undef DECLARE
static PFN_vkCreateInstance vkCreateInstance;
static PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties;

static bool
vk_start(struct vkr_ctx *ctx, VkSwapchainKHR *swapchain, VkImage *images, VkImageView *views, uint32_t *image_count,
         VkCommandBuffer *cmd, VkFence *fence, VkSemaphore *acquired, VkSemaphore *rendered)
{
    VkInstance instance;
    VkPhysicalDevice phys;
    VkDevice dev;
    VkQueue queue;
    VkResult r;
    uint32_t count, family = UINT32_MAX;

    vkCreateInstance = (PFN_vkCreateInstance)vk_icdGetInstanceProcAddr(NULL, "vkCreateInstance");
    vkEnumerateInstanceExtensionProperties =
        (PFN_vkEnumerateInstanceExtensionProperties)vk_icdGetInstanceProcAddr(NULL, "vkEnumerateInstanceExtensionProperties");
    if (!vkCreateInstance) {
        report("FAIL no vkCreateInstance");
        return false;
    }
    {
        VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "Lantern Miner", .apiVersion = VK_API_VERSION_1_4};
        char const *exts[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_DISPLAY_EXTENSION_NAME};
        VkInstanceCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app,
                                   .enabledExtensionCount = 2, .ppEnabledExtensionNames = exts};
        r = vkCreateInstance(&ci, NULL, &instance);
        report("%s vkCreateInstance (%d)", r == VK_SUCCESS ? "PASS" : "FAIL", r);
        if (r != VK_SUCCESS)
            return false;
    }
#define LOAD_I(name)                                                                                               \
    vk##name = (PFN_vk##name)vk_icdGetInstanceProcAddr(instance, "vk" #name);                                      \
    if (!vk##name) { report("FAIL missing vk" #name); return false; }
    INSTANCE_COMMANDS(LOAD_I)
#undef LOAD_I
    count = 1;
    r = vkEnumeratePhysicalDevices(instance, &count, &phys);
    if ((r != VK_SUCCESS && r != VK_INCOMPLETE) || count != 1) {
        report("FAIL physical device (%d, %u)", r, count);
        return false;
    }
    {
        VkPhysicalDeviceDriverProperties drv = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
        VkPhysicalDeviceProperties2 pr = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &drv};
        vkGetPhysicalDeviceProperties2(phys, &pr);
        report("device: %s, Vulkan %u.%u.%u, %s %s", pr.properties.deviceName, VK_API_VERSION_MAJOR(pr.properties.apiVersion),
               VK_API_VERSION_MINOR(pr.properties.apiVersion), VK_API_VERSION_PATCH(pr.properties.apiVersion), drv.driverName, drv.driverInfo);
    }
    {
        VkQueueFamilyProperties fam[8];
        count = 8;
        vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, fam);
        for (uint32_t i = 0; i < count; i++)
            if ((fam[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT)) == (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT)) {
                family = i;
                break;
            }
        if (family == UINT32_MAX) {
            report("FAIL no graphics queue");
            return false;
        }
    }
    vkGetPhysicalDeviceMemoryProperties(phys, &ctx->mem);
    {
        float prio = 1.0f;
        VkDeviceQueueCreateInfo qi = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = family, .queueCount = 1, .pQueuePriorities = &prio};
        VkPhysicalDeviceVulkan13Features f13 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, .synchronization2 = VK_TRUE, .dynamicRendering = VK_TRUE};
        char const *dexts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        VkDeviceCreateInfo di = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .pNext = &f13, .enabledExtensionCount = 1,
                                 .ppEnabledExtensionNames = dexts, .queueCreateInfoCount = 1, .pQueueCreateInfos = &qi};
        r = vkCreateDevice(phys, &di, NULL, &dev);
        report("%s vkCreateDevice (%d)", r == VK_SUCCESS ? "PASS" : "FAIL", r);
        if (r != VK_SUCCESS)
            return false;
    }
#define LOAD_D(name)                                                                                               \
    vk##name = (PFN_vk##name)vkGetDeviceProcAddr(dev, "vk" #name);                                                 \
    if (!vk##name) { report("FAIL missing vk" #name); return false; }
    DEVICE_COMMANDS(LOAD_D)
#undef LOAD_D
    vkGetDeviceQueue(dev, family, 0, &queue);

    /* display + swapchain */
    {
        VkDisplayPropertiesKHR display;
        VkDisplayModePropertiesKHR modes[2];
        VkSurfaceKHR surface;
        VkBool32 supported = VK_FALSE;
        count = 1;
        r = vkGetPhysicalDeviceDisplayPropertiesKHR(phys, &count, &display);
        if ((r != VK_SUCCESS && r != VK_INCOMPLETE) || count < 1) {
            report("FAIL display properties (%d)", r);
            return false;
        }
        count = 2;
        r = vkGetDisplayModePropertiesKHR(phys, display.display, &count, modes);
        if (r < VK_SUCCESS || count == 0) {
            report("FAIL display modes (%d)", r);
            return false;
        }
        report("display %ux%u, mode 0 at %.3f Hz", display.physicalResolution.width, display.physicalResolution.height,
               modes[0].parameters.refreshRate / 1000.0);
        VkDisplaySurfaceCreateInfoKHR si = {.sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR, .displayMode = modes[0].displayMode,
            .transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR, .alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR, .imageExtent = {OUT_W, OUT_H}};
        r = vkCreateDisplayPlaneSurfaceKHR(instance, &si, NULL, &surface);
        if (r != VK_SUCCESS) {
            report("FAIL display surface (%d)", r);
            return false;
        }
        vkGetPhysicalDeviceSurfaceSupportKHR(phys, family, surface, &supported);
        VkSwapchainCreateInfoKHR sc = {.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, .surface = surface, .minImageCount = 3,
            .imageFormat = VK_FORMAT_B8G8R8A8_UNORM, .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, .imageExtent = {OUT_W, OUT_H},
            .imageArrayLayers = 1, .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE, .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, .presentMode = VK_PRESENT_MODE_FIFO_KHR, .clipped = VK_TRUE};
        r = vkCreateSwapchainKHR(dev, &sc, NULL, swapchain);
        if (r != VK_SUCCESS) { /* retry with colour-attachment usage only */
            sc.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            r = vkCreateSwapchainKHR(dev, &sc, NULL, swapchain);
        }
        report("%s vkCreateSwapchainKHR (%d), surface support %u", r == VK_SUCCESS ? "PASS" : "FAIL", r, supported);
        if (r != VK_SUCCESS)
            return false;
        *image_count = 8;
        vkGetSwapchainImagesKHR(dev, *swapchain, image_count, images);
        report("swapchain images: %u", *image_count);
        for (uint32_t i = 0; i < *image_count; i++) {
            VkImageViewCreateInfo vi = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = images[i], .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = VK_FORMAT_B8G8R8A8_UNORM, .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
            r = vkCreateImageView(dev, &vi, NULL, &views[i]);
            if (r != VK_SUCCESS) {
                report("FAIL swapchain image view (%d)", r);
                return false;
            }
        }
    }
    /* per-frame objects */
    {
        VkCommandPool pool;
        VkCommandPoolCreateInfo pi = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, .queueFamilyIndex = family};
        VkFenceCreateInfo fi = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        VkSemaphoreCreateInfo semi = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkCommandBufferAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1};
        if (vkCreateCommandPool(dev, &pi, NULL, &pool) != VK_SUCCESS) return false;
        ai.commandPool = pool;
        if (vkAllocateCommandBuffers(dev, &ai, cmd) != VK_SUCCESS || vkCreateFence(dev, &fi, NULL, fence) != VK_SUCCESS ||
            vkCreateSemaphore(dev, &semi, NULL, acquired) != VK_SUCCESS || vkCreateSemaphore(dev, &semi, NULL, rendered) != VK_SUCCESS)
            return false;
    }

    ctx->instance = instance;
    ctx->phys = phys;
    ctx->dev = dev;
    ctx->queue = queue;
    ctx->family = family;
    ctx->get_device_proc = vkGetDeviceProcAddr;
    ctx->color_format = VK_FORMAT_B8G8R8A8_UNORM;
    ctx->width = OUT_W;
    ctx->height = OUT_H;
    ctx->scale = (float)OUT_H / 1080.0f;
    ctx->final_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    return true;
}

/* -------------------------------------------------------------------- main */

static bool
dir_writable(char const *dir)
{
    char probe[300];
    FILE *f;
    mkdir(dir, 0755);
    snprintf(probe, sizeof(probe), "%s/.probe", dir);
    f = fopen(probe, "w");
    if (!f)
        return false;
    fclose(f);
    remove(probe);
    return true;
}

int
main(void)
{
    struct vkr_ctx ctx;
    struct vkr *vkr;
    struct app_env env;
    VkSwapchainKHR swapchain;
    VkImage images[8];
    VkImageView views[8];
    uint32_t image_count = 0;
    VkCommandBuffer cmd;
    VkFence fence;
    VkSemaphore acquired, rendered;
    pthread_t audio;
    bool audio_started = false;
    char const *data_dir;
    char logpath[300];
    data_dir = dir_writable(DATA_DIR) ? DATA_DIR : FALLBACK_DIR;

    ps5_klog_capture_stderr("[LanternMiner:stderr] ");
    mkdir(data_dir, 0755);
    snprintf(logpath, sizeof(logpath), "%s/logs", data_dir);
    mkdir(logpath, 0755);
    snprintf(logpath, sizeof(logpath), "%s/logs/lantern-miner.txt", data_dir);
    g_log = fopen(logpath, "w");
    report("Lantern Miner starts; data dir %s", data_dir);

    memset(&ctx, 0, sizeof(ctx));
    ctx.font_path[VKR_DISPLAY] = "/app0/assets/fonts/Fraunces-Bold.ttf";
    ctx.font_path[VKR_DISPLAY_SEMI] = "/app0/assets/fonts/Fraunces-SemiBold.ttf";
    ctx.font_path[VKR_SANS] = "/app0/assets/fonts/DMSans-Medium.ttf";
    ctx.font_path[VKR_SANS_BOLD] = "/app0/assets/fonts/DMSans-Bold.ttf";
    ctx.font_path[VKR_MONO] = "/app0/assets/fonts/JetBrainsMono-Medium.ttf";
    ctx.font_path[VKR_MONO_BOLD] = "/app0/assets/fonts/JetBrainsMono-Bold.ttf";
    ctx.log = report;
    if (!vk_start(&ctx, &swapchain, images, views, &image_count, &cmd, &fence, &acquired, &rendered)) {
        report("ABORT: Vulkan failed");
        return 1;
    }
    vkr = vkr_create(&ctx);
    if (!vkr) {
        report("ABORT: renderer failed");
        return 1;
    }

    memset(&env, 0, sizeof(env));
    snprintf(env.data_dir, sizeof(env.data_dir), "%s", data_dir);
    snprintf(env.asset_dir, sizeof(env.asset_dir), "/app0/assets");
    env.log = report;
    g_app = app_create(&env);

    pad_init();
    audio_started = pthread_create(&audio, NULL, audio_thread, NULL) == 0;

    {
        double t_start = now_s(), t_report = t_start, worst = 0, last = t_start;
        unsigned frames = 0;
        struct app_input in;
        memset(&in, 0, sizeof(in));
        report("main loop");
        while (!app_wants_exit(g_app)) {
            double t0 = now_s();
            uint32_t index;
            VkResult r;
            pad_poll(&in, t0);
            app_frame(g_app, &in, t0, vkr);
            {
                static uint8_t last_large, last_small;
                static bool was_connected = true;
                static unsigned logged;
                uint8_t large = 0, small = 0;
                if (in.connected && !was_connected) {
                    pad_rumble_mode(); /* a reconnected controller starts over in haptics mode */
                    last_large = last_small = 0;
                }
                was_connected = in.connected;
                app_rumble(g_app, &large, &small);
                if ((large != last_large || small != last_small) && g_pad.handle >= 0) {
                    struct pad_vibration v = {large, small};
                    int rc = scePadSetVibration(g_pad.handle, &v);
                    if (logged < 8) {
                        logged++;
                        report("pad: rumble %u/%u -> %x", large, small, rc);
                    }
                    last_large = large;
                    last_small = small;
                }
            }

            r = vkAcquireNextImageKHR(ctx.dev, swapchain, UINT64_MAX, acquired, VK_NULL_HANDLE, &index);
            if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
                report("FAIL acquire (%d)", r);
                break;
            }
            {
                VkCommandBufferBeginInfo bi = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
                VkPipelineStageFlags wait = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
                VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .waitSemaphoreCount = 1, .pWaitSemaphores = &acquired,
                    .pWaitDstStageMask = &wait, .commandBufferCount = 1, .pCommandBuffers = &cmd, .signalSemaphoreCount = 1, .pSignalSemaphores = &rendered};
                VkPresentInfoKHR pi = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, .waitSemaphoreCount = 1, .pWaitSemaphores = &rendered,
                    .swapchainCount = 1, .pSwapchains = &swapchain, .pImageIndices = &index};
                vkResetCommandBuffer(cmd, 0);
                vkBeginCommandBuffer(cmd, &bi);
                vkr_end_frame(vkr, cmd, images[index], views[index]);
                vkEndCommandBuffer(cmd);
                vkResetFences(ctx.dev, 1, &fence);
                r = vkQueueSubmit(ctx.queue, 1, &si, fence);
                if (r != VK_SUCCESS) {
                    report("FAIL submit (%d)", r);
                    break;
                }
                r = vkQueuePresentKHR(ctx.queue, &pi);
                vkWaitForFences(ctx.dev, 1, &fence, VK_TRUE, UINT64_MAX);
                if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
                    report("FAIL present (%d)", r);
                    break;
                }
            }
            {
                /* The console keeps the title's pic1 launch background up until the app hides it. */
                static bool splash_hidden;
                if (!splash_hidden) {
                    splash_hidden = true;
                    report("splash hide -> %d", sceSystemServiceHideSplashScreen());
                }
            }
            {
                double t1 = now_s();
                if (t1 - last > worst)
                    worst = t1 - last;
                last = t1;
                frames++;
                if (t1 - t_report >= 10.0) {
                    report("t=%.0fs  %.1f fps  worst %.1f ms", t1 - t_start, frames / (t1 - t_report), worst * 1000.0);
                    t_report = t1;
                    frames = 0;
                    worst = 0;
                }
            }
        }
    }
    /* Deterministic shutdown: stop the game and write its save, stop our threads, then leave without running exit-time
     * destructors (the Vulkan driver's threads and static state are not safe to tear down while the process exits). */
    report("exiting");
    if (g_pad.handle >= 0) {
        struct pad_vibration off = {0, 0}; /* never leave the controller buzzing */
        scePadSetVibration(g_pad.handle, &off);
    }
    app_shutdown(g_app);
    atomic_store(&g_quit, true);
    if (audio_started)
        pthread_join(audio, NULL);
    vkDeviceWaitIdle(ctx.dev);
    report("bye");
    pthread_mutex_lock(&g_log_lock);
    atomic_store(&g_logging, false);
    if (g_log) {
        fflush(g_log);
        fclose(g_log);
        g_log = NULL;
    }
    pthread_mutex_unlock(&g_log_lock);
    /* Ask the system to close us (a plain _exit ends in SIGSYS inside libkernel and shows a crash dialog). */
    sceSystemServiceLoadExec("exit", NULL);
    for (int i = 0; i < 100; i++)
        usleep(100 * 1000);
    _exit(0);
}
