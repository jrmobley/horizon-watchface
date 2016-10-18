/*
 *
 */

#include <pebble.h>
#include <pebble-fctx/fctx.h>
#include <pebble-fctx/ffont.h>
#include <pebble-utf8/pebble-utf8.h>
#include "isqrt.h"
#include "sysfont.h"

// --------------------------------------------------------------------------
// constants
// --------------------------------------------------------------------------

#define MESSAGE_BUFFER_SIZE 200
#define CLOCK_ANIM_DURATION 1000
const char* kDateFormat = "%b %d";
const char* kWeekdayFormat = "%a";

// --------------------------------------------------------------------------
// globals
// --------------------------------------------------------------------------

typedef enum PersistKeys {
    PersistKeyLocation = 100,
    PersistKeyLongitude,
    PersistKeyLatitude,
    PersistKeyTimezone,
    PersistKeyTimestamp,
    PersistKeySunrise,
    PersistKeySunset,
    PersistKeySunSouth,
    PersistKeySunStatus,
    PersistKeyBluetooth,
    PersistKeyBattery,
    PersistKeyPalette,
    PersistKeyDateFont,
} PersistKeys;

enum Palette {
    PaletteColorBehind,
    PaletteColorBelow,
    PaletteColorAbove,
    PaletteColorWithin,
    PaletteColorMarks,
    PaletteColorEngraving,
    PaletteColorText,
    PaletteColorSolar,
    PaletteColorCapacity,
    PaletteColorCharge,
    PaletteColorOnline,
    PaletteColorOffline,
    PaletteSize,
};

#if defined(PBL_COLOR)
static const uint8_t kDefaultPalette[PaletteSize] = {
    /* Should be the same as the default "color" palette in colors.js. */
    GColorLightGrayARGB8, // Behind
    GColorPictonBlueARGB8, // Below
    GColorIcterineARGB8, // Above
    GColorWhiteARGB8, // Within
    GColorBlackARGB8, // Marks
    GColorLightGrayARGB8, // Engraving
    GColorBlackARGB8, // Text
    GColorWhiteARGB8, // Solar
    GColorDarkGrayARGB8, // Capacity
    GColorWhiteARGB8, // Charge
    GColorWhiteARGB8, // Online
    GColorDarkGrayARGB8, // Offline
};
#else
static const uint8_t kDefaultPalette[PaletteSize] = {
    /* Should be the same as the default "white" palette in colors.js. */
    GColorLightGrayARGB8, // Behind
    GColorWhiteARGB8, // Below
    GColorWhiteARGB8, // Above
    GColorWhiteARGB8, // Within
    GColorBlackARGB8, // Marks
    GColorLightGrayARGB8, // Engraving
    GColorBlackARGB8, // Text
    GColorWhiteARGB8, // Solar
    GColorBlackARGB8, // Capacity
    GColorWhiteARGB8, // Charge
    GColorWhiteARGB8, // Online
    GColorBlackARGB8, // Offline
};
#endif

static void gctx_draw_string(FContext* fctx, const char* str, SystemFont* font, GTextAlignment align, FTextAnchor anchor) {

    if (font->font == NULL) {
        font->font = fonts_get_system_font(font->key);
    }

    GTextOverflowMode overflow = GTextOverflowModeTrailingEllipsis;
    GPoint offset = {
        .x = FIXED_TO_INT(fctx->transform_offset.x),
        .y = FIXED_TO_INT(fctx->transform_offset.y),
    };
    GRect box = {
        .origin = { .x = 0, .y = 0 },
        .size = { .w = 256, .h = font->em },
    };

    if (align == GTextAlignmentLeft) {
        box.origin.x = offset.x;
    } else {
        GSize size = graphics_text_layout_get_content_size(
            str, font->font, box, overflow, GTextAlignmentLeft);
        if (align == GTextAlignmentRight) {
            box.origin.x = offset.x - size.w;
        } else /* align == GTextAlignmentCenter */ {
            box.origin.x = offset.x - size.w / 2;
        }
    }

    if (anchor == FTextAnchorBottom) {
        box.origin.y = offset.y - font->descent - font->em;
    } else if (anchor == FTextAnchorMiddle || anchor == FTextAnchorCapMiddle) {
        box.origin.y = offset.y + font->ascent / 2 - font->em;
    } else if (anchor == FTextAnchorTop || anchor == FTextAnchorCapTop) {
        box.origin.y = offset.y + font->ascent - font->em;
    } else /* anchor == FTextAnchorBaseline) */ {
        box.origin.y = offset.y - font->em;
    }

    graphics_draw_text(fctx->gctx, str, font->font, box, overflow, GTextAlignmentLeft, NULL);
}


