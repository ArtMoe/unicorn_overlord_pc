/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef UNICORN_BUTTON_ICONS_H
#define UNICORN_BUTTON_ICONS_H
/* Unicorn Overlord: Xbox and Nintendo button icons from the game's own data.
 *
 * GUI/Interface_Button.mbs (a Vanillaware FMBS sprite file) names every button animation four
 * times, one per release of the game: "BUTTON_B" (Switch), "CROSS_PS4" and "BUTTON_B_2" (the PS4
 * dark and light icons) and "BUTTON_B_3" (Xbox). The PS4 build only plays the PS4 two. Animation
 * entries (section 9, 0x30 bytes: bounds, name, first keyframe set and count) are read where the
 * file lies in memory, so pointing the PS4 entries at the Xbox or Switch keyframes changes every
 * icon the game draws, also while it runs. The file is found when the CRI decompressor
 * (FUN_001fdfc0) has decoded it: header = the 0x100 raw bytes after the compressed stream.
 * Section layout (pointer at 0xf8, count at 0x6c) as documented by rufaswan's Web2D_Games. */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { BUTTON_ICONS_PLAYSTATION = 0, BUTTON_ICONS_XBOX = 1, BUTTON_ICONS_NINTENDO = 2 };

#define BUTTON_ICONS_HEADER 0x100u
#define BUTTON_ICONS_ENTRY 0x30u
#define BUTTON_ICONS_NAME 0x10u
#define BUTTON_ICONS_NAME_SIZE 0x18u
#define BUTTON_ICONS_KEYS 0x28u
#define BUTTON_ICONS_MAX_GROUPS 48

typedef struct {
    unsigned char *entries; /* section 9 in the game's copy of the file */
    unsigned char *saved;   /* the original entries */
    uint32_t count, groups;
    uint16_t group[BUTTON_ICONS_MAX_GROUPS][4]; /* Switch, PS4 dark, PS4 light, Xbox */
    int style;
} ButtonIcons;

static ButtonIcons button_icons = {.style = BUTTON_ICONS_PLAYSTATION};
static pthread_mutex_t button_icons_lock = PTHREAD_MUTEX_INITIALIZER;

static const char *button_icons_name(const unsigned char *entry) {
    return (const char *)entry + BUTTON_ICONS_NAME;
}

static int button_icons_named(const unsigned char *entry, const char *name) {
    return !strncmp(button_icons_name(entry), name, BUTTON_ICONS_NAME_SIZE);
}

/* Groups of four: "X", then "..._PS4" or "X_1", then "X_2" and "X_3". */
static uint32_t button_icons_find_groups(const unsigned char *entries, uint32_t count,
                                         uint16_t group[][4], uint32_t max) {
    uint32_t found = 0;
    for (uint32_t i = 0; i + 3 < count && found < max; ++i) {
        char base[BUTTON_ICONS_NAME_SIZE + 1] = {0}, name[BUTTON_ICONS_NAME_SIZE + 8];
        memcpy(base, button_icons_name(entries + i * BUTTON_ICONS_ENTRY), BUTTON_ICONS_NAME_SIZE);
        if (!base[0] || strlen(base) > BUTTON_ICONS_NAME_SIZE - 3) continue;
        const char *second = button_icons_name(entries + (i + 1) * BUTTON_ICONS_ENTRY);
        size_t second_length = 0;
        while (second_length < BUTTON_ICONS_NAME_SIZE && second[second_length]) ++second_length;
        snprintf(name, sizeof(name), "%s_1", base);
        if (!(second_length > 4 && !memcmp(second + second_length - 4, "_PS4", 4)) &&
            !button_icons_named(entries + (i + 1) * BUTTON_ICONS_ENTRY, name))
            continue;
        snprintf(name, sizeof(name), "%s_2", base);
        if (!button_icons_named(entries + (i + 2) * BUTTON_ICONS_ENTRY, name)) continue;
        snprintf(name, sizeof(name), "%s_3", base);
        if (!button_icons_named(entries + (i + 3) * BUTTON_ICONS_ENTRY, name)) continue;
        for (uint32_t k = 0; k < 4; ++k) group[found][k] = (uint16_t)(i + k);
        ++found;
        i += 3;
    }
    return found;
}

