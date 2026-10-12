/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ODIN_RENDER_SCALE_H
#define ODIN_RENDER_SCALE_H
#include <stdint.h>
#include <string.h>

/* Odin Sphere Leifthrasir CUSA05083 v01.01: render above 1920x1080 (docs/odin/RE_NOTES.md).
 * The game sizes its render targets and full-screen viewports with 1920/1080 immediates; views
 * (projection built from the screen config's 1920x1080 floats, untouched) set their viewport
 * from float rects at view+0x284..0x290 in FUN_006b5f80. Scaling the targets, those viewports and
 * the view rects by the same factor rasterizes the unchanged 2D layout at more pixels. */

/* imm32 operands holding 1920 (width) and 1080 (height): the main render target inits
 * (0x6ac138, 0x6ac269: MAIN BUFFER color/depth), the three USER OFFSCREEN BUFFERs (calls to
 * 0x6adaa0 at 0x6ac440/59/72) and
 * the full-screen viewports (calls to 0x744910 at 0x6acd62, 0x6ae731). */
static const uint32_t odin_scale_width_imm[] = {0x6ac139, 0x6ac26a, 0x6ac429, 0x6ac446, 0x6ac45f,
                                                0x6acd51, 0x6ae723};
static const uint32_t odin_scale_height_imm[] = {0x6ac13e, 0x6ac26f, 0x6ac42e, 0x6ac44b, 0x6ac464,
                                                 0x6acd57, 0x6ae729};

/* float imm32s of 1920.0/1080.0: the source rects (in texels) FUN_0064a1f0 draws its three
 * full-screen offscreen layers with through FUN_0069d7e0 (the book intro text, ...). */
static const uint32_t odin_scale_width_float[] = {0x64a3df, 0x64a454, 0x64a4c9};
static const uint32_t odin_scale_height_float[] = {0x64a409, 0x64a47e, 0x64a4f3};

/* imm32 of `mov dword [rsp+0xf8], 0x4000000` in FUN_006ab500: the 64 MiB "RENDERBUFFER" heap the
 * USER OFFSCREEN BUFFERs (FUN_006adaa0: three 1920x1080 color+depth pairs, the UI layer among
 * them) are allocated from. Scaled targets need it times scale^2, or the allocations fail. */
enum { ODIN_RENDERBUFFER_HEAP_IMM = 0x6ab61e, ODIN_RENDERBUFFER_HEAP_SIZE = 0x4000000 };

/* FUN_006b5f80: vcvttss2si rsi/rdx/rcx/r8, [r12+0x284/0x288/0x28c/0x290] before
 * call 0x744910 (set viewport). */
enum { ODIN_VIEWPORT_HOOK_OFFSET = 0x6b5fbb, ODIN_VIEWPORT_HOOK_LENGTH = 40 };
static const unsigned char odin_viewport_original[ODIN_VIEWPORT_HOOK_LENGTH] = {
    0xc4,0xc1,0xfa,0x2c,0xb4,0x24,0x84,0x02,0x00,0x00, 0xc4,0xc1,0xfa,0x2c,0x94,0x24,0x88,0x02,0x00,0x00,
    0xc4,0xc1,0xfa,0x2c,0x8c,0x24,0x8c,0x02,0x00,0x00, 0xc4,0x41,0xfa,0x2c,0x84,0x24,0x90,0x02,0x00,0x00,
};

/* FUN_006b4140(ctx, cmd, x, y, w, h): the clip-region scissor of FUN_006b34e0 (minimap, menus),
 * in 1920x1080 pixels; tail-jumps to the Gnm scissor setter 0x6810c0 with corners. Replaced
 * by the same with x, y, w, h scaled (rounded). */
enum { ODIN_SCISSOR_HOOK_OFFSET = 0x6b4140, ODIN_SCISSOR_HOOK_LENGTH = 30, ODIN_SCISSOR_TARGET = 0x6810c0 };
static const unsigned char odin_scissor_original[ODIN_SCISSOR_HOOK_LENGTH] = {
    0x48,0x85,0xf6,0x74,0x18,0x41,0x01,0xd0,0x41,0x01,0xc9,0x48,0x89,0xf7,0x89,0xd6,
    0x89,0xca,0x44,0x89,0xc1,0x45,0x89,0xc8,0xe9,0x63,0xcf,0xfc,0xff,0xc3,
};

