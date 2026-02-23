#!/bin/bash
# Create an OpenCPN plugin tarball from a cmake build directory.
# Usage: ./package.sh <build_dir> <version>
#   build_dir  path to cmake build output (contains the .so/.dylib)
#   version    plugin version string, e.g. "0.1.0"
set -e

BUILD_DIR="${1:?Usage: $0 <build_dir> <version>}"
VERSION="${2:?Usage: $0 <build_dir> <version>}"

PLATFORM=$(uname -s | tr '[:upper:]' '[:lower:]')
ARCH=$(uname -m)

write_metadata() {
    local dest="$1" target="$2" target_ver="$3"
    cat > "${dest}/metadata.xml" << METAEOF
<?xml version="1.0" encoding="UTF-8"?>
<plugin version="1">
  <name>shipobs_pi</name>
  <version>${VERSION}</version>
  <release>1</release>
  <summary>Ship and Buoy Observation Reports</summary>
  <api-version>1.16</api-version>
  <open-source>yes</open-source>
  <author>Alexey Tuboltsev</author>
  <source>https://github.com/AlexeyTuboltsev/ship_reports</source>
  <info-url>https://github.com/AlexeyTuboltsev/ship_reports</info-url>
  <description>Fetches ship and buoy observations from a self-hosted server and renders them as overlays on the OpenCPN chart.</description>
  <target>${target}</target>
  <target-version>${target_ver}</target-version>
  <target-arch>${ARCH}</target-arch>
  <tarball-url></tarball-url>
  <tarball-checksum></tarball-checksum>
</plugin>
METAEOF
}

if [ "${PLATFORM}" = "darwin" ]; then
    MACOS_VER=$(sw_vers -productVersion | cut -d. -f1-2)
    PKG="shipobs_pi-${VERSION}_darwin-${MACOS_VER}-${ARCH}"
    rm -rf "${PKG}"
    mkdir -p "${PKG}/OpenCPN.app/Contents/PlugIns"
    cp "${BUILD_DIR}/libshipobs_pi.dylib" "${PKG}/OpenCPN.app/Contents/PlugIns/"
    write_metadata "${PKG}" "darwin" "${MACOS_VER}"
    tar czf "${PKG}.tar.gz" "${PKG}"
    rm -rf "${PKG}"
    echo "Created: ${PKG}.tar.gz"

elif [ "${PLATFORM}" = "linux" ]; then
    LINUX_VER=$(lsb_release -rs 2>/dev/null || echo "unknown")
    PKG="shipobs_pi-${VERSION}_ubuntu-${LINUX_VER}-${ARCH}"
    rm -rf "${PKG}"
    mkdir -p "${PKG}/lib/opencpn"
    cp "${BUILD_DIR}/libshipobs_pi.so" "${PKG}/lib/opencpn/"
    write_metadata "${PKG}" "ubuntu" "${LINUX_VER}"
    tar czf "${PKG}.tar.gz" "${PKG}"
    rm -rf "${PKG}"
    echo "Created: ${PKG}.tar.gz"

else
    echo "Unsupported platform: ${PLATFORM}" >&2
    exit 1
fi
