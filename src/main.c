/*
 *
 */

#include "pebble.h"
#include "fctx/fctx.h"
#include "fctx/ffont.h"
#include "debug.h"

// --------------------------------------------------------------------------
// globals
// --------------------------------------------------------------------------

#ifdef PBL_ROUND

#define TIME_TEXT_SIZE 36
#define DATE_FONT_KEY  FONT_KEY_GOTHIC_18_BOLD
#define ORBIT_RADIUS   INT_TO_FIXED(75)
#define SOLAR_RADIUS   INT_TO_FIXED(10)
#define SOLAR_STROKE   INT_TO_FIXED(2)
#define PIP_RADIUS     (SOLAR_RADIUS / 4)
#define READOUT_RADIUS INT_TO_FIXED(62)
#define READOUT_STROKE INT_TO_FIXED(2)


#else

#define TIME_TEXT_SIZE 32
#define DATE_FONT_KEY  FONT_KEY_GOTHIC_18
#define ORBIT_RADIUS   INT_TO_FIXED(61)
#define SOLAR_RADIUS   INT_TO_FIXED(8)
#define SOLAR_STROKE   INT_TO_FIXED(2)
#define PIP_RADIUS     (SOLAR_RADIUS / 4)
#define READOUT_RADIUS INT_TO_FIXED(51)
#define READOUT_STROKE INT_TO_FIXED(2)

#endif

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
    0b11111111,
    0b11011011,
    0b11111101,
    0b11111111,
    0b11000000,
    0b11000000,
    0b01111111,
};

typedef struct LocationFix {
    int32_t timestamp;
    int32_t timezone;
    int32_t longitude;
	int32_t latitude;
	int32_t sunrise;
	int32_t sunset;
	int32_t sunsouth;
	int32_t sunstat;
} LocationFix;

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
	fixed_t horizon;
	int32_t kilter;
    char strbuf[32];

	Window* window;
	Layer* layer;
	FFont* font;

} g;

void location_service_init();
static void applyPalette(const uint8_t* palette, int16_t length);
void initClock(GRect bounds);
void configureClock();
void drawClock(Layer* layer, GContext* ctx);
static void timeChanged(struct tm* tickTime, TimeUnits unitsChanged);

// --------------------------------------------------------------------------
// init
// --------------------------------------------------------------------------