static inline unsigned odin_scissor_build_stub(unsigned char *out, float scale, uintptr_t target) {
    static const unsigned char body[] = {
        0x48,0x85,0xf6, 0x74,0x5b,                     /* test rsi, rsi; jz done */
        0xc5,0xea,0x2a,0xd2, 0xc5,0xea,0x59,0x15,0x58,0,0,0, 0xc5,0xfa,0x2d,0xd2, /* edx *= scale */
        0xc5,0xea,0x2a,0xd1, 0xc5,0xea,0x59,0x15,0x48,0,0,0, 0xc5,0xfa,0x2d,0xca, /* ecx */
        0xc4,0xc1,0x6a,0x2a,0xd0, 0xc5,0xea,0x59,0x15,0x37,0,0,0, 0xc5,0x7a,0x2d,0xc2, /* r8d */
        0xc4,0xc1,0x6a,0x2a,0xd1, 0xc5,0xea,0x59,0x15,0x26,0,0,0, 0xc5,0x7a,0x2d,0xca, /* r9d */
        0x41,0x01,0xd0, 0x41,0x01,0xc9,                /* add r8d, edx; add r9d, ecx */
        0x48,0x89,0xf7, 0x89,0xd6, 0x89,0xca, 0x44,0x89,0xc1, 0x45,0x89,0xc8,
        0xff,0x25,0x01,0x00,0x00,0x00,                 /* jmp [rip+target] */
        0xc3,                                          /* done: ret */
    };
    memcpy(out, body, sizeof(body));
    memcpy(out + sizeof(body), &target, 8);     /* target at 0x61 */
    memcpy(out + sizeof(body) + 8, &scale, 4);  /* scale at 0x69 */
    return sizeof(body) + 12;
}

/* FUN_006b5900: the on-demand layer buffer (the book intro text, among others) is created at the
 * screen size from the 1920x1080 getters, then drawn through a view whose viewport is scaled.
 * `mov esi,[rbp-0x30]; mov edx,[rbp-0x2c]; mov rdi,r14; call 0x6adaa0` replaced by the same
 * with a 1920x1080 size replaced by the scaled one. */
enum { ODIN_LAYER_HOOK_OFFSET = 0x6b595a, ODIN_LAYER_HOOK_LENGTH = 14, ODIN_LAYER_CALLEE = 0x6adaa0 };
static const unsigned char odin_layer_original[ODIN_LAYER_HOOK_LENGTH] = {
    0x8b,0x75,0xd0, 0x8b,0x55,0xd4, 0x4c,0x89,0xf7, 0xe8,0x38,0x81,0xff,0xff,
};

static inline unsigned odin_layer_build_stub(unsigned char *out, uint32_t width, uint32_t height,
                                             uintptr_t callee, uintptr_t resume) {
    unsigned char body[] = {
        0x8b,0x75,0xd0, 0x8b,0x55,0xd4,                /* mov esi,[rbp-0x30]; mov edx,[rbp-0x2c] */
        0x81,0xfe,0x80,0x07,0x00,0x00,                 /* cmp esi, 1920 */
        0x75,0x12,                                     /* jne call */
        0x81,0xfa,0x38,0x04,0x00,0x00,                 /* cmp edx, 1080 */
        0x75,0x0a,                                     /* jne call */
        0xbe,0,0,0,0,                                  /* mov esi, width */
        0xba,0,0,0,0,                                  /* mov edx, height */
        0x4c,0x89,0xf7,                                /* call: mov rdi, r14 */
        0xff,0x15,0x06,0x00,0x00,0x00,                 /* call [rip+callee] */
        0xff,0x25,0x08,0x00,0x00,0x00,                 /* jmp [rip+resume] */
    };
    memcpy(body + 23, &width, 4);
    memcpy(body + 28, &height, 4);
    memcpy(out, body, sizeof(body));
    memcpy(out + sizeof(body), &callee, 8);
    memcpy(out + sizeof(body) + 8, &resume, 8);
    return sizeof(body) + 16;
}

