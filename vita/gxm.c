// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The gxm suite (docs/families/Gxm.md): the Vita's GPU through sceGxm, brought up as the SDK's
// samples do, with vita2d's clear and texture programs (fetched by vita.mk at a fixed commit).
// Every case works out what the pixels must be from what it drew and compares them exactly: each
// edge on a pixel's boundary and each texel at a pixel's centre, which every correct rasteriser
// and sampler places the same, at any scale a machine draws at inside.

#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/kernel/sysmem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gxm.h"
#include "report.h"

static constexpr char SUITE[] = "gxm";
static constexpr uint32_t TARGET_SIDE = 64; // the render target drawn into, square
static constexpr uint32_t DISPLAY_W = 960;
static constexpr uint32_t DISPLAY_H = 544;
static constexpr uint32_t DISPLAY_STRIDE = 960;

extern const SceGxmProgram clear_v_gxp_start;
extern const SceGxmProgram clear_f_gxp_start;
extern const SceGxmProgram texture_v_gxp_start;
extern const SceGxmProgram texture_f_gxp_start;

typedef struct {
    float x, y;
} ClearVertex;
typedef struct {
    float x, y, z, u, v;
} TextureVertex;

static struct {
    SceGxmContext *context;
    SceGxmShaderPatcher *patcher;
    SceGxmShaderPatcherId clear_v, clear_f, texture_v, texture_f;
    SceGxmVertexProgram *clear_vp, *texture_vp;
    SceGxmFragmentProgram *clear_fp, *texture_fp;
    const SceGxmProgramParameter *clear_colour, *wvp;
    SceGxmRenderTarget *target_rt, *display_rt;
    SceGxmColorSurface target_surface, display_surface;
    uint32_t *target_pixels, *display_pixels;
    ClearVertex *clear_vertices;
    TextureVertex *texture_vertices;
    uint16_t *indices;
    char why[160]; // why the bring-up stopped, for every case's detail
    bool up;
} g_gxm;

/// A block of the GPU's memory, mapped for it with @p attribs, or for the USSE (vertex or
/// fragment, @p usse 1 or 2) at the offset it writes. nullptr, with why, when the system gives none.
static void *GpuAlloc(SceKernelMemBlockType type, uint32_t size, int usse, SceGxmMemoryAttribFlags attribs,
                      unsigned *offset) {
    const uint32_t align = type == SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW ? 256u * 1024u : 4096u;
    size = (size + align - 1u) & ~(align - 1u);
    const SceUID uid = sceKernelAllocMemBlock("amberlith gxm", type, size, nullptr);
    void *base = nullptr;
    if (uid < 0 || sceKernelGetMemBlockBase(uid, &base) < 0) {
        snprintf(g_gxm.why, sizeof(g_gxm.why), "sceKernelAllocMemBlock gave no %u bytes (0x%08x)",
                 (unsigned)size, (unsigned)uid);
        return nullptr;
    }
    int r = usse == 1 ? sceGxmMapVertexUsseMemory(base, size, offset)
            : usse == 2 ? sceGxmMapFragmentUsseMemory(base, size, offset)
                        : sceGxmMapMemory(base, size, attribs);
    if (r < 0) {
        snprintf(g_gxm.why, sizeof(g_gxm.why), "sceGxm refused to map %u bytes (0x%08x)", (unsigned)size,
                 (unsigned)r);
        return nullptr;
    }
    return base;
}

static void *PatcherAlloc([[maybe_unused]] void *user, SceSize size) {
    return malloc(size);
}
static void PatcherFree([[maybe_unused]] void *user, void *memory) {
    free(memory);
}
static void DisplayQueue([[maybe_unused]] const void *data) {}

/// One step of the bring-up: true when @p result is not an error, else the call's name and code
/// are why every case of the suite stops.
static bool Ok(int result, const char *call) {
    if (result >= 0) {
        return true;
    }
    snprintf(g_gxm.why, sizeof(g_gxm.why), "%s answered 0x%08x", call, (unsigned)result);
    return false;
}

