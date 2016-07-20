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

var Clay = require('pebble-clay');
var clayConfig = require('./config.js');
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });
var sunriset = require('./sunriset.js');

function locationSuccess(pos) {
    'use strict';
    var coordinates = pos.coords,
        now = new Date(),
        utcOffset = now.getTimezoneOffset(),
        sun = sunriset.sun_rise_set(now, coordinates.longitude, coordinates.latitude),
        message = {
            'LATITUDE': coordinates.latitude * 0x10000,
            'LONGITUDE': coordinates.longitude * 0x10000,
            'TIMEZONE': -utcOffset,
            'TIMESTAMP': pos.timestamp / 1000,
            'SUNRISE': sun.rise * 60,
            'SUNSET': sun.set * 60,
            'SUNSOUTH': sun.south * 60,
            'SUNSTAT': sun.status
        };

    console.log('location: ' + coordinates.latitude + ', ' + coordinates.longitude + ' +-' + coordinates.accuracy + 'm');
    console.log('sun: ' + sun.rise + ' ~ ' + sun.south + ' ~ ' + sun.set + ' (' + sun.status + ')');
    console.log('send ' + JSON.stringify(message, null, 2));
    Pebble.sendAppMessage(message, function (result) {
        console.log('ack tx ' + result.data.transactionId);
    }, function (result) {
        console.log(result.data.error.message);
    });
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
        console.log('get current position');
        navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
    } else {
        console.log('location services not available');
    }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

Pebble.addEventListener('ready', function () {
    'use strict';
    locationRequest();
});

Pebble.addEventListener('showConfiguration', function(e) {
    Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener("webviewclosed", function (e) {
    if (e && !e.response) {
        return;
    }

    var dict = clay.getSettings(e.response);
    /*
    Pebble.sendAppMessage(dict, function(e) {
        console.log('Sent config data to Pebble');
    }, function(e) {
        console.log('Failed to send config data!');
        console.log(JSON.stringify(e));
    });
    */
});
