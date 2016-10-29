module.exports = function (minified) {
    var clayConfig = this,
        _ = minified._,
        $ = minified.$,
        HTML = minified.HTML;

    function handleLocationChange() {
        var location = this.get(),
            latitudeInput = clayConfig.getItemByMessageKey('LATITUDE'),
            longitudeInput = clayConfig.getItemByMessageKey('LONGITUDE'),
            timezoneInput = clayConfig.getItemByMessageKey('TIMEZONE');

        if (location) {
            latitudeInput.hide();
            longitudeInput.hide();
            timezoneInput.hide();
        } else {
            latitudeInput.show();
            longitudeInput.show();
            timezoneInput.show();
        }
    }

    var colorNames = [
      'Black', 'OxfordBlue', 'DukeBlue', 'Blue',
      'DarkGreen', 'MidnightGreen', 'CobaltBlue', 'BlueMoon',
      'IslamicGreen', 'JaegerGreen', 'TiffanyBlue', 'VividCerulean',
      'Green', 'Malachite', 'MediumSpringGreen', 'Cyan',
      'BulgarianRose', 'ImperialPurple', 'Indigo', 'ElectricUltramarine',
      'ArmyGreen', 'DarkGray', 'Liberty', 'VeryLightBlue',
      'KellyGreen', 'MayGreen', 'CadetBlue', 'PictonBlue',
      'BrightGreen', 'ScreaminGreen', 'MediumAquamarine', 'ElectricBlue',
      'DarkCandyAppleRed', 'JazzberryJam', 'Purple', 'VividViolet',
      'WindsorTan', 'RoseVale', 'Purpureus', 'LavenderIndigo',
      'Limerick', 'Brass', 'LightGray', 'BabyBlueEyes',
      'SpringBud', 'Inchworm', 'MintGreen', 'Celeste',
      'Red', 'Folly', 'FashionMagenta', 'Magenta',
      'Orange', 'SunsetOrange', 'BrilliantRose', 'ShockingPink',
      'ChromeYellow', 'Rajah', 'Melon', 'RichBrilliantLavender',
      'Yellow', 'Icterine', 'PastelYellow', 'White'
    ];

    /**
     * @param {number|string} value
     * @returns {string|*}
     */
    function decodeColorName(value) {
      var index = colorNames.indexOf(value);
      if (index === -1) {
        return value;
      }
      var r = ('0' + ((index >> 4 & 0x03) * 0x55).toString(16)).substr(-2);
      var g = ('0' + ((index >> 2 & 0x03) * 0x55).toString(16)).substr(-2);
      var b = ('0' + ((index >> 0 & 0x03) * 0x55).toString(16)).substr(-2);
      return r + g + b;
    }

    /**
     * Applies the selected colors to the appropriate color pickers.
     * @return {void}
     */
    function handlePaletteChange() {
        var paletteSelector = this;
        var selectedIndex = paletteSelector.$manipulatorTarget.get('selectedIndex');
        var preset = paletteSelector.config.options[selectedIndex];
        for (var colorIndex = 0; colorIndex < preset.colors.length; ++colorIndex) {
            var messageKey = 'COLORS[' + colorIndex + ']';
            var colorPicker = clayConfig.getItemByMessageKey(messageKey);
            if (colorPicker && colorPicker.config.type === 'color') {
                colorPicker.set(decodeColorName(preset.colors[colorIndex]));
                if (preset.writable) {
                    colorPicker.show();
                } else {
                    colorPicker.hide();
                }
            }
        }
    }

    /**
     * When a color is modified, write the value back into the preset.
     * @return {void}
     */
    function handleColorChange() {
        var colorPicker = this;
        var colorIndex = parseInt(/COLORS\[([0-9]+)\]/.exec(colorPicker.messageKey)[1], 10);
        var paletteSelector = clayConfig.getItemByMessageKey('PALETTE');
        var presetIndex = paletteSelector.$manipulatorTarget.get('selectedIndex');
        var preset = paletteSelector.config.options[presetIndex];
        if (preset.writable) {
            preset.colors[colorIndex] = colorPicker.get();
            preset.modified = true;
        }
    }

    clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {

        /* Attach change handler to location select. */
        var locationSelector = clayConfig.getItemByMessageKey('LOCATION');
        handleLocationChange.call(locationSelector);
        locationSelector.on('change', handleLocationChange);

        /* Attach change handler to palette select. */
        var paletteSelector = clayConfig.getItemByMessageKey('PALETTE');
        handlePaletteChange.call(paletteSelector);
        paletteSelector.on('change', handlePaletteChange);

        /* Attach change handler to all color pickers that are referenced
           by the paletteSelector. */
        var selectedIndex = paletteSelector.$manipulatorTarget.get('selectedIndex');
        var preset = paletteSelector.config.options[selectedIndex];
        for (var colorIndex = 0; colorIndex < preset.colors.length; ++colorIndex) {
            var messageKey = 'COLORS[' + colorIndex + ']';
            var colorPicker = clayConfig.getItemByMessageKey(messageKey);
            if (colorPicker && colorPicker.config.type === 'color') {
                colorPicker.on('change', handleColorChange);
            }
        }
        
    });

    clayConfig.on(clayConfig.EVENTS.BEFORE_SUBMIT, function() {

        var paletteSelector = clayConfig.getItemByMessageKey('PALETTE');
        var modifiedPresets = [];
        paletteSelector.config.options.forEach(function(preset) {
            if (preset.modified) {
                modifiedPresets.push(preset);
            }
        });
        
        clayConfig.meta.userData.modifiedPresets = modifiedPresets;
    });

};
