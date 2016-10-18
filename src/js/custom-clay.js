module.exports = function (minified) {
    var clayConfig = this,
        _ = minified._,
        $ = minified.$,
        HTML = minified.HTML;

    function resetAllToDefault() {
        clayConfig.getAllItems().forEach(function(item) {
            if(item.config && item.config.defaultValue){
                item.set(item.config.defaultValue);
            }
        });
    }

    function handleLocationChange() {
        var location = this.get(),
            latitudeInput = clayConfig.getItemByMessageKey('LATITUDE'),
            longitudeInput = clayConfig.getItemByMessageKey('LONGITUDE');
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

    function handlePaletteChange() {
        var palette = this.get();

        clayConfig.getItemsByType('color').forEach(function(item) {
            if (palette === 'custom') {
                item.show();
            } else {
                item.hide();
            }
        });
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

    });

};
