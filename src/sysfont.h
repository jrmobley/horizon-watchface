
#pragma once
#include <pebble.h>

typedef struct {
    const char* key;
    GFont font;
    int16_t em;
    int16_t ascent;
    int16_t descent;
} SystemFont;

extern SystemFont Gothic14;
extern SystemFont Gothic14Bold;
extern SystemFont Gothic18;
extern SystemFont Gothic18Bold;
extern SystemFont Gothic24;
extern SystemFont Gothic24Bold;
extern SystemFont Gothic28;
extern SystemFont Gothic28Bold;