static bool Surface(SceGxmColorSurface *surface, SceGxmRenderTarget **target, uint32_t **pixels,
                    uint32_t width, uint32_t height, uint32_t stride) {
    *pixels = GpuAlloc(SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, stride * height * 4u, 0,
                       SCE_GXM_MEMORY_ATTRIB_RW, nullptr);
    if (*pixels == nullptr) {
        return false;
    }
    SceGxmRenderTargetParams params;
    memset(&params, 0, sizeof(params));
    params.width = width;
    params.height = height;
    params.scenesPerFrame = 1;
    params.multisampleMode = SCE_GXM_MULTISAMPLE_NONE;
    params.driverMemBlock = -1;
    return Ok(sceGxmColorSurfaceInit(surface, SCE_GXM_COLOR_FORMAT_A8B8G8R8, SCE_GXM_COLOR_SURFACE_LINEAR,
                                     SCE_GXM_COLOR_SURFACE_SCALE_NONE, SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT, width,
                                     height, stride, *pixels),
              "sceGxmColorSurfaceInit") &&
           Ok(sceGxmCreateRenderTarget(&params, target), "sceGxmCreateRenderTarget");
}

static bool Program(const SceGxmProgram *program, SceGxmShaderPatcherId *id) {
    return Ok(sceGxmProgramCheck(program), "sceGxmProgramCheck") &&
           Ok(sceGxmShaderPatcherRegisterProgram(g_gxm.patcher, program, id),
              "sceGxmShaderPatcherRegisterProgram");
}

