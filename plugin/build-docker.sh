#!/bin/bash
# Build shipobs_pi inside the OpenCPN builder container.
# Usage: ./build-docker.sh
set -e
PLUGIN_DIR="$(cd "$(dirname "$0")" && pwd)"
OCPN_INC="$HOME/OpenCPN/include"

docker build -t shipobs-builder:latest "$PLUGIN_DIR"

docker run --rm \
    -v "$PLUGIN_DIR:/plugin" \
    -v "$OCPN_INC:/ocpn-include:ro" \
    shipobs-builder:latest \
    bash -c "
        set -e
        cd /plugin
        rm -rf build && mkdir build && cd build
        cmake .. -DOPENCPN_INCLUDE_DIR=/ocpn-include
        make -j\$(nproc)
    "

echo "Built: $PLUGIN_DIR/build/libshipobs_pi.so"