typedef struct {
    int32_t timestamp;
    int32_t timezone;
    int32_t longitude;
    int32_t latitude;
    int32_t sunrise;
    int32_t sunset;
    int32_t sunsouth;
    int32_t sunstat;
} LocationFix;

typedef struct {
    int16_t from;
    int16_t to;
    int16_t current;
} int16_anim_t;

typedef struct {
    int32_t from;
    int32_t to;
    int32_t current;
} int32_anim_t;

struct Clock {

    // layout
    fixed_t sunDiscRadius;
    fixed_t hourPipRadius;
    fixed_t sunOrbitRadius;
    fixed_t readoutDiscRadius;
    fixed_t dateTextGap;
    fixed_t strokeWidth;
    int16_t hourCapHeight;
    int16_t timeCapHeight;
    int16_t dateCapHeight;
    SystemFont* gothicRegular;
    SystemFont* gothicBold;
    SystemFont* dateFont;
    fixed_t battery_x[14];

    // configuration
    uint8_t bluetoothAlert;
    uint8_t batteryIndicator;
    GColor colors[PaletteSize];

    // external state
    struct tm gregorian;
    bool bluetooth;
    uint16_t battery;
    LocationFix location;

    // computed state
    uint16_t horizon;
    int32_t kilter;
    char strbuf[32];

    // animated state
    int16_anim_t above;
    int16_anim_t below;
    int32_anim_t rotation;

    AnimationImplementation animationImplementation;
    Animation* animation;
    Window* window;
    Layer* layer;
    FFont* font;

} g;

static void init();
static void deinit();
static void configureClock();
static void animateClock();
static void interpolateClock(Animation* animation, const AnimationProgress progress);
static void drawClock(Layer* layer, GContext* ctx);
static void drawBatteryDish(FContext* fctx, int height);
static void timeChanged(struct tm* tickTime, TimeUnits unitsChanged);
static void bluetoothConnected(bool connected);
static void batteryStateChanged(BatteryChargeState charge);
static void messageReceived(DictionaryIterator* iterator, void *context);

static FPoint clockPoint(FPoint center, fixed_t radius, uint32_t angle);
static GColor colorFromConfig(uint8_t cc);
static void applyPalette(const uint8_t* palette, int16_t length);
static void applyDateFont(int32_t dateFont);

static void logLocationFix(LocationFix* loc);

// --------------------------------------------------------------------------
// inline utility functions
// --------------------------------------------------------------------------

/* Convert hours to angle, measured clockwise from midnight. */
static inline int32_t hourAngle(int32_t hour) {
    return hour * TRIG_MAX_ANGLE / 24;
}

/* Convert minutes to angle, measured clockwise from midnight. */
static inline int32_t minuteAngle(int32_t minute) {
    return minute * TRIG_MAX_ANGLE / (24*60);
}

static inline int16_t sizeInner(GSize size) {
    if (size.w < size.h) {
        return size.w;
    }
    return size.h;
}

// --------------------------------------------------------------------------
// main
// --------------------------------------------------------------------------

int main() {
    init();
    app_event_loop();
    deinit();
}

// --------------------------------------------------------------------------
// init
// --------------------------------------------------------------------------