static bool BringUp(void) {
    SceGxmInitializeParams init;
    memset(&init, 0, sizeof(init));
    init.displayQueueMaxPendingCount = 1;
    init.displayQueueCallback = DisplayQueue;
    init.displayQueueCallbackDataSize = 4;
    init.parameterBufferSize = SCE_GXM_DEFAULT_PARAMETER_BUFFER_SIZE;
    if (!Ok(sceGxmInitialize(&init), "sceGxmInitialize")) {
        return false;
    }
    unsigned fragment_usse_offset = 0;
    SceGxmContextParams context;
    memset(&context, 0, sizeof(context));
    context.hostMem = malloc(SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE);
    context.hostMemSize = SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE;
    context.vdmRingBufferMem = GpuAlloc(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
                                        SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE, 0, SCE_GXM_MEMORY_ATTRIB_READ, nullptr);
    context.vdmRingBufferMemSize = SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE;
    context.vertexRingBufferMem = GpuAlloc(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
                                           SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE, 0,
                                           SCE_GXM_MEMORY_ATTRIB_READ, nullptr);
    context.vertexRingBufferMemSize = SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE;
    context.fragmentRingBufferMem = GpuAlloc(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
                                             SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE, 0,
                                             SCE_GXM_MEMORY_ATTRIB_READ, nullptr);
    context.fragmentRingBufferMemSize = SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE;
    context.fragmentUsseRingBufferMem = GpuAlloc(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
                                                 SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE, 2, 0,
                                                 &fragment_usse_offset);
    context.fragmentUsseRingBufferMemSize = SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE;
    context.fragmentUsseRingBufferOffset = fragment_usse_offset;
    if (context.hostMem == nullptr || context.vdmRingBufferMem == nullptr || context.vertexRingBufferMem == nullptr ||
        context.fragmentRingBufferMem == nullptr || context.fragmentUsseRingBufferMem == nullptr ||
        !Ok(sceGxmCreateContext(&context, &g_gxm.context), "sceGxmCreateContext")) {
        return false;
    }
    // The shader patcher, with its buffers for the programs it patches
    constexpr uint32_t PATCHER_BUFFER = 64u * 1024u;
    constexpr uint32_t PATCHER_USSE = 64u * 1024u;
    unsigned vertex_usse_offset = 0, fragment_patcher_offset = 0;
    SceGxmShaderPatcherParams patcher;
    memset(&patcher, 0, sizeof(patcher));
    patcher.hostAllocCallback = PatcherAlloc;
    patcher.hostFreeCallback = PatcherFree;
    patcher.bufferMem = GpuAlloc(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, PATCHER_BUFFER, 0,
                                 SCE_GXM_MEMORY_ATTRIB_RW, nullptr);
    patcher.bufferMemSize = PATCHER_BUFFER;
    patcher.vertexUsseMem = GpuAlloc(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, PATCHER_USSE, 1, 0,
                                     &vertex_usse_offset);
    patcher.vertexUsseMemSize = PATCHER_USSE;
    patcher.vertexUsseOffset = vertex_usse_offset;
    patcher.fragmentUsseMem = GpuAlloc(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, PATCHER_USSE, 2, 0,
                                       &fragment_patcher_offset);
    patcher.fragmentUsseMemSize = PATCHER_USSE;
    patcher.fragmentUsseOffset = fragment_patcher_offset;
    if (patcher.bufferMem == nullptr || patcher.vertexUsseMem == nullptr || patcher.fragmentUsseMem == nullptr ||
        !Ok(sceGxmShaderPatcherCreate(&patcher, &g_gxm.patcher), "sceGxmShaderPatcherCreate") ||
        !Program(&clear_v_gxp_start, &g_gxm.clear_v) || !Program(&clear_f_gxp_start, &g_gxm.clear_f) ||
        !Program(&texture_v_gxp_start, &g_gxm.texture_v) || !Program(&texture_f_gxp_start, &g_gxm.texture_f)) {
        return false;
    }
    // The clear program: a position of two floats; the texture program: three, then two
    const SceGxmProgramParameter *position = sceGxmProgramFindParameterByName(&clear_v_gxp_start, "aPosition");
    const SceGxmProgramParameter *texture_position =
        sceGxmProgramFindParameterByName(&texture_v_gxp_start, "aPosition");
    const SceGxmProgramParameter *texcoord = sceGxmProgramFindParameterByName(&texture_v_gxp_start, "aTexcoord");
    g_gxm.clear_colour = sceGxmProgramFindParameterByName(&clear_f_gxp_start, "uClearColor");
    g_gxm.wvp = sceGxmProgramFindParameterByName(&texture_v_gxp_start, "wvp");
    if (position == nullptr || texture_position == nullptr || texcoord == nullptr || g_gxm.clear_colour == nullptr ||
        g_gxm.wvp == nullptr) {
        snprintf(g_gxm.why, sizeof(g_gxm.why), "a vita2d program lacks a parameter this suite sets");
        return false;
    }
    SceGxmVertexAttribute clear_attribute = { 0, 0, SCE_GXM_ATTRIBUTE_FORMAT_F32, 2,
                                              (unsigned short)sceGxmProgramParameterGetResourceIndex(position) };
    SceGxmVertexStream clear_stream = { sizeof(ClearVertex), SCE_GXM_INDEX_SOURCE_INDEX_16BIT };
    SceGxmVertexAttribute texture_attributes[2] = {
        { 0, 0, SCE_GXM_ATTRIBUTE_FORMAT_F32, 3,
          (unsigned short)sceGxmProgramParameterGetResourceIndex(texture_position) },
        { 0, 12, SCE_GXM_ATTRIBUTE_FORMAT_F32, 2, (unsigned short)sceGxmProgramParameterGetResourceIndex(texcoord) },
    };
    SceGxmVertexStream texture_stream = { sizeof(TextureVertex), SCE_GXM_INDEX_SOURCE_INDEX_16BIT };
    if (!Ok(sceGxmShaderPatcherCreateVertexProgram(g_gxm.patcher, g_gxm.clear_v, &clear_attribute, 1,
                                                   &clear_stream, 1, &g_gxm.clear_vp),
            "sceGxmShaderPatcherCreateVertexProgram") ||
        !Ok(sceGxmShaderPatcherCreateFragmentProgram(g_gxm.patcher, g_gxm.clear_f,
                                                     SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
                                                     SCE_GXM_MULTISAMPLE_NONE, nullptr, &clear_v_gxp_start,
                                                     &g_gxm.clear_fp),
            "sceGxmShaderPatcherCreateFragmentProgram") ||
        !Ok(sceGxmShaderPatcherCreateVertexProgram(g_gxm.patcher, g_gxm.texture_v, texture_attributes, 2,
                                                   &texture_stream, 1, &g_gxm.texture_vp),
            "sceGxmShaderPatcherCreateVertexProgram") ||
        !Ok(sceGxmShaderPatcherCreateFragmentProgram(g_gxm.patcher, g_gxm.texture_f,
                                                     SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
                                                     SCE_GXM_MULTISAMPLE_NONE, nullptr, &texture_v_gxp_start,
                                                     &g_gxm.texture_fp),
            "sceGxmShaderPatcherCreateFragmentProgram")) {
        return false;
    }
    // The vertices and indices, in memory the GPU reads
    g_gxm.clear_vertices = GpuAlloc(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, 4096u, 0, SCE_GXM_MEMORY_ATTRIB_READ,
                                    nullptr);
    g_gxm.texture_vertices = GpuAlloc(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, 4096u, 0,
                                      SCE_GXM_MEMORY_ATTRIB_READ, nullptr);
    g_gxm.indices = GpuAlloc(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, 4096u, 0, SCE_GXM_MEMORY_ATTRIB_READ, nullptr);
    if (g_gxm.clear_vertices == nullptr || g_gxm.texture_vertices == nullptr || g_gxm.indices == nullptr) {
        return false;
    }
    for (uint16_t i = 0; i < 4u; ++i) {
        g_gxm.indices[i] = i;
    }
    return Surface(&g_gxm.target_surface, &g_gxm.target_rt, &g_gxm.target_pixels, TARGET_SIDE, TARGET_SIDE,
                   TARGET_SIDE) &&
           Surface(&g_gxm.display_surface, &g_gxm.display_rt, &g_gxm.display_pixels, DISPLAY_W, DISPLAY_H,
                   DISPLAY_STRIDE);
}