static void init() {

    if (persist_exists(AppKeyColorPalette)) {
        uint8_t palette[PALETTE_SIZE];
        int length = persist_read_data(AppKeyColorPalette, palette, PALETTE_SIZE);
        applyPalette(palette, length);
    } else {
        applyPalette(DEFAULT_PALETTE, PALETTE_SIZE);
    }

	g.bluetoothAlert = 1;

    setlocale(LC_ALL, "");

    g.font = ffont_create_from_resource(RESOURCE_ID_DIGITS_FFONT);
    if (g.font) {
    	ffont_debug_log(g.font);
    }
    location_service_init();

    g.window = window_create();
    window_set_background_color(g.window, GColorBlack);
    window_stack_push(g.window, true);
    Layer* windowLayer = window_get_root_layer(g.window);
    GRect windowFrame = layer_get_frame(windowLayer);

    g.layer = layer_create(windowFrame);
    layer_set_update_proc(g.layer, &drawClock);
    layer_add_child(windowLayer, g.layer);
	initClock(windowFrame);

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

int main() {
    init();
    app_event_loop();
    deinit();
}

// --------------------------------------------------------------------------

// --------------------------------------------------------------------------

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

static void timeChanged(struct tm* gregorian, TimeUnits unitsChanged) {
	g.gregorian = *gregorian;
    if (unitsChanged & MINUTE_UNIT) {
		layer_mark_dirty(g.layer);
    }
    if (unitsChanged & DAY_UNIT) {
        configureClock();
		layer_mark_dirty(g.layer);
    }
}

void initClock(GRect bounds) {

	struct Clock* clock = &g;

	// external state
	time_t now = time(NULL);
	clock->gregorian = *localtime(&now);

	// computed state
	clock->size.w = INT_TO_FIXED(bounds.size.w);
	clock->size.h = INT_TO_FIXED(bounds.size.h);
	clock->origin.x = clock->size.w / 2;
	clock->origin.y = clock->size.h / 2;
	clock->kilter = 0;

	configureClock();
}

static inline FPoint clockPoint(fixed_t radius, uint32_t angle) {
	FPoint pt;
	angle += g.kilter;
	int32_t c = cos_lookup(angle);
	int32_t s = sin_lookup(angle);
	pt.x = g.origin.x - s * radius / TRIG_MAX_RATIO;
	pt.y = g.origin.y + c * radius / TRIG_MAX_RATIO;
	return pt;
}

/* Convert hours to angle, measured clockwise from midnight. */
static inline int32_t hourAngle(int32_t hour) {
	return hour * TRIG_MAX_ANGLE / 24;
}

/* Convert minutes to angle, measured clockwise from midnight. */
static inline int32_t minuteAngle(int32_t minute) {
	return minute * TRIG_MAX_ANGLE / (24*60);
}

void configureClock() {

	struct Clock* clock = &g;

	if (0 == clock->location.timestamp) {
		clock->kilter = 0;
		clock->horizon = clock->size.h;
		return;
	}

	/* Adjust the sun event times to integer minutes, local time. */
	int32_t timezone = clock->location.timezone;
	int32_t sunRise = clock->location.sunrise + timezone;
	int32_t sunSet = clock->location.sunset + timezone;
	int32_t sunSouth = clock->location.sunsouth + timezone;

	int32_t sunriseAngle = minuteAngle(sunRise);
	int32_t sunsetAngle = minuteAngle(sunSet);
    FPoint sunrisePoint = clockPoint(ORBIT_RADIUS, sunriseAngle);
	FPoint sunsetPoint = clockPoint(ORBIT_RADIUS, sunsetAngle);

	clock->kilter = minuteAngle(12*60 - sunSouth);
	clock->horizon = (sunrisePoint.y + sunsetPoint.y) / 2;
}

void drawClock(Layer* layer, GContext* ctx) {
	struct Clock* clock = &g;

	GRect bounds = layer_get_bounds(layer);
	GRect fill = bounds;
	int16_t h = FIXED_TO_INT(clock->horizon);

	// draw the sky
	fill.size.h = h - bounds.origin.y;
	graphics_context_set_fill_color(ctx, clock->colors[COLOR_ABOVE]);
	graphics_fill_rect(ctx, fill, 0, GCornerNone);

	// draw the ground
	fill.origin.y = h;
	fill.size.h = bounds.origin.y + bounds.size.h - fill.origin.y;
	graphics_context_set_fill_color(ctx, clock->colors[COLOR_BELOW]);
	graphics_fill_rect(ctx, fill, 0, GCornerNone);

	// draw the horizon line
    fill.origin.y = h;
    fill.size.h = 2;
	graphics_context_set_fill_color(ctx, clock->colors[COLOR_MARKS]);
    graphics_fill_rect(ctx, fill, 0, GCornerNone);

	FContext fctx;
	fctx_init_context(&fctx, ctx);

	// draw the solar orbit as a ring of dots.
	fctx_set_fill_color(&fctx, clock->colors[COLOR_MARKS]);
	fctx_set_color_bias(&fctx, 0);
	fctx_begin_fill(&fctx);
	fctx_set_text_size(&fctx, g.font, 20);
	fctx_set_rotation(&fctx, clock->kilter);
	for (int h = 0; h < 24; ++h) {
        FPoint c = clockPoint(ORBIT_RADIUS, hourAngle(h));

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
	uint32_t minute = clock->gregorian.tm_hour * 60 + clock->gregorian.tm_min;
	FPoint sunPoint = clockPoint(ORBIT_RADIUS, minuteAngle(minute));

	fctx_begin_fill(&fctx);
	fctx_set_fill_color(&fctx, clock->colors[COLOR_SOLAR]);
	fctx_set_color_bias(&fctx, -3);
	fctx_plot_circle(&fctx, &sunPoint, SOLAR_RADIUS - SOLAR_STROKE / 2);
	fctx_end_fill(&fctx);
	fctx_set_color_bias(&fctx, 0);

	fctx_begin_fill(&fctx);
	fctx_set_fill_color(&fctx, clock->colors[COLOR_MARKS]);
	fctx_plot_circle(&fctx, &sunPoint, SOLAR_RADIUS);
	fctx_plot_circle(&fctx, &sunPoint, SOLAR_RADIUS - SOLAR_STROKE);
	fctx_end_fill(&fctx);

    fctx_begin_fill(&fctx);
    fctx_set_fill_color(&fctx, clock->colors[COLOR_MARKS]);
    fctx_plot_circle(&fctx, &clock->origin, READOUT_RADIUS);
    fctx_end_fill(&fctx);

    fctx_begin_fill(&fctx);
    fctx_set_fill_color(&fctx, clock->colors[COLOR_WITHIN]);
    fctx_plot_circle(&fctx, &clock->origin, READOUT_RADIUS - READOUT_STROKE);
    fctx_end_fill(&fctx);

    clock_copy_time_string(clock->strbuf, ARRAY_LENGTH(clock->strbuf));

    fctx_begin_fill(&fctx);
    fctx_set_rotation(&fctx, 0);
    fctx_set_offset(&fctx, clock->origin);
    fctx_set_text_size(&fctx, g.font, TIME_TEXT_SIZE);
    fctx_set_fill_color(&fctx, clock->colors[COLOR_TEXT]);
    fctx_draw_string(&fctx, clock->strbuf, g.font, GTextAlignmentCenter, FTextAnchorMiddle);
    fctx_end_fill(&fctx);

	fctx_deinit_context(&fctx);

    // draw the date text
    graphics_context_set_text_color(ctx, clock->colors[COLOR_TEXT]);
    GFont font = fonts_get_system_font(DATE_FONT_KEY);
    GRect box = bounds;
    h = FIXED_TO_INT(clock->origin.y);
    box.origin.y = h + 10;
    strftime(clock->strbuf, ARRAY_LENGTH(clock->strbuf), "%B %d", &clock->gregorian);
    graphics_draw_text(ctx, clock->strbuf, font, box, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

    // draw the weekday text
    box.origin.y = h - 36;
    strftime(clock->strbuf, ARRAY_LENGTH(clock->strbuf), "%A", &clock->gregorian);
    graphics_draw_text(ctx, clock->strbuf, font, box, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

// ---------------------------------------------------------------------------
//
// ---------------------------------------------------------------------------

void logLocationFix(LocationFix* loc) {

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
}

void appmsg_inbox_received(DictionaryIterator* iterator, void *context) {

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
		layer_mark_dirty(g.layer);
	}

}

void appmsg_inbox_dropped(AppMessageResult reason, void *context) {
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "appmsg_inbox_dropped %d", (int)reason);
}

void appmsg_outbox_sent(DictionaryIterator *iterator, void *context) {
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "appmsg_outbox_sent");
}

void appmsg_outbox_failed(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "appmsg_outbox_failed %d", (int)reason);
}

void location_service_init() {

	if (false && persist_exists(AppKeyTimestamp)) {
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

	app_message_register_inbox_received(appmsg_inbox_received);
	app_message_register_inbox_dropped(appmsg_inbox_dropped);
	app_message_register_outbox_sent(appmsg_outbox_sent);
	app_message_register_outbox_failed(appmsg_outbox_failed);

	app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}