static void init() {

    /* KOJAK - Until we get accented characters into the font, it's no good
       to enable locale time formats.
    setlocale(LC_ALL, "");
    */

    /* --- Restore state from persistent storage. --- */

    int32_t dateFont = PBL_IF_BW_ELSE(1, 0);
    if (persist_exists(PersistKeyDateFont)) {
        dateFont = persist_read_int(PersistKeyDateFont);
    }
    /* applyDateFont after the font sizes are selected below. */

    if (persist_exists(PersistKeyBattery)) {
        g.batteryIndicator = persist_read_bool(PersistKeyBattery);
    } else {
        g.batteryIndicator = true;
    }

    if (persist_exists(PersistKeyBluetooth)) {
        g.bluetoothAlert = persist_read_int(PersistKeyBluetooth);
    } else {
        g.bluetoothAlert = 1;
    }

    if (persist_exists(PersistKeyPalette)) {
        uint8_t palette[PaletteSize];
        int length = persist_read_data(PersistKeyPalette, palette, PaletteSize);
        applyPalette(palette, length);
    } else {
        applyPalette(kDefaultPalette, PaletteSize);
    }

    if (persist_exists(PersistKeyTimestamp)) {
        g.location.timestamp = persist_read_int(PersistKeyTimestamp);
        g.location.timezone  = persist_read_int(PersistKeyTimezone);
        g.location.longitude = persist_read_int(PersistKeyLongitude);
        g.location.latitude  = persist_read_int(PersistKeyLatitude);
        g.location.sunrise   = persist_read_int(PersistKeySunrise);
        g.location.sunset    = persist_read_int(PersistKeySunset);
        g.location.sunsouth  = persist_read_int(PersistKeySunSouth);
        g.location.sunstat   = persist_read_int(PersistKeySunStatus);
        logLocationFix(&g.location);
    } else {
        g.location.timestamp = 0;
    }

    /* --- Allocate system resources. --- */

    g.window = window_create();
    window_set_background_color(g.window, GColorBlack);
    window_stack_push(g.window, true);
    Layer* windowLayer = window_get_root_layer(g.window);
    GRect frame = layer_get_frame(windowLayer);

    g.layer = layer_create(frame);
    layer_set_update_proc(g.layer, &drawClock);
    layer_add_child(windowLayer, g.layer);

    g.animation = NULL;

    g.font = ffont_create_from_resource(RESOURCE_ID_DIN_CONDENSED_FFONT);

    /* --- Calculate layout. --- */

    int16_t designRadius = sizeInner(frame.size) / 2 - PBL_IF_ROUND_ELSE(4, 0);
    g.sunDiscRadius = designRadius * 3 / 25;
    g.hourCapHeight = g.sunDiscRadius * 25 / 17;
    int16_t sunDiscMargin = g.sunDiscRadius * 1 / 3;
    g.sunOrbitRadius = designRadius - sunDiscMargin - g.sunDiscRadius;
    g.readoutDiscRadius = g.sunOrbitRadius - g.sunDiscRadius - sunDiscMargin;
    g.timeCapHeight = (g.readoutDiscRadius * 10 / 21) / 2 * 2;
    g.dateCapHeight = g.readoutDiscRadius * 3 / 11;
    g.dateTextGap = g.readoutDiscRadius * 4 / 50;
    g.strokeWidth = 2;

    /* KOJAK TODO : Return to a calculated dateCapHeight.
       Then choose the next smaller system gothic.
       The two fonts will not be the same height.  But that is okay
       because they aren't the same width at all anyway. */

#if defined(PBL_PLATFORM_EMERY)
    g.gothicRegular = &Gothic28;
    g.gothicBold = &Gothic28Bold;
#elif defined(PBL_PLATFORM_CHALK)
    g.gothicRegular = &Gothic24;
    g.gothicBold = &Gothic24Bold;
#else
    g.gothicRegular = &Gothic18;
    g.gothicBold = &Gothic18Bold;
#endif
    applyDateFont(dateFont);

    int32_t r = g.readoutDiscRadius - g.strokeWidth / 2;
    for (uint32_t k = 0; k < ARRAY_LENGTH(g.battery_x); ++k) {
        int32_t y = r - k;
        uint32_t q = usqrt(r*r - y*y);
        g.battery_x[k] = q / (SQRT_SCALE / FIXED_POINT_SCALE);
    }

    g.sunDiscRadius *= FIXED_POINT_SCALE;
    g.sunOrbitRadius *= FIXED_POINT_SCALE;
    g.readoutDiscRadius *= FIXED_POINT_SCALE;
    g.dateTextGap *= FIXED_POINT_SCALE;
    g.strokeWidth *= FIXED_POINT_SCALE;

    g.hourPipRadius = g.sunDiscRadius * 1 / 4;

    /* --- Initialize the clock state. --- */

    time_t now = time(NULL);
    g.gregorian = *localtime(&now);
    g.kilter = 0;

    g.bluetooth = bluetooth_connection_service_peek();

    batteryStateChanged(battery_state_service_peek());

    configureClock();

    if (0 == g.location.timestamp) {
        g.above.current = -frame.size.h / 2;
        g.below.current = frame.size.h / 2;
        g.rotation.current = 0;
    } else {
        g.above.current = g.horizon;
        g.below.current = g.horizon;
        g.rotation.current = g.kilter;
    }

    /* --- Activate system services. --- */

    app_message_register_inbox_received(messageReceived);
    app_message_open(MESSAGE_BUFFER_SIZE, APP_MESSAGE_OUTBOX_SIZE_MINIMUM);

    tick_timer_service_subscribe(MINUTE_UNIT | DAY_UNIT, &timeChanged);
    bluetooth_connection_service_subscribe(&bluetoothConnected);
    battery_state_service_subscribe(&batteryStateChanged);
}