/* FUN_006b6aa0 (sprite batch execution): the texture size constant the sprite shaders divide
 * their texel coordinates by, from the T# width/height fields. Game texel coordinates into
 * render targets are in 1920x1080 units, so a scaled target (exactly 1920s x 1080s) reports
 * 1920x1080; its T# keeps the full size. `and ebx,0x3fff; inc ebx; inc r15d` replaced. */
enum { ODIN_TEXSIZE_HOOK_OFFSET = 0x6b87cf, ODIN_TEXSIZE_HOOK_LENGTH = 11 };
static const unsigned char odin_texsize_original[ODIN_TEXSIZE_HOOK_LENGTH] = {
    0x81,0xe3,0xff,0x3f,0x00,0x00, 0xff,0xc3, 0x41,0xff,0xc7,
};

static inline unsigned odin_texsize_build_stub(unsigned char *out, uint32_t width, uint32_t height,
                                               uintptr_t resume) {
    unsigned char body[] = {
        0x81,0xe3,0xff,0x3f,0x00,0x00, 0xff,0xc3, 0x41,0xff,0xc7, /* the replaced code */
        0x81,0xfb,0,0,0,0,                             /* cmp ebx, width */
        0x75,0x14,                                     /* jne done */
        0x41,0x81,0xff,0,0,0,0,                        /* cmp r15d, height */
        0x75,0x0b,                                     /* jne done */
        0xbb,0x80,0x07,0x00,0x00,                      /* mov ebx, 1920 */
        0x41,0xbf,0x38,0x04,0x00,0x00,                 /* mov r15d, 1080 */
        0xff,0x25,0x00,0x00,0x00,0x00,                 /* done: jmp [rip+resume] */
    };
    memcpy(body + 13, &width, 4);
    memcpy(body + 22, &height, 4);
    memcpy(out, body, sizeof(body));
    memcpy(out + sizeof(body), &resume, 8);
    return sizeof(body) + 8;
}

/* The layer composites (at 0x6b917e and 0x6ba130) scale their texel rects by the
 * reciprocal of the offscreen buffer's T# size; a scaled buffer counts as 1920x1080 there too.
 * `and ecx,0x3fff; inc eax; inc ecx` (ecx width, eax height) replaced. */
static const uint32_t odin_uvscale_hooks[] = {0x6b917e, 0x6ba130};
enum { ODIN_UVSCALE_HOOK_LENGTH = 10 };
static const unsigned char odin_uvscale_original[ODIN_UVSCALE_HOOK_LENGTH] = {
    0x81,0xe1,0xff,0x3f,0x00,0x00, 0xff,0xc0, 0xff,0xc1,
};

static inline unsigned odin_uvscale_build_stub(unsigned char *out, uint32_t width, uint32_t height,
                                               uintptr_t resume) {
    unsigned char body[] = {
        0x81,0xe1,0xff,0x3f,0x00,0x00, 0xff,0xc0, 0xff,0xc1, /* the replaced code */
        0x81,0xf9,0,0,0,0,                             /* cmp ecx, width */
        0x75,0x11,                                     /* jne done */
        0x3d,0,0,0,0,                                  /* cmp eax, height */
        0x75,0x0a,                                     /* jne done */
        0xb9,0x80,0x07,0x00,0x00,                      /* mov ecx, 1920 */
        0xb8,0x38,0x04,0x00,0x00,                      /* mov eax, 1080 */
        0xff,0x25,0x00,0x00,0x00,0x00,                 /* done: jmp [rip+resume] */
    };
    memcpy(body + 12, &width, 4);
    memcpy(body + 19, &height, 4);
    memcpy(out, body, sizeof(body));
    memcpy(out + sizeof(body), &resume, 8);
    return sizeof(body) + 8;
}

