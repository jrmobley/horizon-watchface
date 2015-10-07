/*
 *
 */

#include "pebble.h"
#include "fctx/fctx.h"
#include "fctx/ffont.h"
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
#define DATE_FORMAT "%B %d"
#define WDAY_FORMAT "%A"

#ifdef PBL_ROUND

#define TIME_TEXT_SIZE 36
#define DATE_FONT_KEY  FONT_KEY_GOTHIC_18
#define DATE_TEXT_OFFSET 10
#define WDAY_TEXT_OFFSET -36
#define ORBIT_RADIUS   INT_TO_FIXED(75)
#define SOLAR_RADIUS   INT_TO_FIXED(10)
#define SOLAR_STROKE   INT_TO_FIXED(2)
#define PIP_RADIUS     (SOLAR_RADIUS / 4)
#define READOUT_RADIUS INT_TO_FIXED(62)
#define READOUT_STROKE INT_TO_FIXED(2)

#else // PBL_RECT

#define TIME_TEXT_SIZE 30
#define DATE_FONT_KEY  FONT_KEY_GOTHIC_14
#define DATE_TEXT_OFFSET 11
#define WDAY_TEXT_OFFSET -30
#define ORBIT_RADIUS   INT_TO_FIXED(61)
#define SOLAR_RADIUS   INT_TO_FIXED(8)
#define SOLAR_STROKE   INT_TO_FIXED(2)
#define PIP_RADIUS     (SOLAR_RADIUS / 4)
#define READOUT_RADIUS INT_TO_FIXED(50)
#define READOUT_STROKE INT_TO_FIXED(2)

#endif

// --------------------------------------------------------------------------
// globals
// --------------------------------------------------------------------------

/* keep these in sync with appinfo.json */
typedef enum AppKeys {
	AppKeyLocation = 100,
	AppKeyLongitude,
	AppKeyLatitude,
	AppKeyTimezone,
	AppKeyTimestamp,
	AppKeySunrise,
	AppKeySunset,
	AppKeySunSouth,
	AppKeySunStatus,
	AppKeyBluetoothAlert,
	AppKeyColorPalette,
} AppKeys;

enum Palette {
    COLOR_BEHIND,
    COLOR_BELOW,
    COLOR_ABOVE,
    COLOR_WITHIN,
    COLOR_MARKS,
    COLOR_TEXT,
    COLOR_SOLAR,
    PALETTE_SIZE,
};