// --------------------------------------------------------------------------

// --------------------------------------------------------------------------

static void deinit() {
    tick_timer_service_unsubscribe();
    bluetooth_connection_service_unsubscribe();
    battery_state_service_unsubscribe();
    window_destroy(g.window);
    layer_destroy(g.layer);
    ffont_destroy(g.font);
}

// --------------------------------------------------------------------------

// --------------------------------------------------------------------------

void configureClock() {

    if (0 == g.location.timestamp) {
        g.kilter = 0;
        g.horizon = 0;
        return;
    }

    /* Adjust the sun event times to integer minutes, local time. */
    int32_t timezone = g.location.timezone;
    int32_t sunRise = g.location.sunrise + timezone;
    int32_t sunSet = g.location.sunset + timezone;
    int32_t sunSouth = g.location.sunsouth + timezone;

    g.kilter = minuteAngle(12*60 - sunSouth);

    FPoint origin = FPointZero;
    int32_t sunriseAngle = minuteAngle(sunRise);
    int32_t sunsetAngle = minuteAngle(sunSet);
    FPoint sunrisePoint = clockPoint(origin, g.sunOrbitRadius, sunriseAngle + g.kilter);
    FPoint sunsetPoint = clockPoint(origin, g.sunOrbitRadius, sunsetAngle + g.kilter);

    g.horizon = FIXED_TO_INT((sunrisePoint.y + sunsetPoint.y + FIX1) / 2);
}

// --------------------------------------------------------------------------

// --------------------------------------------------------------------------

static void anim_stopped_handler(Animation *animation, bool finished, void *context) {
    if (animation == g.animation) {
        animation_destroy(g.animation);
        g.animation = NULL;
    }
}

void animateClock() {

    /* If none of the values have changed "much" then just set them
       and skip the animation. */
    uint16_t horizonThreshold = 4; // 4 pixels
    uint32_t kilterThreshold = TRIG_MAX_ANGLE / 60; // 1 arc minute
    bool skipAbove = abs(g.above.current - g.horizon) < horizonThreshold;
    bool skipBelow = abs(g.below.current - g.horizon) < horizonThreshold;
    bool skipRotation = abs(g.rotation.current - g.kilter) < kilterThreshold;
    if (skipAbove && skipBelow && skipRotation) {
        g.above.current = g.horizon;
        g.below.current = g.horizon;
        g.rotation.current = g.kilter;
        layer_mark_dirty(g.layer);
        return;
    }

    g.above.from = g.above.current;
    g.above.to = g.horizon;

    g.below.from = g.below.current;
    g.below.to = g.horizon;

    g.rotation.from = g.rotation.current;
    g.rotation.to = g.kilter;

    g.animationImplementation.setup = NULL;
    g.animationImplementation.update = &interpolateClock;
    g.animationImplementation.teardown = NULL;
    g.animation = animation_create();
    animation_set_implementation(g.animation, &g.animationImplementation);

    animation_set_duration(g.animation, CLOCK_ANIM_DURATION);
    animation_set_curve(g.animation, AnimationCurveEaseInOut);
    animation_set_handlers(g.animation, (AnimationHandlers) {
        .started = NULL,
        .stopped = anim_stopped_handler
    }, NULL);
    animation_schedule(g.animation);
}

void interpolateClock(Animation* animation, const AnimationProgress progress) {

    AnimationProgress norm = ANIMATION_NORMALIZED_MAX - ANIMATION_NORMALIZED_MIN;
    AnimationProgress t = progress - ANIMATION_NORMALIZED_MIN;

    // from + (to - from) * t
    g.above.current = g.above.from + (g.above.to - g.above.from) * t / norm;
    g.below.current = g.below.from + (g.below.to - g.below.from) * t / norm;
    g.rotation.current = g.rotation.from + (g.rotation.to - g.rotation.from) * t / norm;
    layer_mark_dirty(g.layer);
}

