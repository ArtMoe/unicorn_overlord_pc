/* src/unicorn_ultrawide.h on a made-up image holding the v01.04 values at their addresses: a
 * 2560x1080 render size widens the 1920-wide screen to 2560. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unicorn_ultrawide.h"

enum { SIZE = 0x1602b00 };

static void put_float(unsigned char *image, uint32_t at, float value) { memcpy(image + at, &value, 4); }

static float get_float(const unsigned char *image, uint32_t at) {
    float value;
    memcpy(&value, image + at, 4);
    return value;
}

int main(void) {
    /* The floats the loads are pointed at follow the image, as a page near it does in the probe. */
    unsigned char *image = calloc(1, SIZE + 64);
    assert(image);
    float *values = (float *)(image + SIZE);
    for (unsigned i = 0; i < UNICORN_ULTRAWIDE_ORTHOS; ++i) {
        const uint32_t at = unicorn_ultrawide_ortho[i].imm, length = unicorn_ultrawide_ortho[i].length;
        memcpy(image + at - length, unicorn_ultrawide_ortho[i].opcode, length);
        const uint32_t ortho = UNICORN_ULTRAWIDE_ORTHO_1920;
        memcpy(image + at, &ortho, 4);
    }
    put_float(image, unicorn_ultrawide_aspect, 9.0f / 16.0f);
    static const float quad[8] = {-960, -540, 960, -540, 960, 540, -960, 540}, rect[4] = {-960, -540, 960, 540};
    for (unsigned i = 0; i < 8; ++i) put_float(image, unicorn_ultrawide_quad + 4 * i, quad[i]);
    for (unsigned i = 0; i < 4; ++i) put_float(image, unicorn_ultrawide_rect + 4 * i, rect[i]);
    for (unsigned i = 0; i < 2; ++i) {
        put_float(image, unicorn_ultrawide_fill[i], i ? 960.0f : -960.0f);
        put_float(image, unicorn_ultrawide_fill[i] + 4, i ? 540.0f : -540.0f);
    }
    for (unsigned i = 0; i < UNICORN_ULTRAWIDE_LOADS; ++i) {
        const uint32_t at = unicorn_ultrawide_loads[i].at;
        const unsigned char op[4] = {0xc5, 0xfa, 0x10, 0x05}; /* vmovss xmm0, [rip+disp32] */
        memcpy(image + at, op, 4);
        const int32_t disp = (int32_t)(unicorn_ultrawide_loads[i].target - (at + 8));
        memcpy(image + at + 4, &disp, 4);
    }
    assert(unicorn_ultrawide_signature_match(image, SIZE));
    assert(!unicorn_ultrawide_signature_match(image, unicorn_ultrawide_quad));

    assert(unicorn_ultrawide_apply(image, 2560, 1080, values));
    uint32_t ortho;
    memcpy(&ortho, image + unicorn_ultrawide_ortho[0].imm, 4);
    float orthof;
    memcpy(&orthof, &ortho, 4);
    assert(orthof == 2.0f / 2560.0f);
    assert(get_float(image, unicorn_ultrawide_aspect) == 1080.0f / 2560.0f);
    assert(get_float(image, unicorn_ultrawide_quad) == -1280.0f && get_float(image, unicorn_ultrawide_quad + 4) == -540.0f);
    assert(get_float(image, unicorn_ultrawide_quad + 8) == 1280.0f && get_float(image, unicorn_ultrawide_quad + 24) == -1280.0f);
    assert(get_float(image, unicorn_ultrawide_rect) == -1280.0f && get_float(image, unicorn_ultrawide_rect + 8) == 1280.0f);
    assert(get_float(image, unicorn_ultrawide_rect + 12) == 540.0f);
    assert(get_float(image, unicorn_ultrawide_fill[0]) == -1280.0f && get_float(image, unicorn_ultrawide_fill[1]) == 1280.0f);
    static const float expect[UNICORN_ULTRAWIDE_VALUES] = {2560.0f, -1280.0f, 1280.0f};
    for (unsigned i = 0; i < UNICORN_ULTRAWIDE_LOADS; ++i) {
        const uint32_t at = unicorn_ultrawide_loads[i].at;
        const float *read = (const float *)(image + at + 8 + unicorn_ultrawide_disp(image, at));
        assert(*read == expect[unicorn_ultrawide_loads[i].value]);
    }
    assert(!unicorn_ultrawide_signature_match(image, SIZE)); /* patched once only */
    free(image);
    puts("ultrawide: ok");
    return 0;
}
