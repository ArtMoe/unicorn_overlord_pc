/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef UNICORN_RENDER_SCALE_H
#define UNICORN_RENDER_SCALE_H
#include <stdint.h>
#include <string.h>

/* Unicorn Overlord CUSA36381 v01.04: render above 1920x1080 (docs/unicorn/HANDOFF.md).
 * On a PS4 Pro (sceKernelIsNeoMode, cached by FUN_00218680) the game draws natively at
 * 3840x2160 with its Font_4K/ fonts. Its screen size getters FUN_0003e2b0 (width) and
 * FUN_0003e2d0 (height), and their two inlined copies in FUN_0003cb60 (the MAIN BUFFER color
 * and depth targets), pick the Pro size with `mov ecx, 0xf00` / `mov ecx, 0x870`: a smaller
 * Pro size (1440p, 1800p) is those six imm32 operands. */
static const uint32_t unicorn_width_imm[] = {0x3d9fd, 0x3db77, 0x3e2bd};
static const uint32_t unicorn_height_imm[] = {0x3da19, 0x3db5b, 0x3e2dd};

/* Each operand follows the `mov ecx` opcode (0xb9) and holds 3840 or 2160. */
static inline int unicorn_render_scale_signature_match(const unsigned char *image, uint64_t size) {
    for (unsigned i = 0; i < 3; ++i) {
        const uint32_t w = unicorn_width_imm[i], h = unicorn_height_imm[i];
        uint32_t wv, hv;
        if (w + 4 > size || h + 4 > size || image[w - 1] != 0xb9 || image[h - 1] != 0xb9) return 0;
        memcpy(&wv, image + w, 4);
        memcpy(&hv, image + h, 4);
        if (wv != 3840 || hv != 2160) return 0;
    }
    return 1;
}

#endif