/// A rectangle of one colour, its corners in pixels of a surface @p width by @p height: as clip
/// space for the clear program, whose position is that alone.
static void Rectangle(uint32_t width, uint32_t height, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1,
                      uint32_t abgr, int slot) {
    ClearVertex *v = g_gxm.clear_vertices + slot * 4;
    const float l = -1.0f + 2.0f * (float)x0 / (float)width, r = -1.0f + 2.0f * (float)x1 / (float)width;
    const float t = 1.0f - 2.0f * (float)y0 / (float)height, b = 1.0f - 2.0f * (float)y1 / (float)height;
    v[0] = (ClearVertex){ l, t };
    v[1] = (ClearVertex){ r, t };
    v[2] = (ClearVertex){ l, b };
    v[3] = (ClearVertex){ r, b };
    const float colour[4] = { (float)(abgr & 0xffu) / 255.0f, (float)(abgr >> 8 & 0xffu) / 255.0f,
                              (float)(abgr >> 16 & 0xffu) / 255.0f, (float)(abgr >> 24) / 255.0f };
    sceGxmSetVertexProgram(g_gxm.context, g_gxm.clear_vp);
    sceGxmSetFragmentProgram(g_gxm.context, g_gxm.clear_fp);
    void *uniforms = nullptr;
    sceGxmReserveFragmentDefaultUniformBuffer(g_gxm.context, &uniforms);
    sceGxmSetUniformDataF(uniforms, g_gxm.clear_colour, 0, 4, colour);
    sceGxmSetVertexStream(g_gxm.context, 0, v);
    sceGxmDraw(g_gxm.context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, g_gxm.indices, 4);
}

// The render target's picture: a background, and a square inside it whose edges lie on pixels'
// boundaries, with a corner of another colour: the pixels each must hold.
static constexpr uint32_t BACKGROUND = 0xff402010u;
static constexpr uint32_t SQUARE = 0xff10c080u;
static constexpr uint32_t CORNER = 0x80ff00ffu;
static uint32_t Expected(uint32_t x, uint32_t y) {
    if (x >= 16u && x < 24u && y >= 16u && y < 24u) {
        return CORNER;
    }
    if (x >= 16u && x < 48u && y >= 16u && y < 40u) {
        return SQUARE;
    }
    return BACKGROUND;
}