static const uint8_t DEFAULT_PALETTE[PALETTE_SIZE] = {
    //AARRGGBB
    0b11101010,
    0b11011011,
    0b11111101,
    0b11111111,
    0b11000000,
    0b11000000,
    0b11111111,
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
    GColor colors[PALETTE_SIZE];

	// external state
	struct tm gregorian;
	LocationFix location;

	// computed state
	FSize size;
	FPoint origin;
	uint16_t horizon;
	int32_t kilter;
    char strbuf[32];

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
static void timeChanged(struct tm* tickTime, TimeUnits unitsChanged);
static void messageReceived(DictionaryIterator* iterator, void *context);

static inline FPoint clockPoint(fixed_t radius, uint32_t angle);
static inline GColor colorFromConfig(uint8_t cc);
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

    /* --- Restore state from persistant storage. --- */

    g.bluetoothAlert = 1;
    if (persist_exists(AppKeyColorPalette)) {
        uint8_t palette[PALETTE_SIZE];
        int length = persist_read_data(AppKeyColorPalette, palette, PALETTE_SIZE);
        applyPalette(palette, length);
    } else {
        applyPalette(DEFAULT_PALETTE, PALETTE_SIZE);
    }
    if (persist_exists(AppKeyTimestamp)) {
		g.location.timestamp = persist_read_int(AppKeyTimestamp);
		g.location.timezone  = persist_read_int(AppKeyTimezone);
		g.location.longitude = persist_read_int(AppKeyLongitude);
		g.location.latitude  = persist_read_int(AppKeyLatitude);
		g.location.sunrise   = persist_read_int(AppKeySunrise);
		g.location.sunset    = persist_read_int(AppKeySunset);
		g.location.sunsouth  = persist_read_int(AppKeySunSouth);
		g.location.sunstat   = persist_read_int(AppKeySunStatus);
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

    g.font = ffont_create_from_resource(RESOURCE_ID_DIGITS_FFONT);

    /* --- Initialize the clock state. --- */

	time_t now = time(NULL);
	g.gregorian = *localtime(&now);
	g.size.w = INT_TO_FIXED(frame.size.w);
	g.size.h = INT_TO_FIXED(frame.size.h);
	g.origin.x = g.size.w / 2;
	g.origin.y = g.size.h / 2;
	g.kilter = 0;

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
}

// --------------------------------------------------------------------------

// --------------------------------------------------------------------------

static void deinit() {
	tick_timer_service_unsubscribe();
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
    FPoint sunrisePoint = clockPoint(ORBIT_RADIUS, sunriseAngle + g.kilter);
	FPoint sunsetPoint = clockPoint(ORBIT_RADIUS, sunsetAngle + g.kilter);

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

// --------------------------------------------------------------------------

// --------------------------------------------------------------------------

void drawClock(Layer* layer, GContext* ctx) {

	GRect bounds = layer_get_bounds(layer);
	GRect fill = bounds;
    GPoint left;
    GPoint right;
    left.x = bounds.origin.x;
    right.x = bounds.origin.x + bounds.size.w;

    /* Fill the space behind. */
    graphics_context_set_fill_color(ctx, g.colors[COLOR_BEHIND]);
    graphics_fill_rect(ctx, fill, 0, GCornerNone);

	/* Fill the space above. */
    fill.origin.y = 0;
	fill.size.h = g.above.current;
    left.y = right.y = g.above.current - 1;
	graphics_context_set_fill_color(ctx, g.colors[COLOR_ABOVE]);
	graphics_fill_rect(ctx, fill, 0, GCornerNone);
    graphics_context_set_stroke_color(ctx, g.colors[COLOR_MARKS]);
    graphics_draw_line(ctx, left, right);

	/* Fill the space below. */
	fill.origin.y = g.below.current;
	fill.size.h = bounds.origin.y + bounds.size.h - fill.origin.y;
    left.y = right.y = g.below.current;
	graphics_context_set_fill_color(ctx, g.colors[COLOR_BELOW]);
	graphics_fill_rect(ctx, fill, 0, GCornerNone);
    graphics_draw_line(ctx, left, right);

	FContext fctx;
	fctx_init_context(&fctx, ctx);

	// draw the solar orbit as a ring of dots.
	fctx_set_fill_color(&fctx, g.colors[COLOR_MARKS]);
	fctx_set_color_bias(&fctx, 0);
	fctx_begin_fill(&fctx);
	fctx_set_text_size(&fctx, g.font, 20);
	fctx_set_rotation(&fctx, g.rotation.current);
	for (int h = 0; h < 24; ++h) {
        FPoint c = clockPoint(ORBIT_RADIUS, hourAngle(h) + g.rotation.current);

		if (h % 6) {
			fctx_plot_circle(&fctx, &c, PIP_RADIUS);
		} else {
#ifdef PBL_ROUND
			fctx_plot_circle(&fctx, &c, SOLAR_RADIUS);
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

	// draw the solar disc
	uint32_t minute = g.gregorian.tm_hour * 60 + g.gregorian.tm_min;
	FPoint sunPoint = clockPoint(ORBIT_RADIUS, minuteAngle(minute) + g.rotation.current);

	fctx_begin_fill(&fctx);
	fctx_set_fill_color(&fctx, g.colors[COLOR_SOLAR]);
	fctx_set_color_bias(&fctx, -3);
	fctx_plot_circle(&fctx, &sunPoint, SOLAR_RADIUS - SOLAR_STROKE / 2);
	fctx_end_fill(&fctx);
	fctx_set_color_bias(&fctx, 0);

	fctx_begin_fill(&fctx);
	fctx_set_fill_color(&fctx, g.colors[COLOR_MARKS]);
	fctx_plot_circle(&fctx, &sunPoint, SOLAR_RADIUS);
	fctx_plot_circle(&fctx, &sunPoint, SOLAR_RADIUS - SOLAR_STROKE);
	fctx_end_fill(&fctx);

    fctx_begin_fill(&fctx);
    fctx_set_fill_color(&fctx, g.colors[COLOR_MARKS]);
    fctx_plot_circle(&fctx, &g.origin, READOUT_RADIUS);
    fctx_end_fill(&fctx);

    fctx_begin_fill(&fctx);
    fctx_set_fill_color(&fctx, g.colors[COLOR_WITHIN]);
    fctx_plot_circle(&fctx, &g.origin, READOUT_RADIUS - READOUT_STROKE);
    fctx_end_fill(&fctx);

    clock_copy_time_string(g.strbuf, ARRAY_LENGTH(g.strbuf));

    fctx_begin_fill(&fctx);
    fctx_set_rotation(&fctx, 0);
    fctx_set_offset(&fctx, g.origin);
    fctx_set_text_size(&fctx, g.font, TIME_TEXT_SIZE);
    fctx_set_fill_color(&fctx, g.colors[COLOR_TEXT]);
    fctx_draw_string(&fctx, g.strbuf, g.font, GTextAlignmentCenter, FTextAnchorMiddle);
    fctx_end_fill(&fctx);

	fctx_deinit_context(&fctx);

    // draw the date text
    graphics_context_set_text_color(ctx, g.colors[COLOR_TEXT]);
    GFont font = fonts_get_system_font(DATE_FONT_KEY);
    GRect box = bounds;
    int16_t midline = FIXED_TO_INT(g.origin.y);
    box.origin.y = midline + DATE_TEXT_OFFSET;
    strftime(g.strbuf, ARRAY_LENGTH(g.strbuf), DATE_FORMAT, &g.gregorian);
    graphics_draw_text(ctx, g.strbuf, font, box, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

    // draw the weekday text
    box.origin.y = midline + WDAY_TEXT_OFFSET;
    strftime(g.strbuf, ARRAY_LENGTH(g.strbuf), WDAY_FORMAT, &g.gregorian);
    graphics_draw_text(ctx, g.strbuf, font, box, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
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

// ---------------------------------------------------------------------------
//
// ---------------------------------------------------------------------------

static void messageReceived(DictionaryIterator* iterator, void *context) {

	bool got_location = false;
	Tuple* tuple;
	for (tuple = dict_read_first(iterator); tuple; tuple = dict_read_next(iterator)) {
        if (AppKeyBluetoothAlert == tuple->key) {
            g.bluetoothAlert = (uint8_t)tuple->value->uint32;
            persist_write_int(AppKeyBluetoothAlert, g.bluetoothAlert);
            //bluetoothConnectionCallback(bluetooth_connection_service_peek());
        } else if (AppKeyColorPalette == tuple->key && TUPLE_BYTE_ARRAY == tuple->type) {
			int length = (tuple->length < PALETTE_SIZE) ? tuple->length : PALETTE_SIZE;
            persist_write_data(AppKeyColorPalette, tuple->value->data, length);
			applyPalette(tuple->value->data, length);
			layer_mark_dirty(g.layer);
        } else if (AppKeyTimestamp == tuple->key) {
			got_location = true;
			g.location.timestamp = tuple->value->int32;
			persist_write_int(AppKeyTimestamp, g.location.timestamp);
		} else if (AppKeyTimezone == tuple->key) {
			g.location.timezone = tuple->value->int32;
			persist_write_int(AppKeyTimezone,  g.location.timezone);
		} else if (AppKeyLongitude == tuple->key) {
			g.location.longitude = tuple->value->int32;
			persist_write_int(AppKeyLongitude, g.location.longitude);
		} else if (AppKeyLatitude == tuple->key) {
			g.location.latitude = tuple->value->int32;
			persist_write_int(AppKeyLatitude,  g.location.latitude);
		} else if (AppKeySunrise == tuple->key) {
			g.location.sunrise = tuple->value->int32;
			persist_write_int(AppKeySunrise,   g.location.sunrise);
		} else if (AppKeySunset == tuple->key) {
			g.location.sunset = tuple->value->int32;
			persist_write_int(AppKeySunset,    g.location.sunset);
		} else if (AppKeySunSouth == tuple->key) {
			g.location.sunsouth = tuple->value->int32;
			persist_write_int(AppKeySunSouth,  g.location.sunsouth);
		} else if (AppKeySunStatus == tuple->key) {
			g.location.sunstat = tuple->value->int32;
			persist_write_int(AppKeySunStatus, g.location.sunstat);
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
    for (int k = 0; k < length; ++k) {
        g.colors[k] = colorFromConfig(palette[k]);
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

    	APP_LOG(APP_LOG_LEVEL_DEBUG, "%s Z%c%d:%02d DST:%d",
    		tzname, tzsign, tzhour, tzmin, tm->tm_isdst);
    }
#endif
}
