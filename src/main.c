/*
 *
 */

#include <pebble.h>
#include <pebble-fctx/fctx.h>
#include <pebble-fctx/ffont.h>
#include <ctype.h>
#include "isqrt.h"
#include "debug.h"

// --------------------------------------------------------------------------
// constants
// --------------------------------------------------------------------------

/* An options message: 26 = 1 + 2 * 7  (overhead for a message with 2 tuples)
 *                     + 4 (int32_t bluetooth alert)
 *                     + 7 (uint8_t[7] color palette)
 * A location message: 89 = 1 + 8 * 7 (overhead for a message with 8 tuples)
 *                     + 8 * 4 (8x uint32_t values)
 */
#define MESSAGE_BUFFER_SIZE 100
#define CLOCK_ANIM_DURATION 1000
const char* kDateFormat = "%B %d";
const char* kWeekdayFormat = "%A";

#ifdef PBL_ROUND

const int16_t kTimeTextSize = 28;
const int16_t kDateTextSize = 13;
const int16_t kDateTextGap = 5;

const fixed_t kOrbitRadius = INT_TO_FIXED(73);
const fixed_t kSolarRadius = INT_TO_FIXED(10);
const fixed_t kSolarInset = INT_TO_FIXED(2);
const fixed_t kPipRadius = INT_TO_FIXED(10)/4;
const fixed_t kReadoutRadius = INT_TO_FIXED(60);
const fixed_t kReadoutInset = INT_TO_FIXED(2);
const int16_t kBatteryDishRadius = 59;
const int16_t kBluetoothRadius = 43;

#else // PBL_RECT

const int16_t kTimeTextSize = 24;
const int16_t kDateTextSize = 11;
const int16_t kDateTextGap = 4;

const fixed_t kOrbitRadius = INT_TO_FIXED(61);
const fixed_t kSolarRadius = INT_TO_FIXED(8);
const fixed_t kSolarInset = INT_TO_FIXED(2);
const fixed_t kPipRadius = INT_TO_FIXED(2);
const fixed_t kReadoutRadius = INT_TO_FIXED(50);
const fixed_t kReadoutInset = INT_TO_FIXED(2);
const int16_t kBatteryDishRadius = 49;
const int16_t kBluetoothRadius = 37;

#endif

/* This is the actual height of capital letters in the font,
 * expressed in em-square units.
 */
const int32_t kFontCapHeight = 712 * 72 / 1000;

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
} PersistKeys;

enum Palette {
    PaletteColorBehind,
    PaletteColorBelow,
    PaletteColorAbove,
    PaletteColorWithin,
    PaletteColorMarks,
    PaletteColorText,
    PaletteColorSolar,
    PaletteColorCapacity,
    PaletteColorCharge,
    PaletteSize,
};

static const uint8_t kDefaultPalette[PaletteSize] = {
    //AARRGGBB
    0b11101010, // Behind: 222 (Light Gray)
    0b11011011, // Below : 123 (Picton Blue)
    0b11111101, // Above : 331 (Icterine)
    0b11111111, // Within: 333 (White)
    0b11000000, // Marks : 000 (Black)
    0b11000000, // Text  : 000 (Black)
    0b11111111, // Solar : 333 (White)
    0b11010101, // Capacity: 111
    0b11101100, // Charge: 230 (Spring Bud)
};

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
    uint16_t from;
    uint16_t to;
    uint16_t current;
} uint16_anim_t;

typedef struct {
    uint32_t from;
    uint32_t to;
    uint32_t current;
} uint32_anim_t;

struct Clock {

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
    FSize size;
    FPoint origin;
    uint16_t horizon;
    int32_t kilter;
    char strbuf[32];
    fixed_t battery_x[14];