/// A scene on @p target: clip space over the whole of it, y up, as the SDK's samples set it.
static void Begin(SceGxmRenderTarget *target, SceGxmColorSurface *surface, uint32_t width, uint32_t height) {
    sceGxmBeginScene(g_gxm.context, 0, target, nullptr, nullptr, nullptr, surface, nullptr);
    sceGxmSetViewport(g_gxm.context, (float)width / 2.0f, (float)width / 2.0f, (float)height / 2.0f,
                      -(float)height / 2.0f, 0.5f, 0.5f);
}

static void DrawTarget(void) {
    Begin(g_gxm.target_rt, &g_gxm.target_surface, TARGET_SIDE, TARGET_SIDE);
    Rectangle(TARGET_SIDE, TARGET_SIDE, 0, 0, TARGET_SIDE, TARGET_SIDE, BACKGROUND, 0);
    Rectangle(TARGET_SIDE, TARGET_SIDE, 16, 16, 48, 40, SQUARE, 1);
    Rectangle(TARGET_SIDE, TARGET_SIDE, 16, 16, 24, 24, CORNER, 2);
    sceGxmEndScene(g_gxm.context, nullptr, nullptr);
    sceGxmFinish(g_gxm.context);
}

/// The pixels of a surface that are not the expected ones, the first named in @p detail.
static uint32_t Wrong(const uint32_t *pixels, uint32_t stride, uint32_t x0, uint32_t y0, uint32_t width,
                      uint32_t height, uint32_t (*expected)(uint32_t, uint32_t), char *detail, size_t size) {
    uint32_t wrong = 0;
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const uint32_t got = pixels[(y0 + y) * stride + x0 + x], want = expected(x, y);
            if (got != want && wrong++ == 0u) {
                snprintf(detail, size, "pixel (%u, %u) is 0x%08x, not 0x%08x", (unsigned)x, (unsigned)y,
                         (unsigned)got, (unsigned)want);
            }
        }
    }
    if (wrong > 1u) {
        const size_t used = strlen(detail);
        snprintf(detail + used, size - used, ", and %u more pixels are wrong", (unsigned)(wrong - 1u));
    }
    return wrong;
}

static uint32_t DisplayExpected(uint32_t x, uint32_t y) {
    return x < TARGET_SIDE && y < TARGET_SIDE ? Expected(x, y) : 0xff000000u;
}

/// The render target, as a texture of its own pixels sampled at their centres, onto the display's
/// buffer at its top left corner, the same size: a copy, texel for pixel. The rest is black.
static void DrawDisplay(void) {
    SceGxmTexture texture;
    sceGxmTextureInitLinear(&texture, g_gxm.target_pixels, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR, TARGET_SIDE,
                            TARGET_SIDE, 0);
    sceGxmTextureSetMinFilter(&texture, SCE_GXM_TEXTURE_FILTER_POINT);
    sceGxmTextureSetMagFilter(&texture, SCE_GXM_TEXTURE_FILTER_POINT);
    const float l = -1.0f, r = -1.0f + 2.0f * (float)TARGET_SIDE / (float)DISPLAY_W;
    const float t = 1.0f, b = 1.0f - 2.0f * (float)TARGET_SIDE / (float)DISPLAY_H;
    TextureVertex *v = g_gxm.texture_vertices;
    v[0] = (TextureVertex){ l, t, 0.5f, 0.0f, 0.0f };
    v[1] = (TextureVertex){ r, t, 0.5f, 1.0f, 0.0f };
    v[2] = (TextureVertex){ l, b, 0.5f, 0.0f, 1.0f };
    v[3] = (TextureVertex){ r, b, 0.5f, 1.0f, 1.0f };
    static const float identity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    Begin(g_gxm.display_rt, &g_gxm.display_surface, DISPLAY_W, DISPLAY_H);
    Rectangle(DISPLAY_W, DISPLAY_H, 0, 0, DISPLAY_W, DISPLAY_H, 0xff000000u, 3);
    sceGxmSetVertexProgram(g_gxm.context, g_gxm.texture_vp);
    sceGxmSetFragmentProgram(g_gxm.context, g_gxm.texture_fp);
    void *uniforms = nullptr;
    sceGxmReserveVertexDefaultUniformBuffer(g_gxm.context, &uniforms);
    sceGxmSetUniformDataF(uniforms, g_gxm.wvp, 0, 16, identity);
    sceGxmSetFragmentTexture(g_gxm.context, 0, &texture);
    sceGxmSetVertexStream(g_gxm.context, 0, v);
    sceGxmDraw(g_gxm.context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, g_gxm.indices, 4);
    sceGxmEndScene(g_gxm.context, nullptr, nullptr);
    sceGxmFinish(g_gxm.context);
}

