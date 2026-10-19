/* src/unicorn_native_settings.h on a made-up game image: a game that differs is left alone; on
 * v01.04's instructions the tables, bounds and masks take the added rows, their texts come from
 * the rows, and the screen's update applies changed rows. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unicorn_native_settings.h"

static const char *const sizes[] = {"Small", "Large"};
static const VwgpuSettingRow rows[] = {
    {"PC", NULL, VWGPU_ROW_SECTION, 0, NULL, 0, 0, 0, 0, NULL},
    {"Size", "How big.", VWGPU_ROW_CHOICE, 2, sizes, 1, 0, 0, 0, NULL},
    {"Glow", "Shines.", VWGPU_ROW_SWITCH, 0, NULL, 0, 0, 0, 0, NULL},
    {"Strength", "How strong.", VWGPU_ROW_SLIDER, 0, NULL, 50, 0, 100, 1, "%.0f%%"},
};
enum { ROWS = sizeof(rows) / sizeof(*rows), IMAGE = 0x1600000, PAGE = 4096 };
static float settings[ROWS] = {0, 1, 0, 50};
static int sets;

int vwgpu_setting_rows(const VwgpuSettingRow **out) {
    if (out) *out = rows;
    return ROWS;
}
float vwgpu_setting_value(int row) { return settings[row]; }
void vwgpu_setting_set(int row, float value) {
    settings[row] = value;
    ++sets;
}

static void put(unsigned char *image, uint64_t at, const char *hex) {
    for (size_t i = 0; hex[2 * i]; ++i) {
        unsigned value;
        sscanf(hex + 2 * i, "%2x", &value);
        image[at + i] = (unsigned char)value;
    }
}
static uint64_t lea_target(const unsigned char *image, uint64_t at) {
    int32_t disp;
    memcpy(&disp, image + at + 3, 4);
    return (uint64_t)((int64_t)at + 7 + disp);
}

/* The screen's rows: value() through the vtable, as the game's row classes do. */
typedef struct { const void *vtable; float value; } Row;
static ABI float row_value(void *row) { return ((Row *)row)->value; }
static ABI void game_update(void *ui) { (void)ui; }