static inline int odin_render_scale_signature_match(const unsigned char *image, uint64_t size) {
    if (ODIN_VIEWPORT_HOOK_OFFSET + ODIN_VIEWPORT_HOOK_LENGTH > size ||
        memcmp(image + ODIN_VIEWPORT_HOOK_OFFSET, odin_viewport_original, ODIN_VIEWPORT_HOOK_LENGTH))
        return 0;
    for (unsigned i = 0; i < sizeof(odin_scale_width_imm) / sizeof(*odin_scale_width_imm); ++i) {
        uint32_t w, h;
        memcpy(&w, image + odin_scale_width_imm[i], 4);
        memcpy(&h, image + odin_scale_height_imm[i], 4);
        if (w != 1920 || h != 1080) return 0;
    }
    for (unsigned i = 0; i < sizeof(odin_scale_width_float) / sizeof(*odin_scale_width_float); ++i) {
        float w, h;
        memcpy(&w, image + odin_scale_width_float[i], 4);
        memcpy(&h, image + odin_scale_height_float[i], 4);
        if (w != 1920.0f || h != 1080.0f) return 0;
    }
    uint32_t heap;
    memcpy(&heap, image + ODIN_RENDERBUFFER_HEAP_IMM, 4);
    return heap == ODIN_RENDERBUFFER_HEAP_SIZE &&
           !memcmp(image + ODIN_SCISSOR_HOOK_OFFSET, odin_scissor_original, ODIN_SCISSOR_HOOK_LENGTH) &&
           !memcmp(image + ODIN_LAYER_HOOK_OFFSET, odin_layer_original, ODIN_LAYER_HOOK_LENGTH) &&
           !memcmp(image + ODIN_TEXSIZE_HOOK_OFFSET, odin_texsize_original, ODIN_TEXSIZE_HOOK_LENGTH) &&
           !memcmp(image + odin_uvscale_hooks[0], odin_uvscale_original, ODIN_UVSCALE_HOOK_LENGTH) &&
           !memcmp(image + odin_uvscale_hooks[1], odin_uvscale_original, ODIN_UVSCALE_HOOK_LENGTH);
}

/* The same conversions with the rect scaled first; xmm2 is free (a call follows). */
static inline unsigned odin_viewport_build_stub(unsigned char *out, float scale, uintptr_t resume) {
    static const unsigned char body[] = {
        0xc4,0xc1,0x7a,0x10,0x94,0x24,0x84,0x02,0x00,0x00, /* vmovss xmm2, [r12+0x284] */
        0xc5,0xea,0x59,0x15,0x58,0x00,0x00,0x00,           /* vmulss xmm2, xmm2, [rip+scale] */
        0xc4,0xe1,0xfa,0x2c,0xf2,                          /* vcvttss2si rsi, xmm2 */
        0xc4,0xc1,0x7a,0x10,0x94,0x24,0x88,0x02,0x00,0x00, /* vmovss xmm2, [r12+0x288] */
        0xc5,0xea,0x59,0x15,0x41,0x00,0x00,0x00,
        0xc4,0xe1,0xfa,0x2c,0xd2,                          /* vcvttss2si rdx, xmm2 */
        0xc4,0xc1,0x7a,0x10,0x94,0x24,0x8c,0x02,0x00,0x00, /* vmovss xmm2, [r12+0x28c] */
        0xc5,0xea,0x59,0x15,0x2a,0x00,0x00,0x00,
        0xc4,0xe1,0xfa,0x2c,0xca,                          /* vcvttss2si rcx, xmm2 */
        0xc4,0xc1,0x7a,0x10,0x94,0x24,0x90,0x02,0x00,0x00, /* vmovss xmm2, [r12+0x290] */
        0xc5,0xea,0x59,0x15,0x13,0x00,0x00,0x00,
        0xc4,0x61,0xfa,0x2c,0xc2,                          /* vcvttss2si r8, xmm2 */
        0xff,0x25,0x00,0x00,0x00,0x00,                     /* jmp [rip+resume] */
    };
    memcpy(out, body, sizeof(body));
    memcpy(out + sizeof(body), &resume, 8);    /* resume at 0x62 */
    memcpy(out + sizeof(body) + 8, &scale, 4); /* scale at 0x6a */
    return sizeof(body) + 12;
}
#endif