void formatTime() {
    if (clock_is_24h_style()) {
        /* For 24h format, use strftime, because clock_copy_string does
           not prepend a leading zero to single digit hours. */
        strftime(g.strbuf, ARRAY_LENGTH(g.strbuf), "%H:%M", &g.gregorian);
    } else {
        int hour = g.gregorian.tm_hour % 12;
        if (hour == 0) hour = 12;
        snprintf(g.strbuf, ARRAY_LENGTH(g.strbuf), "%d:%02d", hour, g.gregorian.tm_min);
    }
}

// --------------------------------------------------------------------------

// --------------------------------------------------------------------------

void drawClock(Layer* layer, GContext* ctx) {

    GRect bounds = layer_get_unobstructed_bounds(layer);
    GPoint center = grect_center_point(&bounds);
    FPoint fcenter = g2fpoint(center);
    GRect fill = bounds;
    GPoint left;
    GPoint right;
    left.x = bounds.origin.x;
    right.x = bounds.origin.x + bounds.size.w;

    /* Fill the space behind everything. */
    graphics_context_set_fill_color(ctx, g.colors[PaletteColorBehind]);
    graphics_fill_rect(ctx, fill, 0, GCornerNone);

    /* Fill the space above the horizon. */
    fill.origin.y = 0;
    fill.size.h = center.y + g.above.current;
    left.y = right.y = center.y + g.above.current - 1;
    graphics_context_set_fill_color(ctx, g.colors[PaletteColorAbove]);
    graphics_fill_rect(ctx, fill, 0, GCornerNone);
    graphics_context_set_stroke_color(ctx, g.colors[PaletteColorMarks]);
    graphics_draw_line(ctx, left, right);

    /* Fill the space below the horizon. */
    fill.origin.y = center.y + g.below.current;
    fill.size.h = bounds.origin.y + bounds.size.h - fill.origin.y;
    left.y = right.y = center.y + g.below.current;
    graphics_context_set_fill_color(ctx, g.colors[PaletteColorBelow]);
    graphics_fill_rect(ctx, fill, 0, GCornerNone);
    graphics_draw_line(ctx, left, right);

    FContext fctx;
    fctx_init_context(&fctx, ctx);

    /* Draw the solar orbit markings. */
    fctx_set_fill_color(&fctx, g.colors[PaletteColorMarks]);
    fctx_set_color_bias(&fctx, 0);
    fctx_begin_fill(&fctx);
    fctx_set_text_cap_height(&fctx, g.font, PBL_IF_COLOR_ELSE(g.hourCapHeight, FIXED_TO_INT(g.sunDiscRadius)*2));
    fctx_set_rotation(&fctx, g.rotation.current);
    for (int h = 0; h < 24; ++h) {
        FPoint c = clockPoint(fcenter, g.sunOrbitRadius, hourAngle(h) + g.rotation.current);

        if (h % 6) {
            fctx_plot_circle(&fctx, &c, g.hourPipRadius);
        } else {
#ifdef PBL_COLOR
            fctx_plot_circle(&fctx, &c, g.sunDiscRadius);
#endif
            fctx_set_offset(&fctx, c);
            if (h == 0) {
                fctx_draw_string(&fctx, "00", g.font, GTextAlignmentCenter, FTextAnchorMiddle);
            } else if (h == 6) {
                fctx_draw_string(&fctx, "06", g.font, GTextAlignmentCenter, FTextAnchorMiddle);
            } else if (h == 12) {
                fctx_draw_string(&fctx, "12", g.font, GTextAlignmentCenter, FTextAnchorMiddle);
            } else if (h == 18) {
                fctx_draw_string(&fctx, "18", g.font, GTextAlignmentCenter, FTextAnchorMiddle);
            }
        }
    }
    fctx_end_fill(&fctx);
    fctx_set_color_bias(&fctx, 0);

    /* Prep to draw the solar disc. */
    uint32_t minute = g.gregorian.tm_hour * 60 + g.gregorian.tm_min;
    FPoint sunPoint = clockPoint(fcenter, g.sunOrbitRadius, minuteAngle(minute) + g.rotation.current);

    /* Fill the solar disc. */
    fctx_begin_fill(&fctx);
    fctx_set_fill_color(&fctx, g.colors[PaletteColorSolar]);
    fctx_set_color_bias(&fctx, -3);
    fctx_plot_circle(&fctx, &sunPoint, g.sunDiscRadius - g.strokeWidth / 2);
    fctx_end_fill(&fctx);
    fctx_set_color_bias(&fctx, 0);

    /* Stroke the solar disc perimeter. */
    fctx_begin_fill(&fctx);
    fctx_set_fill_color(&fctx, g.colors[PaletteColorMarks]);
    fctx_plot_circle(&fctx, &sunPoint, g.sunDiscRadius);
    fctx_plot_circle(&fctx, &sunPoint, g.sunDiscRadius - g.strokeWidth);
    fctx_end_fill(&fctx);

    /* Fill the readout background. */
    fctx_begin_fill(&fctx);
    fctx_set_fill_color(&fctx, g.colors[PaletteColorWithin]);
    fctx_plot_circle(&fctx, &fcenter, g.readoutDiscRadius - g.strokeWidth / 2);
    fctx_end_fill(&fctx);

    /* Draw the bluetooth state. */
    fctx_set_rotation(&fctx, 0);
    fctx_set_offset(&fctx, fcenter);
    fctx_set_scale(&fctx, FPoint(1,1), FPoint(1,-1));

    fctx_begin_fill(&fctx);
    fctx_set_fill_color(&fctx, g.colors[PaletteColorEngraving]);
    drawBatteryDish(&fctx, 13);
    fctx_end_fill(&fctx);
    fctx_begin_fill(&fctx);
    fctx_set_fill_color(&fctx, g.colors[g.bluetooth ? PaletteColorOnline : PaletteColorOffline]);
    drawBatteryDish(&fctx, 12);
    fctx_end_fill(&fctx);

    if (!g.bluetooth) {
        GRect box;
        box.origin.x = center.x - 6;
        box.origin.y = center.y - FIXED_TO_INT(g.readoutDiscRadius) + 6;
        box.size.w = 12;
        box.size.h = 3;
        graphics_context_set_fill_color(fctx.gctx, g.colors[PaletteColorWithin]);
        graphics_fill_rect(fctx.gctx, box, 0, 0);
    }

    /* Draw the battery state. */
    fctx_set_scale(&fctx, FPointOne, FPointOne);
    fctx_begin_fill(&fctx);
    fctx_set_fill_color(&fctx, g.colors[PaletteColorEngraving]);
    drawBatteryDish(&fctx, 13);
    fctx_end_fill(&fctx);
    if (g.battery < 10) {
        fctx_begin_fill(&fctx);
        fctx_set_fill_color(&fctx, g.colors[PaletteColorCapacity]);
        drawBatteryDish(&fctx, 12);
        fctx_end_fill(&fctx);
    }
    fctx_begin_fill(&fctx);
    fctx_set_fill_color(&fctx, g.colors[PaletteColorCharge]);
    drawBatteryDish(&fctx, 2 + g.battery);
    fctx_end_fill(&fctx);

    /* Stroke around the readout perimeter. */
    fctx_begin_fill(&fctx);
    fctx_set_fill_color(&fctx, g.colors[PaletteColorMarks]);
    fctx_plot_circle(&fctx, &fcenter, g.readoutDiscRadius);
    fctx_plot_circle(&fctx, &fcenter, g.readoutDiscRadius - g.strokeWidth);
    fctx_end_fill(&fctx);

    FPoint p;
    fctx_begin_fill(&fctx);
    fctx_set_rotation(&fctx, 0);
    fctx_set_fill_color(&fctx, g.colors[PaletteColorText]);
    graphics_context_set_text_color(ctx, g.colors[PaletteColorText]);

    /* Draw the time string. */
    formatTime();
    p.x = fcenter.x;
    p.y = fcenter.y + INT_TO_FIXED(g.timeCapHeight / 2);
    fctx_set_offset(&fctx, p);
    fctx_set_text_cap_height(&fctx, g.font, g.timeCapHeight);
    fctx_draw_string(&fctx, g.strbuf, g.font, GTextAlignmentCenter, FTextAnchorBaseline);

    /* Draw the weekday text. */
    strftime(g.strbuf, ARRAY_LENGTH(g.strbuf), kWeekdayFormat, &g.gregorian);
    utf8_str_to_upper(g.strbuf);
    p.y = fcenter.y - INT_TO_FIXED(g.timeCapHeight / 2) - g.dateTextGap;
    fctx_set_offset(&fctx, p);
    if (g.dateFont) {
        gctx_draw_string(&fctx, g.strbuf, g.dateFont, GTextAlignmentCenter, FTextAnchorBaseline);
    } else {
        fctx_set_text_cap_height(&fctx, g.font, g.dateCapHeight);
        fctx_draw_string(&fctx, g.strbuf, g.font, GTextAlignmentCenter, FTextAnchorBaseline);
    }

    /* Draw the date text. */
    strftime(g.strbuf, ARRAY_LENGTH(g.strbuf), kDateFormat, &g.gregorian);
    utf8_str_to_upper(g.strbuf);
    p.y = fcenter.y + INT_TO_FIXED(g.timeCapHeight / 2) + g.dateTextGap;
    fctx_set_offset(&fctx, p);
    if (g.dateFont) {
        gctx_draw_string(&fctx, g.strbuf, g.dateFont, GTextAlignmentCenter, FTextAnchorCapTop);
    } else {
        fctx_set_text_cap_height(&fctx, g.font, g.dateCapHeight);
        fctx_draw_string(&fctx, g.strbuf, g.font, GTextAlignmentCenter, FTextAnchorCapTop);
    }

    fctx_end_fill(&fctx);

    fctx_deinit_context(&fctx);

}