/* Points the PS4 entries at the style's keyframes (the original ones for PlayStation). */
static void button_icons_write(ButtonIcons *b, int style) {
    for (uint32_t g = 0; g < b->groups; ++g) {
        const uint16_t source = style == BUTTON_ICONS_XBOX       ? b->group[g][3]
                                : style == BUTTON_ICONS_NINTENDO ? b->group[g][0]
                                                                 : 0;
        for (int k = 1; k <= 2; ++k) {
            const uint16_t target = b->group[g][k];
            const unsigned char *from = b->saved + (style == BUTTON_ICONS_PLAYSTATION ? target : source) * BUTTON_ICONS_ENTRY;
            unsigned char *to = b->entries + target * BUTTON_ICONS_ENTRY;
            memcpy(to, from, BUTTON_ICONS_NAME);                                      /* bounds */
            memcpy(to + BUTTON_ICONS_KEYS, from + BUTTON_ICONS_KEYS, 4);               /* keyframes */
        }
    }
}

/* The game's copy is still the button file (not freed and reused). */
static int button_icons_intact(const ButtonIcons *b) {
    if (!b->entries) return 0;
    for (uint32_t g = 0; g < b->groups; ++g)
        for (int k = 0; k < 4; ++k) {
            const uint16_t i = b->group[g][k];
            if (memcmp(b->entries + i * BUTTON_ICONS_ENTRY + BUTTON_ICONS_NAME,
                       b->saved + i * BUTTON_ICONS_ENTRY + BUTTON_ICONS_NAME, BUTTON_ICONS_NAME_SIZE))
                return 0;
        }
    return 1;
}

/* After a decompression: header = the file's first 0x100 bytes, body = the rest as decoded.
 * Returns 1 when it was the button file (and applies the current style to it). */
static int button_icons_loaded(const unsigned char *header, unsigned char *body, uint64_t body_size) {
    if (memcmp(header, "FMBS", 4) || strncmp((const char *)header + 0x80, "Interface_Button.mbs", 0x20)) return 0;
    uint64_t offset;
    uint16_t count;
    memcpy(&offset, header + 0xf8, 8);
    memcpy(&count, header + 0x6c, 2);
    if (offset < BUTTON_ICONS_HEADER || offset - BUTTON_ICONS_HEADER + (uint64_t)count * BUTTON_ICONS_ENTRY > body_size) return 0;
    unsigned char *entries = body + (offset - BUTTON_ICONS_HEADER);
    unsigned char *saved = malloc((size_t)count * BUTTON_ICONS_ENTRY);
    if (!saved) return 0;
    memcpy(saved, entries, (size_t)count * BUTTON_ICONS_ENTRY);
    pthread_mutex_lock(&button_icons_lock);
    free(button_icons.saved);
    button_icons.entries = entries;
    button_icons.saved = saved;
    button_icons.count = count;
    button_icons.groups = button_icons_find_groups(entries, count, button_icons.group, BUTTON_ICONS_MAX_GROUPS);
    button_icons_write(&button_icons, button_icons.style);
    pthread_mutex_unlock(&button_icons_lock);
    return 1;
}

/* A new style (the pad code calls it when the setting or the controller changes). */
static void button_icons_set_style(int style) {
    pthread_mutex_lock(&button_icons_lock);
    if (style != button_icons.style) {
        button_icons.style = style;
        if (button_icons_intact(&button_icons)) button_icons_write(&button_icons, style);
        else button_icons.entries = NULL;
    }
    pthread_mutex_unlock(&button_icons_lock);
}

#endif
