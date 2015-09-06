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

/* keep these in sync with appinfo.json */
typedef enum AppMessageKeys {
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

    COLOR_CANVAS = 0,
    COLOR_BELOW = 1,
    COLOR_ABOVE = 2,
    COLOR_MARKS = 3,
    COLOR_SOLAR = 4,
    PALETTE_SIZE = 5,

} AppMessageKeys;

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
    uint8_t colorPalette[PALETTE_SIZE];
	GColor colorBelow;
	GColor colorAbove;
	GColor orbitColor;
	GColor solarColor;
	fixed_t outerLimit;
	fixed_t orbitRadius;
	fixed_t solarRadius;
	fixed_t solarStroke;

	// external state
	struct tm gregorian;
	LocationFix location;

	// computed state
	FPoint center;
	FPoint origin;
	fixed_t horizon;
	int32_t kilter;

	Window* window;
	Layer* layer;
	FFont* font;

} g;

void location_service_init();
static void applyPalette();
void initClock();
void configureClock();
void drawClock(Layer* layer, GContext* ctx);
static void timeChanged(struct tm* tickTime, TimeUnits unitsChanged);

// --------------------------------------------------------------------------
// init
// --------------------------------------------------------------------------

static void init() {

	g.bluetoothAlert = 1;
	g.colorPalette[COLOR_CANVAS] = 0xc0;
	g.colorPalette[COLOR_BELOW]  = 0x00;
	g.colorPalette[COLOR_ABOVE]  = 0x00;
	g.colorPalette[COLOR_MARKS]  = 0xff;
	g.colorPalette[COLOR_SOLAR]  = 0xff;

    setlocale(LC_ALL, "");

    g.font = ffont_create_from_resource(RESOURCE_ID_DIN_FFONT);
    if (g.font) {
    	ffont_debug_log(g.font);
    }
    location_service_init();
    initClock();

    g.window = window_create();
    window_set_background_color(g.window, GColorBlack);
    window_stack_push(g.window, true);
    Layer* windowLayer = window_get_root_layer(g.window);
    GRect windowFrame = layer_get_frame(windowLayer);

    g.layer = layer_create(windowFrame);
    layer_set_update_proc(g.layer, &drawClock);
    layer_add_child(windowLayer, g.layer);

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

static void applyPalette() {
	g.colorBelow = colorFromConfig(g.colorPalette[COLOR_BELOW]);
	g.colorAbove = colorFromConfig(g.colorPalette[COLOR_ABOVE]);
	g.orbitColor = colorFromConfig(g.colorPalette[COLOR_MARKS]);;
	g.solarColor = colorFromConfig(g.colorPalette[COLOR_SOLAR]);;
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

void initClock() {

	struct Clock* clock = &g;

	// configuration
	clock->colorBelow = COLOR_FALLBACK(GColorPictonBlue, GColorBlack);
	clock->colorAbove = COLOR_FALLBACK(GColorIcterine, GColorBlack);
	clock->orbitColor = COLOR_FALLBACK(GColorBlack, GColorWhite);
	clock->solarColor = COLOR_FALLBACK(GColorWhite, GColorWhite);
	applyPalette();
	clock->outerLimit = INT_TO_FIXED(71);
	clock->solarRadius = INT_TO_FIXED(8);
	clock->solarStroke = INT_TO_FIXED(2);
	clock->orbitRadius = clock->outerLimit - clock->solarRadius;

	// external state
	time_t now = time(NULL);
	clock->gregorian = *localtime(&now);

	// computed state
	clock->center = FPointI(144 / 2, 168 / 2);
	clock->origin = clock->center;
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
		clock->origin = clock->center;
		clock->kilter = 0;
		clock->horizon = INT_TO_FIXED(168);
		return;
	}

	/* Adjust the sun event times to integer minutes, local time. */
	int32_t timezone = clock->location.timezone;
	int32_t sunRise = clock->location.sunrise + timezone;
	int32_t sunSet = clock->location.sunset + timezone;
	int32_t sunSouth = clock->location.sunsouth + timezone;
	int32_t sunriseAngle = minuteAngle(sunRise);
	int32_t sunsetAngle = minuteAngle(sunSet);

	clock->kilter = minuteAngle(12*60 - sunSouth);

	clock->origin = clock->center;
	FPoint sunrisePoint  = clockPoint(clock->orbitRadius, sunriseAngle);
	FPoint sunsetPoint   = clockPoint(clock->orbitRadius, sunsetAngle);

	// calculate the horizon line relative to the origin.
	fixed_t horizon = (sunrisePoint.y + sunsetPoint.y) / 2;

	// calculate the limit of how far the orbit can be shifted.
	fixed_t limit = clock->center.y - clock->outerLimit - INT_TO_FIXED(4);

	// shift the orbit within the limit and shift the horizon by the excess.
	fixed_t offset = horizon - clock->center.y;
	fixed_t excess = 0;
	if (offset > limit) {
		excess = offset - limit;
		offset = limit;
	} else if (offset < -limit) {
		excess = offset + limit;
		offset = -limit;
	}
	clock->origin.y = clock->center.y - offset;
	clock->horizon = clock->center.y + excess;
}

void drawClock(Layer* layer, GContext* ctx) {
	struct Clock* clock = &g;

	GRect bounds = layer_get_bounds(layer);
	GRect fill = bounds;
	int16_t h = FIXED_TO_INT(clock->horizon);

	// draw the sky
	fill.size.h = h - bounds.origin.y;
	graphics_context_set_fill_color(ctx, clock->colorAbove);
	graphics_fill_rect(ctx, fill, 0, GCornerNone);

	// draw the ground
	fill.origin.y = h;
	fill.size.h = bounds.origin.y + bounds.size.h - fill.origin.y;
	graphics_context_set_fill_color(ctx, clock->colorBelow);
	graphics_fill_rect(ctx, fill, 0, GCornerNone);

	// draw the horizon line
	graphics_context_set_stroke_color(ctx, COLOR_FALLBACK(GColorDarkGray, GColorWhite));
	graphics_draw_line(ctx, GPoint(0, h), GPoint(144,h));

	FContext fctx;
	fctx_init_context(&fctx, ctx);

	// draw the solar orbit as a ring of dots.
	fctx_set_fill_color(&fctx, clock->orbitColor);
	fctx_set_color_bias(&fctx, 0);
	fctx_begin_fill(&fctx);
	fctx_set_text_size(&fctx, g.font, 20);
	fctx_set_rotation(&fctx, clock->kilter);
	for (int h = 0; h < 24; ++h) {

		if (h % 6) {
			FPoint c = clockPoint(clock->orbitRadius, hourAngle(h));
			fctx_plot_circle(&fctx, &c, clock->solarRadius / 4);
		} else {
			fixed_t r = clock->orbitRadius /*+ INT_TO_FIXED(3)*/;
			fctx.offset = clockPoint(r, hourAngle(h));
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
	FPoint sunPoint = clockPoint(clock->orbitRadius, minuteAngle(minute));

	fctx_begin_fill(&fctx);
	fctx_set_fill_color(&fctx, clock->solarColor);
	fctx_set_color_bias(&fctx, -3);
	fctx_plot_circle(&fctx, &sunPoint, clock->solarRadius - clock->solarStroke / 2);
	fctx_end_fill(&fctx);
	fctx_set_color_bias(&fctx, 0);

	fctx_begin_fill(&fctx);
	fctx_set_fill_color(&fctx, clock->orbitColor);
	fctx_plot_circle(&fctx, &sunPoint, clock->solarRadius);
	fctx_plot_circle(&fctx, &sunPoint, clock->solarRadius - clock-> solarStroke);
	fctx_end_fill(&fctx);

	fctx_deinit_context(&fctx);
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

void appmsg_inbox_received(DictionaryIterator *iterator, void *context) {

	Tuple* tuple;
	for (tuple = dict_read_first(iterator); tuple; tuple = dict_read_next(iterator)) {
        if (AppKeyBluetoothAlert == tuple->key) {
            g.bluetoothAlert = (uint8_t)tuple->value->uint32;
            persist_write_int(AppKeyBluetoothAlert, g.bluetoothAlert);
            APP_LOG(APP_LOG_LEVEL_DEBUG, "bluetooth alert <- %d", g.bluetoothAlert);
            //bluetoothConnectionCallback(bluetooth_connection_service_peek());
        } else if (AppKeyColorPalette == tuple->key && TUPLE_BYTE_ARRAY == tuple->type) {
			APP_LOG(APP_LOG_LEVEL_DEBUG, "palette <- data[%d]", tuple->length);
			uint16_t length = (tuple->length < PALETTE_SIZE) ? tuple->length : PALETTE_SIZE;
			APP_LOG(APP_LOG_LEVEL_DEBUG, "  copy %d bytes", length);
			memcpy(g.colorPalette, tuple->value->data, length);
			APP_LOG(APP_LOG_LEVEL_DEBUG, "  persist palette");
            persist_write_data(AppKeyColorPalette, g.colorPalette, PALETTE_SIZE);
			applyPalette();
			layer_mark_dirty(g.layer);
        } else if (AppKeyTimestamp == tuple->key) {
			g.location.timestamp = tuple->value->int32;
		} else if (AppKeyTimezone == tuple->key) {
			g.location.timezone = tuple->value->int32;
		} else if (AppKeyLongitude == tuple->key) {
			g.location.longitude = tuple->value->int32;
		} else if (AppKeyLatitude == tuple->key) {
			g.location.latitude = tuple->value->int32;
		} else if (AppKeySunrise == tuple->key) {
			g.location.sunrise = tuple->value->int32;
		} else if (AppKeySunset == tuple->key) {
			g.location.sunset = tuple->value->int32;
		} else if (AppKeySunSouth == tuple->key) {
			g.location.sunsouth = tuple->value->int32;
		} else if (AppKeySunStatus == tuple->key) {
			g.location.sunstat = tuple->value->int32;
		}
	}
	APP_LOG(APP_LOG_LEVEL_DEBUG, "persist <- location <- appmsg");
	persist_write_int(AppKeyTimestamp, g.location.timestamp);
	persist_write_int(AppKeyTimezone,  g.location.timezone);
	persist_write_int(AppKeyLongitude, g.location.longitude);
	persist_write_int(AppKeyLatitude,  g.location.latitude);
	persist_write_int(AppKeySunrise,   g.location.sunrise);
	persist_write_int(AppKeySunset,    g.location.sunset);
	persist_write_int(AppKeySunSouth,  g.location.sunsouth);
	persist_write_int(AppKeySunStatus, g.location.sunstat);
	logLocationFix(&g.location);

	configureClock();
	layer_mark_dirty(g.layer);
}

void appmsg_inbox_dropped(AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "appmsg_inbox_dropped %d", (int)reason);
}

void appmsg_outbox_sent(DictionaryIterator *iterator, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "appmsg_outbox_sent");
}

void appmsg_outbox_failed(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "appmsg_outbox_failed %d", (int)reason);
}

void location_service_init() {

	if (false && persist_exists(AppKeyTimestamp)) {
		g.location.timestamp = persist_read_int(AppKeyTimestamp);
		g.location.timezone = persist_read_int(AppKeyTimezone);
		g.location.longitude = persist_read_int(AppKeyLongitude);
		g.location.latitude  = persist_read_int(AppKeyLatitude);
		g.location.sunrise   = persist_read_int(AppKeySunrise);
		g.location.sunset    = persist_read_int(AppKeySunset);
		g.location.sunsouth  = persist_read_int(AppKeySunSouth);
		g.location.sunstat   = persist_read_int(AppKeySunStatus);
		APP_LOG(APP_LOG_LEVEL_DEBUG, "location <- persist");
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
