/* eslint-disable quotes */

module.exports = [
    {
        type: "heading",
        id: "main-heading",
        defaultValue: "Horizon",
        size: 1
    },
    {
        type: "text",
        defaultValue: "Time is an illusion."
    },
    {
        type: "section",
        items: [
            {
                type: 'heading',
                defaultValue: 'Options'
            },
            {
                capabilities: ['BW'],
                type: 'select',
                messageKey: 'DATEFONT',
                label: 'Date Font',
                options: [
                    { label: 'DIN',         value: 0 },
                    { label: 'Gothic',      value: 1 },
                    { label: 'Gothic Bold', value: 2 }
                ],
                defaultValue: 1
            },
            {
                capabilities: ['COLOR'],
                type: 'select',
                messageKey: 'DATEFONT',
                label: 'Date Font',
                options: [
                    { label: 'DIN',         value: 0 },
                    { label: 'Gothic',      value: 1 },
                    { label: 'Gothic Bold', value: 2 }
                ],
                defaultValue: 0
            },
            {
                type: 'select',
                messageKey: 'BLUETOOTH',
                label: 'Disconnect Alert',
                options: [
                    { label: 'None',               value: 0 },
                    { label: 'Visual',             value: 1 },
                    { label: 'Visual + Vibration', value: 2 }
                ],
                defaultValue: 1
            },
            {
                type: 'toggle',
                messageKey: 'BATTERY',
                label: 'Battery Indicator',
                defaultValue: true
            },
            {
                type: 'toggle',
                messageKey: 'LOCATION',
                label: 'Automatic Location',
                defaultValue: true
            },
            {
                type: 'input',
                messageKey: 'LATITUDE',
                label: 'Latitude',
                description: 'Degrees North',
                defaultValue: 40.42
            },
            {
                type: 'input',
                messageKey: 'LONGITUDE',
                label: 'Longitude',
                description: 'Degrees East',
                defaultValue: -3.72
            },
            {
                type: 'input',
                messageKey: 'TIMEZONE',
                label: 'Time Zone',
                description: 'Minutes from UTC',
                defaultValue: 120
            },
            {
                capabilities: ['BW'],
                type: 'select',
                messageKey: 'PALETTE',
                label: 'Color Palette',
                options: [
                    { label: 'White', value: 'white'      },
                    { label: 'Black', value: 'black'      }
                ],
                defaultValue: 'white'
            },
            {
                capabilities: ['COLOR'],
                type: 'select',
                messageKey: 'PALETTE',
                label: 'Color Palette',
                options: [
                    { label: 'Color',           value: 'color' },
                    { label: 'Extra Color',     value: 'morec' },
                    { label: 'White',           value: 'white'   },
                    { label: 'Black',           value: 'black'   },
                    { label: 'Custom',          value: 'custom'  }
                ],
                defaultValue: 'color'
            },
            { type: 'color', messageKey: 'COLORS[0]',  label: 'Behind',   defaultValue: '#AAAAAA' },
            { type: 'color', messageKey: 'COLORS[2]',  label: 'Above',    defaultValue: '#FFFF55' },
            { type: 'color', messageKey: 'COLORS[1]',  label: 'Below',    defaultValue: '#55AAFF' },
            { type: 'color', messageKey: 'COLORS[3]',  label: 'Within',   defaultValue: '#FFFFFF' },
            { type: 'color', messageKey: 'COLORS[4]',  label: 'Markings', defaultValue: '#000000' },
            { type: 'color', messageKey: 'COLORS[5]',  label: 'Engraving',defaultValue: '#AAAAAA' },
            { type: 'color', messageKey: 'COLORS[6]',  label: 'Text',     defaultValue: '#000000' },
            { type: 'color', messageKey: 'COLORS[7]',  label: 'Solar',    defaultValue: '#FFFFFF' },
            { type: 'color', messageKey: 'COLORS[8]',  label: 'Capacity', defaultValue: '#555555' },
            { type: 'color', messageKey: 'COLORS[9]',  label: 'Charge',   defaultValue: '#AAFF00' },
            { type: 'color', messageKey: 'COLORS[10]', label: 'Online',   defaultValue: '#AAAAFF' },
            { type: 'color', messageKey: 'COLORS[11]', label: 'Offline',  defaultValue: '#AA0000' }
        ]
    },
    {
        type: 'submit',
        defaultValue: 'Save'
    }
];
