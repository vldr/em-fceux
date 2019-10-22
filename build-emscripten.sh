#!/bin/bash
set -eEuo pipefail

hash scons 2>/dev/null || { echo >&2 "ERROR: scons not found. Please install scons."; exit 1; }
hash python 2>/dev/null || { echo >&2 "ERROR: python not found. Please install python."; exit 1; }
[ -z ${EMSDK+x} ] && { echo >&2 "ERROR: emscripten env not set. Please run 'source emsdk_env.sh'."; exit 1; }
if [ -z ${EMSCRIPTEN_ROOT+x} ] ; then
  DEFAULT_ROOT="$EMSDK/upstream/emscripten"
  [ -e "$DEFAULT_ROOT/emcc" ] || echo "WARNING: Failed to generate valid env EMSCRIPTEN_ROOT=$DEFAULT_ROOT. You may need to set it manually for scons."
  export EMSCRIPTEN_ROOT=$DEFAULT_ROOT
fi
echo $EMSCRIPTEN_ROOT

NUM_CPUS=`getconf _NPROCESSORS_ONLN`
emscons scons -j $NUM_CPUS $@

# TODO: tsone: following should be added to Scons scripts?
config_js=src/drivers/em/site/config.js
input_inc=src/drivers/em/input.inc.hpp
config_inc=src/drivers/em/config.inc.hpp
echo "// WARNING! AUTOMATICALLY GENERATED FILE. DO NOT EDIT." > $config_js
emcc -D 'CONTROLLER_PRE=' -D 'CONTROLLER_POST=' -D 'CONTROLLER(i_,d_,e_,id_)=var e_ = i_;' -E $config_inc -P >> $config_js
emcc -D 'CONTROLLER_PRE=var FCEC = { controllers : {' -D 'CONTROLLER_POST=},' -D 'CONTROLLER(i_,d_,e_,id_)=i_ : [ d_, id_ ],' -E $config_inc -P >> $config_js
emcc -D 'INPUT_PRE=inputs : {' -D 'INPUT_POST=},' -D 'INPUT(i_,dk_,dg_,e_,t_)=i_ : [ dk_, dg_, t_ ],' -E $input_inc -P >> $config_js
# Save fceux.js file size to config.js for progress bar size when gzip transfer encoding is used.
fceux_js=src/drivers/em/site/fceux.js
python -c "import os; print 'FCEUX_JS_SIZE : %s,\n};' % (os.stat('$fceux_js').st_size)" >> $config_js
echo "Generated $config_js"
