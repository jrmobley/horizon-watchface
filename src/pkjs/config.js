/* eslint-disable quotes */

module.exports = [
    {
        type: "heading",
        id: "main-heading",
        defaultValue: "Horizon",
        size: 1
    },
    {
        type: "section",
        items: [
            {
                type: 'select',
                messageKey: 'BLUETOOTH',
                label: 'Connection Status',
                options: [
                    { label: 'None', value: 0 },
                    { label: 'Visual', value: 1 },
                    { label: 'Visual + Vibration', value: 2 }
                ],
                defaultValue: 1
            },
            {
                type: 'toggle',
                messageKey: 'BATTERY',
                label: 'Battery Status',
                defaultValue: true
            },
            /*{
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
            }*/
        ]
    },
    {
        type: 'section',
        items: [
            {
                capabilities: ['BW'],
                type: 'select',
                messageKey: 'PALETTE',
                label: 'Color Palette',
                options: [
                    { label: 'White', value: 'white', writable: false, colors: [
                        'LightGray', 'White', 'White', 'White',
                        'Black', 'LightGray', 'Black', 'White',
                        'Black', 'White', 'White', 'Black'
                    ]},
                    { label: 'Black', value: 'black', writable: false, colors: [
                        'LightGray', 'Black', 'Black', 'Black',
                        'White', 'DarkGray', 'White', 'Black',
                        'White', 'Black', 'Black', 'White'
                    ]}
                ],
                defaultValue: 'white'
            },
            {
                capabilities: ['COLOR'],
                type: 'select',
                messageKey: 'PALETTE',
                label: 'Color Palette',
                options: [
                    { label: 'Color', value: 'color', writable: false, colors: [
                        'LightGray', 'PictonBlue', 'Icterine', 'White',
                        'Black', 'LightGray', 'Black', 'White',
                        'DarkGray', 'White', 'White', 'DarkGray'
                    ]},
                    { label: 'Extra Color', value: 'morec', writable: false, colors: [
                        'LightGray', 'PictonBlue', 'Icterine', 'White',
                        'Black', 'LightGray', 'Black', 'White',
                        'DarkGray', 'BrightGreen', 'VividCerulean', 'DarkCandyAppleRed'
                    ]},
                    { label: 'White', value: 'white', writable: false, colors: [
                        'LightGray', 'White', 'White', 'White',
                        'Black', 'LightGray', 'Black', 'White',
                        'Black', 'White', 'White', 'Black'
                    ]},
                    { label: 'Black', value: 'black', writable: false, colors: [
                        'LightGray', 'Black', 'Black', 'Black',
                        'White', 'DarkGray', 'White', 'Black',
                        'White', 'Black', 'Black', 'White'
                    ]},
                    { label: 'Custom', value: 'custom', writable: true, colors: [
                        'LightGray', 'PictonBlue', 'Icterine', 'White',
                        'Black', 'LightGray', 'Black', 'White',
                        'DarkGray', 'BrightGreen', 'VividCerulean', 'DarkCandyAppleRed'
                    ]}
                ],
                defaultValue: 'color'
            },
            { type: 'color', messageKey: 'COLORS[0]', allowGray: true, label: 'Behind' },
            { type: 'color', messageKey: 'COLORS[2]', allowGray: true, label: 'Above' },
            { type: 'color', messageKey: 'COLORS[1]', allowGray: true, label: 'Below' },
            { type: 'color', messageKey: 'COLORS[3]', allowGray: true, label: 'Within' },
            { type: 'color', messageKey: 'COLORS[4]', allowGray: true, label: 'Markings' },
            { type: 'color', messageKey: 'COLORS[5]', allowGray: true, label: 'Engraving' },
            { type: 'color', messageKey: 'COLORS[6]', allowGray: true, label: 'Text' },
            { type: 'color', messageKey: 'COLORS[7]', allowGray: true, label: 'Solar' },
            { type: 'color', messageKey: 'COLORS[8]', allowGray: true, label: 'Capacity' },
            { type: 'color', messageKey: 'COLORS[9]', allowGray: true, label: 'Charge' },
            { type: 'color', messageKey: 'COLORS[10]', allowGray: true, label: 'Online' },
            { type: 'color', messageKey: 'COLORS[11]', allowGray: true, label: 'Offline' }
        ]
    },
    {
        type: 'section',
        capabilities: ['COLOR'],
        items: [
            {
                type: 'heading',
                defaultValue: 'Preview'
            },
            {
                type: 'preview',
                bindings: [
                    { source: 'BLUETOOTH', selector: '#bluetooth-preview', set: '$display', map: { '0': 'none', '1': 'block', '2': 'block' } },
                    { source: 'BATTERY', selector: '#battery-preview', set: '$display', map: 'show' },
                    { source: 'COLORS[0]', selector: '.color-behind', set: '$color', map: 'color' },
                    { source: 'COLORS[2]', selector: '.color-above', set: '$color', map: 'color' },
                    { source: 'COLORS[1]', selector: '.color-below', set: '$color', map: 'color' },
                    { source: 'COLORS[3]', selector: '.color-within', set: '$color', map: 'color' },
                    { source: 'COLORS[4]', selector: '.color-marks', set: '$color', map: 'color' },
                    { source: 'COLORS[5]', selector: '.color-engrave', set: '$color', map: 'color' },
                    { source: 'COLORS[6]', selector: '.color-text', set: '$color', map: 'color' },
                    { source: 'COLORS[7]', selector: '.color-solar', set: '$color', map: 'color' },
                    { source: 'COLORS[8]', selector: '.color-capacity', set: '$color', map: 'color' },
                    { source: 'COLORS[9]', selector: '.color-charge', set: '$color', map: 'color' },
                    { source: 'COLORS[10]', selector: '.color-online', set: '$color', map: 'color' },
                    { source: 'COLORS[11]', selector: '.color-offline', set: '$color', map: 'color' },

                    { source: '#preview-offline', selector: '#bluetooth-offline', set: '$display', map: 'show' },
                    { source: '#preview-offline', selector: '#bluetooth-online', set: '$display', map: 'hide' },

                    { source: '#preview-lowbatt', selector: '#battery-low', set: '$display', map: 'show' },
                    { source: '#preview-lowbatt', selector: '#battery-full', set: '$display', map: 'hide' }
                ]
            },
            {
                type: 'toggle',
                id: 'preview-offline',
                label: 'Preview as offline',
                defaultValue: false
            },
            {
                type: 'toggle',
                id: 'preview-lowbatt',
                label: 'Preview with low battery',
                defaultValue: false
            }
        ]
    },
    {
        type: 'submit',
        defaultValue: 'Save'
    }
];
