# Plugin build image — extends the OpenCPN builder which already has
# libwxgtk3.2-dev, cmake, ninja, libcurl4-openssl-dev, libGL-dev, etc.
FROM opencpn-builder:latest
# gettext needed for msgfmt to compile .po → .mo translation files
RUN apt-get update && apt-get install -y --no-install-recommends gettext && rm -rf /var/lib/apt/lists/*
WORKDIR /plugin
