#!/bin/bash
#
# deploy.sh - Johan Smet - BSD-3-Clause (see LICENSE)

if [[ $# -ne 1 ]]; then
	echo "Usage: ./deploy.sh <destination>"
        exit 1
fi

DESTDIR=$1

mkdir -p $DESTDIR/wasm
cp *.js $DESTDIR
cp *.html $DESTDIR
cp wasm/shader_sim_api.* $DESTDIR/wasm
