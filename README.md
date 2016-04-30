# Horizon Watch Face for Pebble smartwatch.

### Dependencies

This project uses submodules.  Use `git submodule update` to get them.

The resource compiler and the configuration page build process both depend
on Node.js.

#### Resource Compiler

To compile SVG resources, you will need to install the package dependencies
for the `fctx-compiler` submodule.
From the `tools/fctx-compiler` directory, run `npm install`.
From the root project directory, run `tools/fctx-compiler/fctx-compiler resources.svg`.

#### Configuration Page

The `bower` node package is used to install client code packages, i.e. slate.
If needed, install `bower` with `npm install -g bower`.
From the `config` directory, run `bower update` to install packages.

The `grunt` node package is used to build and deploy the config page.
If needed, install `grunt` with `npm install -g grunt`.
From the `config` directory, run `npm install` to install local node package dependencies (i.e. grunt plugins).
Edit `Gruntfile.js` and change the `deployment_path` to something useful to you.
Then, from the `config` directory, run `grunt` to process and copy files to the server (i.e. Dropbox).
