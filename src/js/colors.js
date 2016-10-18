
module.exports = {

    colorNames: [
        'Black'                , 'OxfordBlue'           , 'DukeBlue'             , 'Blue'                 ,
        'DarkGreen'            , 'MidnightGreen'        , 'CobaltBlue'           , 'BlueMoon'             ,
        'IslamicGreen'         , 'JaegerGreen'          , 'TiffanyBlue'          , 'VividCerulean'        ,
        'Green'                , 'Malachite'            , 'MediumSpringGreen'    , 'Cyan'                 ,
        'BulgarianRose'        , 'ImperialPurple'       , 'Indigo'               , 'ElectricUltramarine'  ,
        'ArmyGreen'            , 'DarkGray'             , 'Liberty'              , 'VeryLightBlue'        ,
        'KellyGreen'           , 'MayGreen'             , 'CadetBlue'            , 'PictonBlue'           ,
        'BrightGreen'          , 'ScreaminGreen'        , 'MediumAquamarine'     , 'ElectricBlue'         ,
        'DarkCandyAppleRed'    , 'JazzberryJam'         , 'Purple'               , 'VividViolet'          ,
        'WindsorTan'           , 'RoseVale'             , 'Purpureus'            , 'LavenderIndigo'       ,
        'Limerick'             , 'Brass'                , 'LightGray'            , 'BabyBlueEyes'         ,
        'SpringBud'            , 'Inchworm'             , 'MintGreen'            , 'Celeste'              ,
        'Red'                  , 'Folly'                , 'FashionMagenta'       , 'Magenta'              ,
        'Orange'               , 'SunsetOrange'         , 'BrilliantRose'        , 'ShockingPink'         ,
        'ChromeYellow'         , 'Rajah'                , 'Melon'                , 'RichBrilliantLavender',
        'Yellow'               , 'Icterine'             , 'PastelYellow'         , 'White'
    ],

    hexColorFromName: function (name) {
        var self = this,
            r, g, b, hex,
            index = self.colorNames.indexOf(name);
        if (index === -1) {
            if (name.length == 3) {
                index = parseInt(name, 4);
            } else if (name.length == 7 && name[0] == '#') {
                return name;
            } else {
                index = 0;
            }
        }
        r = ('0' + ((index >> 4 & 0x03) * 0x55).toString(16)).substr(-2);
        g = ('0' + ((index >> 2 & 0x03) * 0x55).toString(16)).substr(-2);
        b = ('0' + ((index >> 0 & 0x03) * 0x55).toString(16)).substr(-2);
        hex = '#' + r + g + b;

        //console.log('color ' + name + ' -> ' + hex);
        return hex;
    },

    eightBitColorFromInt : function (integerColor) {
        'use strict';
        var a, r, g, b, i = 0;
        b = (integerColor >> ( 0 + 6)) & 0x03;
        g = (integerColor >> ( 8 + 6)) & 0x03;
        r = (integerColor >> (16 + 6)) & 0x03;
        a = 0x03;
        i = a * 64 + r * 16 + g * 4 + b;
        return i;
    },

    eightBitColorFromHex : function (hexColor) {
        'use strict';
        var integerColor = parseInt(hexColor.substr(-6), 16);
        return this.eightBitColorFromInt(integerColor);
    },

    palettesAsHexStrings : function () {

        var self = this,
            hexpal = {};

        function getHexColor(colorName) {
            return self.hexColorFromName(colorName);
        }
        for (var paletteName in self.palettes) {
            if (self.palettes.hasOwnProperty(paletteName)) {
                hexpal[paletteName] = self.palettes[paletteName].map(getHexColor);
            }
        }
        return hexpal;
    },

    palettes: {
        //         Behind       Below         Above       Within   Marks    Engraving    Text     Solar    Capacity    Charge         Online           Offline
        'color': [ 'LightGray', 'PictonBlue', 'Icterine', 'White', 'Black', 'LightGray', 'Black', 'White', 'DarkGray', 'White',       'White',         'DarkGray' ],
        'morec': [ 'LightGray', 'PictonBlue', 'Icterine', 'White', 'Black', 'LightGray', 'Black', 'White', 'DarkGray', 'BrightGreen', 'VividCerulean', 'DarkCandyAppleRed' ],
        'white': [ 'LightGray', 'White',      'White',    'White', 'Black', 'LightGray', 'Black', 'White', 'Black',    'White',       'White',         'Black' ],
        'black': [ 'LightGray', 'Black',      'Black',    'Black', 'White', 'DarkGray',  'White', 'Black', 'White',    'Black',       'Black',         'White' ]
    }
};
