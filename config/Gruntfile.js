module.exports = function(grunt) {

    var path = require('path');
    function getHomeDir() {
        var dirpath = path.join.apply(path, arguments);
        var homepath = process.env[process.platform === 'win32' ? 'USERPROFILE' : 'HOME'];
        dirpath = path.resolve(homepath, dirpath);
        return dirpath;
    }

    grunt.initConfig({
        deployment_path: getHomeDir('Dropbox/Apps/site44/files.mustacea.com/horizon/dev'),

        copy: {
            dist: {
                files: [ {src: 'index.html', dest: 'dist/config.html'} ]
            },
            deploy: {
                files: [
                    { src: 'dist/config.html', dest: '<%= deployment_path %>/config.html'},
                    { src: 'dist/config.css',  dest: '<%= deployment_path %>/config.css'},
                    { src: 'dist/config.js',   dest: '<%= deployment_path %>/config.js'}
                ]
            }
        },
        
        'useminPrepare': {
            options: {
                dest: 'dist'
            },
            html: 'index.html'
        },
        
        usemin: {
            html: ['dist/config.html']
        }
    });

    grunt.loadNpmTasks('grunt-contrib-uglify');
    grunt.loadNpmTasks('grunt-contrib-cssmin');
    grunt.loadNpmTasks('grunt-contrib-concat');
    grunt.loadNpmTasks('grunt-contrib-copy');
    grunt.loadNpmTasks('grunt-usemin');

    grunt.registerTask('default', [
        'useminPrepare',
        'copy:dist',
        'concat:generated',
        'cssmin:generated',
        'uglify:generated',
        'usemin',
        'copy:deploy'
    ]);
};