int main(void) {
    unsigned char *image = calloc(1, IMAGE + 2 * PAGE), *code = image + IMAGE, *data = code + PAGE;
    assert(image);
    for (size_t i = 0; i < NATIVE_COUNT(native_code); ++i) put(image, native_code[i].at, native_code[i].bytes);
    for (size_t i = 0; i < NATIVE_COUNT(native_bounds); ++i) put(image, native_bounds[i].at, native_bounds[i].bytes);
    for (size_t i = 0; i < NATIVE_COUNT(native_leas); ++i) {
        const int32_t disp = (int32_t)(native_leas[i].table - (native_leas[i].at + 7));
        put(image, native_leas[i].at, "488d05");
        memcpy(image + native_leas[i].at + 3, &disp, 4);
    }
    const uint32_t order[NATIVE_GAME_ROWS] = {0, 1, 2, 3, 4, 15, 17, 18, 14, 16, 6, 7, 8, 9, 5, 10, 11, 12, 13, 19, 20};
    memcpy(image + NATIVE_ORDER, order, sizeof(order));
    memcpy(image + NATIVE_ORDER_SHORT, order + 1, 4);
    memcpy(image + NATIVE_ORDER_SHORT + 4, order + 2, sizeof(order) - 8);
    const NativeRow volume = {100.0f, 76, 33, VWGPU_ROW_SLIDER, 0};
    memcpy(image + NATIVE_ROWS + 7 * sizeof(NativeRow), &volume, sizeof(volume));

    /* One instruction differs: nothing changes. */
    unsigned char *copy = malloc(IMAGE);
    assert(copy);
    image[0x739135 + 3] = 0x15;
    memcpy(copy, image, IMAGE);
    assert(!native_settings_install(image, code, data));
    assert(!memcmp(copy, image, IMAGE));
    image[0x739135 + 3] = 0x14;

    assert(native_settings_install(image, code, data));
    NativeTables *t = native_settings.tables;
    assert(native_settings.count == ROWS && native_settings.open && native_settings.update);
    for (size_t i = 0; i < NATIVE_COUNT(native_leas); ++i) {
        const uint64_t table = native_leas[i].table, target = lea_target(image, native_leas[i].at);
        const void *want = table == NATIVE_ORDER ? (void *)t->order : table == NATIVE_ORDER_SHORT ? (void *)t->order_short
                           : table == NATIVE_ROWS ? (void *)t->rows : (void *)t->sliders;
        assert(image + target == want);
    }
    assert(t->order[13] == 9 && t->order[14] == 5 && t->order[21] == 21 && t->order[24] == 24);
    assert(t->order_short[19] == 20 && t->order_short[20] == 21 && t->order_short[23] == 24);
    assert(t->rows[7].label == 76 && t->rows[7].fallback == 100.0f);  /* the game's rows are kept */
    assert(t->rows[21].label == NATIVE_TEXT && t->rows[21].help == -1 && t->rows[21].kind == VWGPU_ROW_SECTION);
    assert(t->rows[22].fallback == 1.0f && t->rows[22].help == NATIVE_TEXT + 64 + NATIVE_HELP);
    const NativeChoice *size = (const NativeChoice *)(uintptr_t)t->choices[22];
    assert(size->id == 22 && size->count == 2 && size->first_text == NATIVE_TEXT + 64 + NATIVE_CHOICE);
    assert(t->choices[5] == (uintptr_t)(image + 0xd7cbac));
    const NativeSlider *strength = (const NativeSlider *)(uintptr_t)t->sliders[24 - 4];
    assert(strength->id == 24 && strength->max == 100.0f && strength->step == 1.0f);
    assert(strength->format_text == NATIVE_TEXT + 3 * 64 + NATIVE_FORMAT);

    /* Bounds: 4 rows more; rows = 0x14 + 4 + (order == long). */
    assert(image[0x735790 + 3] == 0x13 + ROWS && image[0x7357c0 + 2] == 0x15 + ROWS && image[0x7398c6 + 3] == 0x14 + ROWS);
    assert(image[0x735719 + 1] == 0xc0 && image[0x735719 + 2] == 0x14 + ROWS);
    uint32_t mask;
    memcpy(&mask, image + 0x738e88 + 1, 4);
    assert(mask == (0x1441u | 1u << 21));
    memcpy(&mask, image + 0x738ed1 + 2, 4);
    assert(mask == (0x1d680cu | 1u << 23));
    assert(image[0x2b590] == 0xe9 && image[0x735490] == 0xe9 && image[0x738f70] == 0xe9);

    /* Texts: the added rows' ids, else the sheet's own table. */
    assert(!strcmp(native_settings_text(NULL, NATIVE_TEXT + 64 + NATIVE_LABEL), "Size"));
    assert(!strcmp(native_settings_text(NULL, NATIVE_TEXT + 64 + NATIVE_HELP + 1), "How big."));
    assert(!strcmp(native_settings_text(NULL, NATIVE_TEXT + 64 + NATIVE_CHOICE + 1), "Large"));
    assert(!native_settings_text(NULL, NATIVE_TEXT + 64 + NATIVE_CHOICE + 2));
    assert(!strcmp(native_settings_text(NULL, NATIVE_TEXT + 3 * 64 + NATIVE_FORMAT), "%.0f%%"));
    assert(!native_settings_text(NULL, NATIVE_TEXT + ROWS * 64));
    unsigned char sheet[0x130] = {0}, table[0x30] = {0};
    const char *strings[2] = {"System", "Sound"};
    const unsigned char *table_address = table;
    const uint32_t count = 2;
    memcpy(sheet + 0x128, &table_address, 8);
    memcpy(table + 0x14, &count, 4);
    memcpy(table + 0x20, strings, sizeof(strings));
    assert(native_settings_text(sheet, 1) == strings[1] && !native_settings_text(sheet, 2) && !native_settings_text(sheet, -1));

    /* Opening loads the settings; the update applies a row that changed and keeps its value on open. */
    for (unsigned i = 0; i < ROWS; ++i) t->values[i] = -1.0f;
    native_settings.open = game_update;
    native_settings_open(NULL);
    assert(t->values[1] == 1.0f && t->values[3] == 50.0f);
    static void *vtable[32];
    vtable[NATIVE_ROW_VALUE / 8] = (void *)row_value;
    enum { ON_SCREEN = NATIVE_GAME_ROWS - 1 + ROWS };
    Row screen[ON_SCREEN];
    void *pointers[ON_SCREEN];
    float saved[ON_SCREEN];
    for (int i = 0; i < ON_SCREEN; ++i) {
        screen[i] = (Row){vtable, 0.0f};
        pointers[i] = &screen[i];
        saved[i] = -1.0f;
    }
    screen[ON_SCREEN - 3].value = 1.0f; /* Size: unchanged */
    screen[ON_SCREEN - 1].value = 64.0f; /* Strength: moved */
    unsigned char ui[0x6300] = {0};
    void **begin = pointers, **end = pointers + ON_SCREEN;
    float *saved_address = saved;
    const int32_t on_screen = ON_SCREEN;
    memcpy(ui + NATIVE_UI_ROWS, &begin, 8);
    memcpy(ui + NATIVE_UI_ROWS + 8, &end, 8);
    memcpy(ui + NATIVE_UI_SAVED, &saved_address, 8);
    memcpy(ui + NATIVE_UI_COUNT, &on_screen, 4);
    native_settings.update = game_update;
    sets = 0;
    native_settings_update(ui);
    assert(sets == 1 && settings[3] == 64.0f && t->values[3] == 64.0f);
    assert(saved[ON_SCREEN - 1] == 64.0f && saved[ON_SCREEN - 3] == 1.0f && saved[ON_SCREEN - 4] == -1.0f);
    native_settings_update(ui); /* nothing new */
    assert(sets == 1);
    free(copy);
    free(image);
    puts("PASS: native settings rows");
    return 0;
}