    // animated state
    uint16_anim_t above;
    uint16_anim_t below;
    uint32_anim_t rotation;

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

static void setCapHeight(FContext* fctx, int16_t pixels);
static FPoint clockPoint(fixed_t radius, uint32_t angle);
static GColor colorFromConfig(uint8_t cc);
static void applyPalette(const uint8_t* palette, int16_t length);

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

static inline void strbuf_to_upper() {
    for (char* p = g.strbuf; *p; ++p) {
        *p = (char)toupper((unsigned char)*p);
    }
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

    setlocale(LC_ALL, "");
    applyPalette(kDefaultPalette, PaletteSize);

    /* --- Restore state from persistent storage. --- */
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

    g.animationImplementation.setup = NULL;
    g.animationImplementation.update = &interpolateClock;
    g.animationImplementation.teardown = NULL;
    g.animation = animation_create();
    animation_set_implementation(g.animation, &g.animationImplementation);

    g.font = ffont_create_from_resource(RESOURCE_ID_DIN_CONDENSED_FFONT);

    /* --- Initialize the clock state. --- */

    int32_t r = kBatteryDishRadius;
    for (uint32_t k = 0; k < ARRAY_LENGTH(g.battery_x); ++k) {
        int32_t y = r - k;
        uint32_t q = usqrt(r*r - y*y);
        g.battery_x[k] = q / (SQRT_SCALE / FIXED_POINT_SCALE);
    }

    time_t now = time(NULL);
    g.gregorian = *localtime(&now);
    g.size.w = INT_TO_FIXED(frame.size.w);
    g.size.h = INT_TO_FIXED(frame.size.h);
    g.origin.x = g.size.w / 2;
    g.origin.y = g.size.h / 2;
    g.kilter = 0;

    g.bluetooth = bluetooth_connection_service_peek();

    batteryStateChanged(battery_state_service_peek());

    configureClock();

    if (0 == g.location.timestamp) {
        g.above.current = 0;
        g.below.current = frame.size.h;
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
        g.horizon = FIXED_TO_INT(g.origin.y);
        return;
    }

    /* Adjust the sun event times to integer minutes, local time. */
    int32_t timezone = g.location.timezone;
    int32_t sunRise = g.location.sunrise + timezone;
    int32_t sunSet = g.location.sunset + timezone;
    int32_t sunSouth = g.location.sunsouth + timezone;

    g.kilter = minuteAngle(12*60 - sunSouth);

    int32_t sunriseAngle = minuteAngle(sunRise);
    int32_t sunsetAngle = minuteAngle(sunSet);
    FPoint sunrisePoint = clockPoint(kOrbitRadius, sunriseAngle + g.kilter);
    FPoint sunsetPoint = clockPoint(kOrbitRadius, sunsetAngle + g.kilter);

    g.horizon = FIXED_TO_INT((sunrisePoint.y + sunsetPoint.y + FIX1) / 2);
}

// --------------------------------------------------------------------------

// --------------------------------------------------------------------------

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

    animation_set_duration(g.animation, CLOCK_ANIM_DURATION);
    animation_set_curve(g.animation, AnimationCurveEaseInOut);
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

    GRect bounds = layer_get_bounds(layer);
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
    fill.size.h = g.above.current;
    left.y = right.y = g.above.current - 1;
    graphics_context_set_fill_color(ctx, g.colors[PaletteColorAbove]);
    graphics_fill_rect(ctx, fill, 0, GCornerNone);
    graphics_context_set_stroke_color(ctx, g.colors[PaletteColorMarks]);
    graphics_draw_line(ctx, left, right);

    /* Fill the space below the horizon. */
    fill.origin.y = g.below.current;
    fill.size.h = bounds.origin.y + bounds.size.h - fill.origin.y;
    left.y = right.y = g.below.current;
    graphics_context_set_fill_color(ctx, g.colors[PaletteColorBelow]);
    graphics_fill_rect(ctx, fill, 0, GCornerNone);
    graphics_draw_line(ctx, left, right);

    FContext fctx;
    fctx_init_context(&fctx, ctx);

