/*jslint
    eqeq: true,
    vars: true,
    unparam: true
*/
/*global
    Pebble: false,
    window: false,
    console: false,
    navigator: false,
    XMLHttpRequest: false
*/

var _ = require('underscore');
var sunriset = require('./sunriset.js');
var colors = require('./colors.js');
var keys = require('message_keys');
var Clay = require('pebble-clay');
var clayConfig = require('./config.js');
var clayCustomFunc = require('./custom-clay.js');
var Preview = require('pebble-clay-preview-component');
var previewComponent = new Preview(require('raw!./preview.svg'), require('raw!./preview.css')); 

/* load custom presets from localStorage */
var customColorPresets = retrieveObject('custom-colors');

/* update clayConfig with custom presets */

var clay = new Clay(clayConfig, clayCustomFunc, { autoHandleEvents: false });
clay.registerComponent(previewComponent);

function locationMessage(pos) {
    'use strict';
    var coordinates = pos.coords,
        now = new Date(),
        sun = sunriset.sun_rise_set(now, coordinates.longitude, coordinates.latitude),
        message = {
            'LATITUDE': coordinates.latitude * 0x10000,
            'LONGITUDE': coordinates.longitude * 0x10000,
            'TIMEZONE': pos.timezone,
            'TIMESTAMP': pos.timestamp / 1000,
            'SUNRISE': sun.rise * 60,
            'SUNSET': sun.set * 60,
            'SUNSOUTH': sun.south * 60,
            'SUNSTAT': sun.status
        };
    //console.log('location: ' + JSON.stringify(pos, null, 2));
    //console.log('sun: ' + JSON.stringify(sun, null, 2));
    //console.log('message: ' + JSON.stringify(message, null, 2));
    return message;
}

function sendLocation(pos) {
    'use strict';
    var message = locationMessage(pos);
    Pebble.sendAppMessage(message, function (result) {
        //console.log('ack tx ' + result.data.transactionId);
    }, function (result) {
        console.log(result.data.error.message);
    });
}

function locationSuccess(pos) {
    'use strict';
    pos.timezone = - new Date().getTimezoneOffset();
    sendLocation(pos);
}

function locationError(positionError) {
    'use strict';
    console.warn('position error: ', positionError);
}

function locationRequest() {
    'use strict';
    var locationOptions = {
            'enableHighAccuracy': true,
            'timeout': 60 * 1000,           // 1 minute (in milliseconds)
            'maximumAge': 5 * 60 * 1000     // 5 minutes (in milliseconds)
        };
    if (navigator.geolocation) {
        //console.log('get current position');
        navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
    } else {
        console.log('location services not available');
    }
}

function locationOverride(locopts) {
    if (locopts && !locopts.automatic) {
        var pos = {
            timestamp: Date.now(),
            timezone: parseInt(locopts.timezone),
            coords: {
                latitude: parseFloat(locopts.latitude),
                longitude: parseFloat(locopts.longitude),
                accuracy: 0
            }
        };
        return pos;
    }
    return null;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

Pebble.addEventListener('ready', function () {
    'use strict';

    var locopts = retrieveObject('location', null),
        pos = locationOverride(locopts);
    if (pos) {
        sendLocation(pos);
    } else {
        locationRequest();
    }
});

Pebble.addEventListener('showConfiguration', function(e) {
    Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener("webviewclosed", function (e) {
    if (e && !e.response) {
        return;
    }

    var k,
        dict = clay.getSettings(e.response),
        userData = clay.getUserData(e.response),
        message = {
            'BLUETOOTH': parseInt(dict[keys.BLUETOOTH], 10),
            'BATTERY': !!dict[keys.BATTERY],
            'PALETTE': []
        },
        locopts = {
            automatic: true /* !!dict[keys.LOCATION],
            latitude: dict[keys.LATITUDE],
            longitude: dict[keys.LONGITUDE],
            timezone: dict[keys.TIMEZONE] */
        };

    //console.log('keys: ' + JSON.stringify(keys, null, 2));
    //console.log('dict: ' + JSON.stringify(dict, null, 2));
    //console.log('locopts: ' + JSON.stringify(locopts, null, 2));
    console.log('userData: ' + JSON.stringify(userData, null, 2));

    storeObject('location', locopts);
    var locpos = locationOverride(locopts);
    if (locpos) {
        var locmsg = locationMessage(locpos);
        _.extendOwn(message, locmsg);
    } else {
        locationRequest();
    }

    for (k = 0; k < 12; ++k) {
        message.PALETTE.push(colors.eightBitColorFromInt(dict[keys.COLORS + k]));
    }

    console.log(JSON.stringify(message, null, 2));

    Pebble.sendAppMessage(message, function(e) {
        //console.log('Sent config data to Pebble');
    }, function(e) {
        console.log('Failed to send config data!');
        console.log(JSON.stringify(e));
    });
});

// ---------------------------------------------------------------------------
// Local Storage
// ---------------------------------------------------------------------------

function storeObject(name, value) {
    window.localStorage.setItem(name, JSON.stringify(value));
}

function retrieveObject(name, defaultValue) {
    var encodedValue = window.localStorage.getItem(name),
        value;
    if (encodedValue) {
        try {
            value = JSON.parse(encodedValue);
            //console.log(name + ': ' + JSON.stringify(value, null, 2));
        } catch (ex) {
            console.log('clear corrupted ' + name + ': ' + encodedValue);
            window.localStorage.removeItem(name);
            value = defaultValue;
        }
    } else {
        value = defaultValue;
    }
    return value;
}