static inline FPoint batteryPoint(int k, fixed_t side) {
    FPoint pt;
    pt.x = g.battery_x[k] * side;
    pt.y = g.readoutDiscRadius - g.strokeWidth / 2 - INT_TO_FIXED(k);
    return pt;
}

static void drawBatteryDish(FContext* fctx, int height) {
    int top = height - 1;
    int k = top;
    fctx_move_to(fctx, batteryPoint(k--, -1));
    while (k >= 0) {
        fctx_line_to(fctx, batteryPoint(k--, -1));
    }
    while (k <= top) {
        fctx_line_to(fctx, batteryPoint(k++, +1));
    }
    fctx_close_path(fctx);
}

// --------------------------------------------------------------------------

// --------------------------------------------------------------------------

static void timeChanged(struct tm* gregorian, TimeUnits unitsChanged) {
    g.gregorian = *gregorian;
    if (unitsChanged & MINUTE_UNIT) {
        layer_mark_dirty(g.layer);
    }
    if (unitsChanged & DAY_UNIT) {
        configureClock();
        animateClock();
    }
}

static void bluetoothConnected(bool connected) {
    g.bluetooth = connected;
    layer_mark_dirty(g.layer);
}

static void batteryStateChanged(BatteryChargeState charge) {
    g.battery = (charge.charge_percent + 5) / 10;
    layer_mark_dirty(g.layer);
}

