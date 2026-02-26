#!/bin/bash
# Build and run C++ unit tests inside the shipobs-builder container.
# Usage: ./run-tests-docker.sh [ctest-regex-filter]
#
# Examples:
#   ./run-tests-docker.sh                 # run all tests
#   ./run-tests-docker.sh url_builder     # run only url_builder tests
#   ./run-tests-docker.sh obs_parser      # run only obs_parser tests
set -e

PLUGIN_DIR="$(cd "$(dirname "$0")" && pwd)"

docker build -t shipobs-builder:latest "$PLUGIN_DIR"

CTEST_ARGS="--output-on-failure"
if [ -n "$1" ]; then
    CTEST_ARGS="$CTEST_ARGS -R $1"
fi

docker run --rm \
    -v "$PLUGIN_DIR:/plugin" \
    shipobs-builder:latest \
    bash -c "
        set -e
        cd /plugin
        rm -rf build-tests && mkdir build-tests && cd build-tests
        cmake .. -DBUILD_TESTS=ON
        make -j\$(nproc)
        ctest $CTEST_ARGS
    "

echo "All tests passed."
