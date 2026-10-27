/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef UNICORN_ULTRAWIDE_H
#define UNICORN_ULTRAWIDE_H
#include <stdint.h>
#include <string.h>
#include "unicorn_build.h"

/* Unicorn Overlord CUSA36381 v01.04: a render size wider than 16:9 shows more of the scene at
 * the sides (Hor+) instead of stretching it; menus and text keep their 16:9 layout in the
 * middle. The game lays everything out for a 1920x1080 screen; at a width W and height H these
 * patches widen that screen to D = 1080 * W / H.
 *
 * 2D: FUN_00233cd0 gives every 2D layer the projection it builds at 0x234cd4 (`mov dword
 * [rsp+0x180], 2/1920`); it becomes 2/D. The same 2/1920 at 0x3cc9cc stays: it projects the
 * battle map's haze mask (1280x720, x = -990..990, sampled across the whole screen), which at
 * 2/D covered only the middle and left darker, sharper strips at the sides. */
static const struct {
    uint32_t imm;              /* the imm32 operand */
    unsigned char length;      /* opcode bytes before it */
    unsigned char opcode[7];
} unicorn_ultrawide_ortho[] = {
    {0x234cdb, 7, {0xc7, 0x84, 0x24, 0x80, 0x01, 0x00, 0x00}}, /* mov dword [rsp+0x180], imm32 */
};
enum { UNICORN_ULTRAWIDE_ORTHOS = sizeof(unicorn_ultrawide_ortho) / sizeof(*unicorn_ultrawide_ortho) };
#define UNICORN_ULTRAWIDE_ORTHO_1920 0x3a888889u /* 2.0f / 1920 */
/* 3D: the battle map's perspective takes its aspect from 9/16 in rodata (m[0] = m[5] * 9/16,
 * built after 0x3cc9c6); FUN_00a35840 reads it too. It becomes H/W. */
static const uint32_t unicorn_ultrawide_aspect = 0xaf3350;
/* The full-screen quad of the last post-processing pass (depth of field, colour) and a
 * rectangle beside it, in .data: x = -960 and 960 become -D/2 and D/2. Without them that pass
 * squeezes the whole scene into the middle 16:9. */
static const uint32_t unicorn_ultrawide_quad = 0x1602ad8; /* 4 corners (x, y) */
static const uint32_t unicorn_ultrawide_rect = 0x1600ce8; /* left, top, right, bottom */
/* The corners of full-screen fills (fades, loading screens) in rodata: (-960, -540) and
 * (960, 540), read by about a dozen callers of the rectangle draw FUN_00024990. */
static const uint32_t unicorn_ultrawide_fill[] = {0xaf1f90, 0xaf1fa0};

/* Instructions that read 1920.0 (0xaf3290), -960.0 (0xaf3298) or 960.0 (0xaf31ac) for the
 * screen width, among many that must keep them (menus centred with FUN_00011f20, ...): each is
 * an 8-byte VEX op with a [rip+disp32] operand at +4, pointed at a float of its own. */
enum { UNICORN_ULTRAWIDE_WIDTH, UNICORN_ULTRAWIDE_LEFT, UNICORN_ULTRAWIDE_RIGHT, UNICORN_ULTRAWIDE_VALUES };
static const struct {
    uint32_t at, target;
    unsigned char value; /* UNICORN_ULTRAWIDE_WIDTH (D), _LEFT (-D/2) or _RIGHT (D/2) */
} unicorn_ultrawide_loads[] = {
    {0x386121, 0xaf3290, UNICORN_ULTRAWIDE_WIDTH}, /* FUN_003860f0: full-screen fade (loading) */
    /* The battle map camera keeps the screen inside the map: FUN_002dfe70 and FUN_002e1750 take
     * zoom >= 1920 / map width and clamp the centre to the map minus +-960 / zoom. */
    {0x2e0dda, 0xaf3290, UNICORN_ULTRAWIDE_WIDTH},
    {0x2e0e1f, 0xaf3298, UNICORN_ULTRAWIDE_LEFT},
    {0x2e0e27, 0xaf31ac, UNICORN_ULTRAWIDE_RIGHT},
    {0x2e17cf, 0xaf3290, UNICORN_ULTRAWIDE_WIDTH},
    {0x2e1823, 0xaf3298, UNICORN_ULTRAWIDE_LEFT},
    {0x2e182b, 0xaf31ac, UNICORN_ULTRAWIDE_RIGHT},
    {0x3868eb, 0xaf31ac, UNICORN_ULTRAWIDE_RIGHT}, /* FUN_00386680: the battle map's haze, +-960 */
    /* FUN_00940d90 and its copy at 0x9458a8: the weather layer's perspective (FUN_000396a0
     * takes the width and height for its aspect). */
    {0x940e79, 0xaf3290, UNICORN_ULTRAWIDE_WIDTH},
    {0x9458a8, 0xaf3290, UNICORN_ULTRAWIDE_WIDTH},
};
enum { UNICORN_ULTRAWIDE_LOADS = sizeof(unicorn_ultrawide_loads) / sizeof(*unicorn_ultrawide_loads) };