static void Stopped(const char *name) {
    if (ReportStart(SUITE, name)) {
        ReportCase(SUITE, name, CHECK_NONE, false, g_gxm.why);
    }
}

void RunGxm(void) {
    static const char *const NAMES[] = {
        "a render target drawn into reads back its background, square and corner",
        "the render target sampled as a texture copies onto the display's buffer texel for pixel",
        "the display's buffer shown reads back the copy and its black around it",
    };
    if (!g_gxm.up && g_gxm.why[0] == '\0') {
        g_gxm.up = BringUp();
    }
    if (!g_gxm.up) {
        for (size_t i = 0; i < sizeof(NAMES) / sizeof(NAMES[0]); ++i) {
            Stopped(NAMES[i]);
        }
        return;
    }
    char detail[192] = "";
    if (ReportStart(SUITE, NAMES[0])) {
        DrawTarget();
        const uint32_t wrong = Wrong(g_gxm.target_pixels, TARGET_SIDE, 0, 0, TARGET_SIDE, TARGET_SIDE, Expected,
                                     detail, sizeof(detail));
        ReportCase(SUITE, NAMES[0], CHECK_EXACT, wrong == 0u, detail);
    }
    detail[0] = '\0';
    if (ReportStart(SUITE, NAMES[1])) {
        DrawTarget();
        DrawDisplay();
        const uint32_t wrong = Wrong(g_gxm.display_pixels, DISPLAY_STRIDE, 0, 0, DISPLAY_W, DISPLAY_H,
                                     DisplayExpected, detail, sizeof(detail));
        ReportCase(SUITE, NAMES[1], CHECK_EXACT, wrong == 0u, detail);
    }
    detail[0] = '\0';
    if (ReportStart(SUITE, NAMES[2])) {
        SceDisplayFrameBuf frame;
        memset(&frame, 0, sizeof(frame));
        frame.size = sizeof(frame);
        frame.base = g_gxm.display_pixels;
        frame.pitch = DISPLAY_STRIDE;
        frame.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
        frame.width = DISPLAY_W;
        frame.height = DISPLAY_H;
        const int shown = sceDisplaySetFrameBuf(&frame, SCE_DISPLAY_SETBUF_NEXTFRAME);
        // On the screen for a second, as a person would see it, and as long for a capture of it
        for (int vblank = 0; vblank < 60; ++vblank) {
            sceDisplayWaitVblankStart();
        }
        uint32_t wrong = 0;
        if (shown < 0) {
            snprintf(detail, sizeof(detail), "sceDisplaySetFrameBuf answered 0x%08x", (unsigned)shown);
        } else {
            wrong = Wrong(g_gxm.display_pixels, DISPLAY_STRIDE, 0, 0, DISPLAY_W, DISPLAY_H, DisplayExpected,
                          detail, sizeof(detail));
        }
        ReportCase(SUITE, NAMES[2], CHECK_EXACT, shown >= 0 && wrong == 0u, detail);
    }
}
