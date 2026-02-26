#ifndef _URL_BUILDER_H_
#define _URL_BUILDER_H_

// Pure URL-building utilities — no wx or curl dependencies.
// Extracted here so they can be unit-tested without the full plugin build.

#include <algorithm>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>

// Format a double with period decimal separator regardless of system locale.
inline std::string FmtDbl(double d) {
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << std::fixed << std::setprecision(4) << d;
    return oss.str();
}

struct BBox {
    double lat_min, lat_max, lon_min, lon_max;
};

// Clamp a bounding box to valid server ranges [-90,90] x [-180,180].
// Handles OpenCPN viewports panned past the antimeridian (lon outside ±180).
inline BBox ClampBbox(double lat_min, double lat_max,
                      double lon_min, double lon_max) {
    lat_min = std::max(-90.0, std::min(90.0, lat_min));
    lat_max = std::max(-90.0, std::min(90.0, lat_max));

    if (lon_max - lon_min >= 360.0) {
        lon_min = -180.0;
        lon_max =  180.0;
    } else {
        while (lon_min < -180.0) { lon_min += 360.0; lon_max += 360.0; }
        while (lon_min >  180.0) { lon_min -= 360.0; lon_max -= 360.0; }
        lon_min = std::max(-180.0, lon_min);
        lon_max = std::min( 180.0, lon_max);
    }
    return {lat_min, lat_max, lon_min, lon_max};
}

#endif // _URL_BUILDER_H_
