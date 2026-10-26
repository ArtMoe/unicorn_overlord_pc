/* src/unicorn_button_icons.h on a made-up Interface_Button.mbs: the PS4 animation entries take the
 * Xbox or Switch keyframes and get their own back for PlayStation. Xbox A and B are swapped
 * against the Switch names (issue #41). */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "unicorn_button_icons.h"

static const char *const names[] = {"BUTTON_A", "CIRCLE_PS4", "BUTTON_A_2", "BUTTON_A_3", "",
                                    "BUTTON_ARROW", "BUTTON_ARROW_1", "BUTTON_ARROW_2", "BUTTON_ARROW_3", "HP",
                                    "BUTTON_B", "CROSS_PS4", "BUTTON_B_2", "BUTTON_B_3"};
enum { COUNT = sizeof(names) / sizeof(*names), SECTION = 0x200 };

static uint16_t keys_of(const unsigned char *body, int i) {
    uint16_t v;
    memcpy(&v, body + SECTION - BUTTON_ICONS_HEADER + i * BUTTON_ICONS_ENTRY + BUTTON_ICONS_KEYS, 2);
    return v;
}

int main(void) {
    static unsigned char header[BUTTON_ICONS_HEADER], body[0x1000];
    memcpy(header, "FMBS", 4);
    strcpy((char *)header + 0x80, "Interface_Button.mbs");
    const uint64_t offset = SECTION;
    const uint16_t count = COUNT;
    memcpy(header + 0xf8, &offset, 8);
    memcpy(header + 0x6c, &count, 2);
    for (uint16_t i = 0; i < COUNT; ++i) {
        unsigned char *e = body + SECTION - BUTTON_ICONS_HEADER + i * BUTTON_ICONS_ENTRY;
        strncpy((char *)e + BUTTON_ICONS_NAME, names[i], BUTTON_ICONS_NAME_SIZE);
        const uint16_t keys = (uint16_t)(100 + i);
        memcpy(e + BUTTON_ICONS_KEYS, &keys, 2);
    }

    unsigned char other[BUTTON_ICONS_HEADER];
    memcpy(other, header, sizeof(other));
    strcpy((char *)other + 0x80, "Interface_Battle_00.mbs");
    assert(!button_icons_loaded(other, body, sizeof(body)));

    button_icons_set_style(BUTTON_ICONS_XBOX); /* before the file loads: applied when it does */
    assert(button_icons_loaded(header, body, sizeof(body)));
    assert(button_icons.groups == 3);
    /* Circle (right) shows the Xbox B, Cross (bottom) the Xbox A; the arrows keep their own. */
    assert(keys_of(body, 1) == 113 && keys_of(body, 2) == 113 && keys_of(body, 0) == 100);
    assert(keys_of(body, 11) == 103 && keys_of(body, 12) == 103);
    assert(keys_of(body, 6) == 108 && keys_of(body, 7) == 108 && keys_of(body, 9) == 109);

    button_icons_set_style(BUTTON_ICONS_NINTENDO);
    assert(keys_of(body, 1) == 100 && keys_of(body, 2) == 100 && keys_of(body, 6) == 105);
    assert(keys_of(body, 11) == 110);

    button_icons_set_style(BUTTON_ICONS_PLAYSTATION);
    assert(keys_of(body, 1) == 101 && keys_of(body, 2) == 102 && keys_of(body, 3) == 103);

    /* The memory now holds something else: left alone. */
    memset(body, 0xab, sizeof(body));
    button_icons_set_style(BUTTON_ICONS_XBOX);
    assert(body[SECTION - BUTTON_ICONS_HEADER + BUTTON_ICONS_ENTRY + BUTTON_ICONS_KEYS] == 0xab);
    puts("PASS: button icon layouts");
    return 0;
}
