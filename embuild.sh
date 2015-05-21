#!/bin/bash
# Huh?? Why we suddenly start needing pwd here?
pushd ~/emsdk && pwd && source emsdk_env.sh && popd
echo "const char *gitversion = \"commit `git rev-parse --short HEAD`\";" > src/drivers/em/gitversion.c 
emscons scons -j 4 $@
gzip -f -k -9 deployment/fceux.js
gzip -f -k -9 deployment/fceux.js.mem