// ---------------------------------------------------------------------------
//
// ---------------------------------------------------------------------------

static void messageReceived(DictionaryIterator* received, void* context) {

    Tuple* tuple;

    tuple = dict_find(received, MESSAGE_KEY_DATEFONT);
    if (tuple) {
        persist_write_int(PersistKeyDateFont, tuple->value->int32);
        applyDateFont(tuple->value->int32);
        layer_mark_dirty(g.layer);
    }

    tuple = dict_find(received, MESSAGE_KEY_PALETTE);
    if (tuple && TUPLE_BYTE_ARRAY == tuple->type) {
        int length = (tuple->length < PaletteSize) ? tuple->length : PaletteSize;
        persist_write_data(PersistKeyPalette, tuple->value->data, length);
        applyPalette(tuple->value->data, length);
        layer_mark_dirty(g.layer);
    }

    tuple = dict_find(received, MESSAGE_KEY_TIMEZONE);
    if (tuple) {
        g.location.timezone = tuple->value->int32;
        persist_write_int(PersistKeyTimezone,  g.location.timezone);
    }

    tuple = dict_find(received, MESSAGE_KEY_LONGITUDE);
    if (tuple) {
        g.location.longitude = tuple->value->int32;
        persist_write_int(PersistKeyLongitude, g.location.longitude);
    }

    tuple = dict_find(received, MESSAGE_KEY_LATITUDE);
    if (tuple) {
        g.location.latitude = tuple->value->int32;
        persist_write_int(PersistKeyLatitude,  g.location.latitude);
    }

    tuple = dict_find(received, MESSAGE_KEY_SUNRISE);
    if (tuple) {
        g.location.sunrise = tuple->value->int32;
        persist_write_int(PersistKeySunrise,   g.location.sunrise);
    }

    tuple = dict_find(received, MESSAGE_KEY_SUNSET);
    if (tuple) {
        g.location.sunset = tuple->value->int32;
        persist_write_int(PersistKeySunset,    g.location.sunset);
    }

    tuple = dict_find(received, MESSAGE_KEY_SUNSOUTH);
    if (tuple) {
        g.location.sunsouth = tuple->value->int32;
        persist_write_int(PersistKeySunSouth,  g.location.sunsouth);
    }

    tuple = dict_find(received, MESSAGE_KEY_SUNSTAT);
    if (tuple) {
        g.location.sunstat = tuple->value->int32;
        persist_write_int(PersistKeySunStatus, g.location.sunstat);
    }

    tuple = dict_find(received, MESSAGE_KEY_TIMESTAMP);
    if (tuple) {
        g.location.timestamp = tuple->value->int32;
        persist_write_int(PersistKeyTimestamp, g.location.timestamp);
        logLocationFix(&g.location);
        configureClock();
        animateClock();
    }

}