    /* Draw the solar orbit markings. */
    fctx_set_fill_color(&fctx, g.colors[PaletteColorMarks]);
    fctx_set_color_bias(&fctx, 0);
    fctx_begin_fill(&fctx);
    fctx_set_text_size(&fctx, g.font, 20);
    fctx_set_rotation(&fctx, g.rotation.current);
    for (int h = 0; h < 24; ++h) {
        FPoint c = clockPoint(kOrbitRadius, hourAngle(h) + g.rotation.current);

        if (h % 6) {
            fctx_plot_circle(&fctx, &c, kPipRadius);
        } else {
#ifdef PBL_ROUND
            fctx_plot_circle(&fctx, &c, kSolarRadius);
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
    FPoint sunPoint = clockPoint(kOrbitRadius, minuteAngle(minute) + g.rotation.current);

    /* Fill the solar disc. */
    fctx_begin_fill(&fctx);
    fctx_set_fill_color(&fctx, g.colors[PaletteColorSolar]);
    fctx_set_color_bias(&fctx, -3);
    fctx_plot_circle(&fctx, &sunPoint, kSolarRadius - kSolarInset / 2);
    fctx_end_fill(&fctx);
    fctx_set_color_bias(&fctx, 0);

    /* Stroke the solar disc perimeter. */
    fctx_begin_fill(&fctx);
    fctx_set_fill_color(&fctx, g.colors[PaletteColorMarks]);
    fctx_plot_circle(&fctx, &sunPoint, kSolarRadius);
    fctx_plot_circle(&fctx, &sunPoint, kSolarRadius - kSolarInset);
    fctx_end_fill(&fctx);

    /* Fill the readout background. */
    fctx_begin_fill(&fctx);
    fctx_set_fill_color(&fctx, g.colors[PaletteColorWithin]);
    fctx_plot_circle(&fctx, &g.origin, kReadoutRadius - kReadoutInset / 2);
    fctx_end_fill(&fctx);

    /* Draw the battery state. */
    fctx_set_rotation(&fctx, 0);
    fctx_set_offset(&fctx, g.origin);
    fctx_set_scale(&fctx, FPointOne, FPointOne);
    fctx_begin_fill(&fctx);
    /*
    fctx_set_fill_color(&fctx, g.colors[PaletteColorMarks]);
    drawBatteryDish(&fctx, 14);
    fctx_end_fill(&fctx);
    */
    fctx_set_fill_color(&fctx, g.colors[PaletteColorCapacity]);
    drawBatteryDish(&fctx, 12);
    fctx_end_fill(&fctx);
    fctx_begin_fill(&fctx);
    fctx_set_fill_color(&fctx, g.colors[PaletteColorCharge]);
    drawBatteryDish(&fctx, 2 + g.battery);
    fctx_end_fill(&fctx);

    /* Stroke around the readout perimeter. */
    fctx_begin_fill(&fctx);
    fctx_set_fill_color(&fctx, g.colors[PaletteColorMarks]);
    fctx_plot_circle(&fctx, &g.origin, kReadoutRadius);
    fctx_plot_circle(&fctx, &g.origin, kReadoutRadius - kReadoutInset);
    fctx_end_fill(&fctx);

    FPoint p;
    fctx_begin_fill(&fctx);
    fctx_set_rotation(&fctx, 0);
    fctx_set_fill_color(&fctx, g.colors[PaletteColorText]);

    /* Draw the time string. */
    formatTime();
    p.x = g.origin.x;
    p.y = g.origin.y + INT_TO_FIXED(kTimeTextSize / 2);
    fctx_set_offset(&fctx, p);
    setCapHeight(&fctx, kTimeTextSize);
    fctx_draw_string(&fctx, g.strbuf, g.font, GTextAlignmentCenter, FTextAnchorBaseline);

    /* Draw the weekday text. */
    p.y = g.origin.y - INT_TO_FIXED(kTimeTextSize / 2 + kDateTextGap);
    fctx_set_offset(&fctx, p);
    strftime(g.strbuf, ARRAY_LENGTH(g.strbuf), kWeekdayFormat, &g.gregorian);
    strbuf_to_upper();
    setCapHeight(&fctx, kDateTextSize);
    fctx_draw_string(&fctx, g.strbuf, g.font, GTextAlignmentCenter, FTextAnchorBaseline);

    /* Draw the date text. */
    p.y = g.origin.y + INT_TO_FIXED(kTimeTextSize / 2 + kDateTextGap + kDateTextSize);
    fctx_set_offset(&fctx, p);
    strftime(g.strbuf, ARRAY_LENGTH(g.strbuf), kDateFormat, &g.gregorian);
    strbuf_to_upper();
    setCapHeight(&fctx, kDateTextSize);
    fctx_draw_string(&fctx, g.strbuf, g.font, GTextAlignmentCenter, FTextAnchorBaseline);

    fctx_end_fill(&fctx);

    fctx_deinit_context(&fctx);

    /* Draw the bluetooth state. */
    if (g.bluetooth == false) {
        GPoint c = grect_center_point(&bounds);
        c.y -= kBluetoothRadius;
        int l = c.x - 3;
        int r = c.x + 3;
        int t = c.y - 3;
        int b = c.y + 3;
        graphics_context_set_stroke_color(ctx, g.colors[PaletteColorText]);
        graphics_context_set_stroke_width(ctx, 1);
        graphics_context_set_antialiased(ctx, true);
        graphics_draw_line(ctx, GPoint(l, t), GPoint(r, b));
        graphics_draw_line(ctx, GPoint(l, b), GPoint(r, t));
        graphics_draw_line(ctx, GPoint(c.x, c.y - 7), GPoint(c.x, c.y + 7));
    }

}

static inline FPoint batteryPoint(int k, fixed_t side) {
    FPoint pt;
    pt.x = g.battery_x[k] * side;
    pt.y = INT_TO_FIXED(kBatteryDishRadius - k);
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

static void messageReceived(DictionaryIterator* iterator, void *context) {

    bool got_location = false;
    Tuple* tuple;
    for (tuple = dict_read_first(iterator); tuple; tuple = dict_read_next(iterator)) {
        if (MESSAGE_KEY_TIMESTAMP == tuple->key) {
            got_location = true;
            g.location.timestamp = tuple->value->int32;
            persist_write_int(PersistKeyTimestamp, g.location.timestamp);

        } else if (MESSAGE_KEY_TIMEZONE == tuple->key) {
            g.location.timezone = tuple->value->int32;
            persist_write_int(PersistKeyTimezone,  g.location.timezone);

        } else if (MESSAGE_KEY_LONGITUDE == tuple->key) {
            g.location.longitude = tuple->value->int32;
            persist_write_int(PersistKeyLongitude, g.location.longitude);

        } else if (MESSAGE_KEY_LATITUDE == tuple->key) {
            g.location.latitude = tuple->value->int32;
            persist_write_int(PersistKeyLatitude,  g.location.latitude);

        } else if (MESSAGE_KEY_SUNRISE == tuple->key) {
            g.location.sunrise = tuple->value->int32;
            persist_write_int(PersistKeySunrise,   g.location.sunrise);

        } else if (MESSAGE_KEY_SUNSET == tuple->key) {
            g.location.sunset = tuple->value->int32;
            persist_write_int(PersistKeySunset,    g.location.sunset);

        } else if (MESSAGE_KEY_SUNSOUTH == tuple->key) {
            g.location.sunsouth = tuple->value->int32;
            persist_write_int(PersistKeySunSouth,  g.location.sunsouth);

        } else if (MESSAGE_KEY_SUNSTAT == tuple->key) {
            g.location.sunstat = tuple->value->int32;
            persist_write_int(PersistKeySunStatus, g.location.sunstat);
        }
    }

    if (got_location) {
        logLocationFix(&g.location);
        configureClock();
        animateClock();
    }
}

// --------------------------------------------------------------------------
// Utility functions.
// --------------------------------------------------------------------------

/*
 * Text size is usually calculated by mapping the em-square to the desired
 * pixel size.  We want to map the actual cap-height to a target pixel size
 * instead.
 */
static void setCapHeight(FContext* fctx, int16_t pixels) {
    fctx->transform_scale_from.x = kFontCapHeight;
    fctx->transform_scale_from.y = -fctx->transform_scale_from.x;
    fctx->transform_scale_to.x = pixels;
    fctx->transform_scale_to.y = pixels;
}

static inline FPoint clockPoint(fixed_t radius, uint32_t angle) {
    FPoint pt;
    int32_t c = cos_lookup(angle);
    int32_t s = sin_lookup(angle);
    pt.x = g.origin.x - s * radius / TRIG_MAX_RATIO;
    pt.y = g.origin.y + c * radius / TRIG_MAX_RATIO;
    return pt;
}

static inline GColor colorFromConfig(uint8_t cc) {
#ifdef PBL_COLOR
    GColor8 gc8;
    gc8.argb = cc;
    return gc8;
#else
    if ((cc & 0x3f) == 0) {
        return GColorBlack;
    }
    return GColorWhite;
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

void logLocationFix(LocationFix* loc) {
#if 1
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

        APP_LOG(APP_LOG_LEVEL_DEBUG, "%s Z%c%d:%02d DST:%d",
            tzname, tzsign, tzhour, tzmin, tm->tm_isdst);
    }
#endif
}
