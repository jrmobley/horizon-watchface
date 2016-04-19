/*global console:false, $:false, window:false, location:false*/
var options;

function loadOptions() {
    'use strict';
    var encodedOptions = window.location.hash.substring(1),
        jsonOptions = decodeURIComponent(encodedOptions);

    try {
        options = JSON.parse(jsonOptions);
    } catch (ex) {
        options = {};
    }
    console.log('options: ' + JSON.stringify(options));
    if (options.bluetooth === undefined) {
        options.bluetooth = 1;
    }
    if (options.battery === undefined) {
        options.battery = 0;
    }
    if (options.palette === undefined) {
        options.palette = 'default';
    }

    $('#bluetooth-select').val(options.bluetooth);
    if (options.battery) $('#battery-toggle').attr('checked', 1);
}

(function () {
    'use strict';
    loadOptions();
})();

function getQueryParam(variable, defaultValue) {
    'use strict';
    // Find all URL parameters
    var query = location.search.substring(1),
        vars = query.split('&'),
        i,
        pair;
    for (i = 0; i < vars.length; i++) {
        pair = vars[i].split('=');

        // If the query variable parameter is found, decode it to use and return it for use
        if (pair[0] === variable) {
            return decodeURIComponent(pair[1]);
        }
    }
    return defaultValue || false;
}

function hexColorFromQuat(quaternaryColor) {
    return '0x' + quaternaryColor.split('').map(function (ch) {
        return ('0' + (parseInt(ch, 4) * 0x55).toString(16)).substr(-2);
    }).join('');
}

function quatColorFromHex(hexColor) {
    var hex = hexColor.substr(-6),
        r = (parseInt(hex.substr(0, 2), 16) / 0x55).toString(4),
        g = (parseInt(hex.substr(2, 2), 16) / 0x55).toString(4),
        b = (parseInt(hex.substr(4, 2), 16) / 0x55).toString(4),
        quat = r + g + b;
    return quat;
}

function rgbaColorFromHex(hexColor, alpha) {
    var hex = hexColor.substr(-6),
        r = parseInt(hex.substr(0, 2), 16).toString(10),
        g = parseInt(hex.substr(2, 2), 16).toString(10),
        b = parseInt(hex.substr(4, 2), 16).toString(10),
        rgba = 'rgba(' + r + ', ' + g + ', ' + b + ', ' + alpha + ')';
    return rgba;
}

$().ready(function () {
    'use strict';
    var platform = getQueryParam('platform', 'aplite'),
        version = getQueryParam('version', '1.6'),
        returnTo = getQueryParam('return_to', 'pebblejs://close#'),
        palettePreview = $('.preview'),
        palettes = {
            'default': { behind: '333', below: '123', above: '331', within: '000', marks: '000', text: '333', charge: '130', solar: '333' },
            'inverse': { behind: '333', below: '123', above: '331', within: '333', marks: '000', text: '000', charge: '130', solar: '333' },
            'bw':      { behind: '000', below: '000', above: '000', within: '333', marks: '333', text: '000', charge: '000', solar: '000' },
            'wb':      { behind: '333', below: '333', above: '333', within: '000', marks: '000', text: '333', charge: '333', solar: '333' },
            'black':   { behind: '333', below: '000', above: '000', within: '000', marks: '333', text: '333', charge: '000', solar: '000' }
        },
        aplitePalettes = ['default', 'inverse'];

    $('#battery-toggle').on('change', function (event) {
        console.log('alert change: ' + $(event.target).attr('checked'));

        var battery = $(event.target).attr('checked');
        $('#battery-preview').css('display', battery ? 'block' : 'none');
    }).change();


    $('#palette-preset').on('change', function (event) {
        console.log('palette preset on change: ' + $(event.target).val());

        var palette = $(event.target).val(),
            colors = palettes[palette],
            key,
            selector,
            hexColor;

        if (colors) {
            for (key in colors) {
                selector = '#color-' + key;
                hexColor = hexColorFromQuat(colors[key]);
                $(selector).val(hexColor).change();
            }
        }
        $('.custom-colors').css('display', (palette !== 'custom') ? 'none' : '');
        $('.item-color').prop('disabled', palette !== 'custom');
    });

    $('.item-color').on('change', function (event) {
        var target = $(event.target),
            name = target.attr('name'),
            selector = 'svg .color-' + name,
            color = target.val().replace(/^0x/, '#');
        if (name === 'solar') {
            color = rgbaColorFromHex(color, 0.5);
        }
        $(selector).css('color', color);
    });

    if (options.custom) {
        palettes.custom = options.custom;
    } else {
        palettes.custom = options.colors;
    }
    $('#palette-preset').val(options.palette).change();

    $('#b-cancel').on('click', function () {
        console.log('Cancel');
        location.href = returnTo;
    });

    function extractColors(palette) {
        var colors = {};
        if (palette === 'custom') {
            console.log('record custom colors');
            $('.item-color').each(function (index, element) {
                var picker = $(this),
                    name = picker.attr('name'),
                    color = quatColorFromHex(picker.val());
                colors[name] = color;
            });
            palettes[palette] = colors;
        }
        if (palettes.hasOwnProperty(palette)) {
            colors = palettes[palette];
        }
        return colors;
    }

    $('#b-submit').on('click', function () {
        console.log('Submit');
        var preset = $('#palette-preset'),
            palette = preset.val(),
            colors = extractColors(palette),
            options = {
                'bluetooth': $('#bluetooth-select').val(),
                'battery': $('#battery-toggle:checked').val() ? 1 : 0,
                'palette': palette,
                'colors': colors,
                'custom': palettes.custom
            },
            jsonOptions = JSON.stringify(options),
            encodedOptions = encodeURIComponent(jsonOptions),
            url = returnTo + encodedOptions;
        console.log('Return options: ' + jsonOptions);
        console.log('Return to: ' + url);
        location.href = url;
    });

    $('#version').text(version);

});