// --------------------------------------------------------------------------
// Utility functions.
// --------------------------------------------------------------------------

static inline FPoint clockPoint(FPoint center, fixed_t radius, uint32_t angle) {
    FPoint pt;
    int32_t c = cos_lookup(angle);
    int32_t s = sin_lookup(angle);
    pt.x = center.x - s * radius / TRIG_MAX_RATIO;
    pt.y = center.y + c * radius / TRIG_MAX_RATIO;
    return pt;
}

static inline GColor colorFromConfig(uint8_t cc) {
#ifdef PBL_BW
    if (cc == GColorBlackARGB8) return GColorBlack;
    if (cc == GColorWhiteARGB8) return GColorWhite;
    return GColorDarkGray;
#else
    GColor8 gc8;
    gc8.argb = cc;
    return gc8;
#endif
}

static void applyPalette(const uint8_t* palette, int16_t length) {
    int k;
    for (k = 0; k < length; ++k) {
        g.colors[k] = colorFromConfig(palette[k]);
    }
    for (; k < PaletteSize; ++k) {
        g.colors[k] = colorFromConfig(kDefaultPalette[k]);
    }
}

static void applyDateFont(int32_t dateFont) {
    if (dateFont == 1) {
        g.dateFont = g.gothicRegular;
    } else if (dateFont == 2) {
        g.dateFont = g.gothicBold;
    } else {
        g.dateFont = NULL;
    }
}

void logLocationFix(LocationFix* loc) {
#if 0
    int8_t tzhour = loc->timezone / 60;
    int8_t tzmin = loc->timezone % 60;
    char tzsign = '+';
    if (loc->timezone < 0) {
        tzsign = '-';
        tzhour *= -1;
    }

    int longdeg =  loc->longitude / TRIG_MAX_ANGLE;
    char longdir = 'E';
    if (longdeg < 0) {
        longdir = 'W';
        longdeg *= -1;
    }
    int latdeg = loc->latitude / TRIG_MAX_ANGLE;
    char latdir = 'N';
    if (latdeg < 0) {
        latdir = 'S';
        latdeg *= -1;
    }

    int8_t risehour = loc->sunrise / 60;
    int8_t risemin = loc->sunrise % 60;
    int8_t sethour = loc->sunset / 60;
    int8_t setmin = loc->sunset % 60;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "UTC%c%d:%02d %d%c x %d%c rise@%d:%02d set@%d:%02d",
        tzsign, tzhour, tzmin, longdeg, longdir, latdeg, latdir, risehour, risemin, sethour, setmin);

    if (clock_is_timezone_set()) {
        char tzname[TIMEZONE_NAME_LENGTH];
        clock_get_timezone(tzname, TIMEZONE_NAME_LENGTH);

        time_t timestamp = time(NULL);
        struct tm* tm = localtime(&timestamp);

        tzhour = tm->tm_gmtoff / 3600;
        tzmin = tm->tm_gmtoff % 3600;
        char tzsign = '+';
        if (tm->tm_gmtoff < 0) {
            tzsign = '-';
            tzhour *= -1;
        }

        APP_LOG(APP_LOG_LEVEL_DEBUG, "(%s Z%c%d:%02d DST:%d)",
            tzname, tzsign, tzhour, tzmin, tm->tm_isdst);
    }
#endif
}