static inline int32_t unicorn_ultrawide_disp(const unsigned char *image, uint32_t at) {
    int32_t disp;
    memcpy(&disp, image + at + 4, 4);
    return disp;
}

static inline int unicorn_ultrawide_float_is(const unsigned char *image, uint32_t at, float value) {
    float f;
    memcpy(&f, image + unicorn_address(at), 4);
    return f == value;
}

static inline int unicorn_ultrawide_signature_match(const unsigned char *image, uint64_t size) {
    static const float quad[8] = {-960, -540, 960, -540, 960, 540, -960, 540}, rect[4] = {-960, -540, 960, 540};
    if (unicorn_ultrawide_quad + sizeof(quad) > size) return 0;
    for (unsigned i = 0; i < UNICORN_ULTRAWIDE_ORTHOS; ++i) {
        const uint32_t at = unicorn_address(unicorn_ultrawide_ortho[i].imm), length = unicorn_ultrawide_ortho[i].length;
        uint32_t value;
        memcpy(&value, image + at, 4);
        if (value != UNICORN_ULTRAWIDE_ORTHO_1920 || memcmp(image + at - length, unicorn_ultrawide_ortho[i].opcode, length))
            return 0;
    }
    if (!unicorn_ultrawide_float_is(image, unicorn_ultrawide_aspect, 9.0f / 16.0f)) return 0;
    for (unsigned i = 0; i < 8; ++i)
        if (!unicorn_ultrawide_float_is(image, unicorn_ultrawide_quad + 4 * i, quad[i])) return 0;
    for (unsigned i = 0; i < 4; ++i)
        if (!unicorn_ultrawide_float_is(image, unicorn_ultrawide_rect + 4 * i, rect[i])) return 0;
    for (unsigned i = 0; i < 2; ++i)
        if (!unicorn_ultrawide_float_is(image, unicorn_ultrawide_fill[i], i ? 960.0f : -960.0f) ||
            !unicorn_ultrawide_float_is(image, unicorn_ultrawide_fill[i] + 4, i ? 540.0f : -540.0f))
            return 0;
    for (unsigned i = 0; i < UNICORN_ULTRAWIDE_LOADS; ++i) {
        const uint32_t at = unicorn_address(unicorn_ultrawide_loads[i].at);
        if (image[at] != 0xc5 || (image[at + 3] & 0xc7) != 0x05 ||
            (int64_t)at + 8 + unicorn_ultrawide_disp(image, at) != unicorn_address(unicorn_ultrawide_loads[i].target))
            return 0;
    }
    return 1;
}

/* Patches a matching image (unicorn_ultrawide_signature_match) for a width x height render
 * size wider than 16:9. values: UNICORN_ULTRAWIDE_VALUES floats that stay allocated, within a
 * rel32 of the image; 0 when they are not (nothing is patched then). */
static inline int unicorn_ultrawide_apply(unsigned char *image, uint32_t width, uint32_t height, float *values) {
    for (unsigned i = 0; i < UNICORN_ULTRAWIDE_LOADS; ++i) {
        const int64_t disp = (int64_t)((intptr_t)(values + unicorn_ultrawide_loads[i].value) -
                                       (intptr_t)(image + unicorn_address(unicorn_ultrawide_loads[i].at) + 8));
        if (disp != (int32_t)disp) return 0;
    }
    const float design = 1080.0f * (float)width / (float)height, half = design / 2.0f, left = -half;
    const float ortho = 2.0f / design, aspect = (float)height / (float)width;
    for (unsigned i = 0; i < UNICORN_ULTRAWIDE_ORTHOS; ++i) memcpy(image + unicorn_address(unicorn_ultrawide_ortho[i].imm), &ortho, 4);
    memcpy(image + unicorn_address(unicorn_ultrawide_aspect), &aspect, 4);
    static const unsigned quad_left[] = {0, 6}, quad_right[] = {2, 4};
    for (unsigned i = 0; i < 2; ++i) {
        memcpy(image + unicorn_address(unicorn_ultrawide_quad + 4 * quad_left[i]), &left, 4);
        memcpy(image + unicorn_address(unicorn_ultrawide_quad + 4 * quad_right[i]), &half, 4);
    }
    memcpy(image + unicorn_address(unicorn_ultrawide_rect), &left, 4);
    memcpy(image + unicorn_address(unicorn_ultrawide_rect + 8), &half, 4);
    memcpy(image + unicorn_address(unicorn_ultrawide_fill[0]), &left, 4);
    memcpy(image + unicorn_address(unicorn_ultrawide_fill[1]), &half, 4);
    values[UNICORN_ULTRAWIDE_WIDTH] = design;
    values[UNICORN_ULTRAWIDE_LEFT] = left;
    values[UNICORN_ULTRAWIDE_RIGHT] = half;
    for (unsigned i = 0; i < UNICORN_ULTRAWIDE_LOADS; ++i) {
        const uint32_t at = unicorn_address(unicorn_ultrawide_loads[i].at);
        const int32_t disp = (int32_t)((intptr_t)(values + unicorn_ultrawide_loads[i].value) - (intptr_t)(image + at + 8));
        memcpy(image + at + 4, &disp, 4);
    }
    return 1;
}

#endif
