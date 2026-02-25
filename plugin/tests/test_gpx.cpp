#include "test_runner.h"
#include "../src/gpx_builder.h"

#include <cmath>
#include <cstdio>
#include <wx/log.h>

// ---- helpers ---------------------------------------------------------------

static ObservationStation make_station(const char *id, double lat, double lon,
                                       double wind_spd_ms = NAN,
                                       double wind_dir = NAN) {
    ObservationStation st;
    st.id   = wxString::FromUTF8(id);
    st.type = wxT("buoy");
    st.lat  = lat;
    st.lon  = lon;
    st.wind_spd = wind_spd_ms;
    st.wind_dir = wind_dir;
    st.time.ParseISOCombined(wxT("2026-02-20T14:30:00Z"));
    return st;
}

static wxDateTime fetch_time() {
    wxDateTime t;
    t.ParseISOCombined(wxT("2026-02-20T15:00:00Z"));
    return t;
}

// ---- structure -------------------------------------------------------------

TEST(BuildGPXString_has_gpx_header) {
    ObservationList stns = {make_station("41008", 31.4, -80.87)};
    wxString gpx = BuildGPXString(fetch_time(), stns);
    REQUIRE(gpx.Contains(wxT("<?xml")));
    REQUIRE(gpx.Contains(wxT("<gpx")));
    REQUIRE(gpx.Contains(wxT("</gpx>")));
}

TEST(BuildGPXString_has_wpt_element) {
    ObservationList stns = {make_station("41008", 31.4, -80.87)};
    wxString gpx = BuildGPXString(fetch_time(), stns);
    REQUIRE(gpx.Contains(wxT("<wpt")));
    REQUIRE(gpx.Contains(wxT("</wpt>")));
}

TEST(BuildGPXString_lat_lon_format) {
    ObservationList stns = {make_station("ST1", 31.400000, -80.870000)};
    wxString gpx = BuildGPXString(fetch_time(), stns);
    REQUIRE(gpx.Contains(wxT("lat=\"31.400000\"")));
    REQUIRE(gpx.Contains(wxT("lon=\"-80.870000\"")));
}

TEST(BuildGPXString_station_name_in_output) {
    ObservationList stns = {make_station("41008", 31.4, -80.87)};
    wxString gpx = BuildGPXString(fetch_time(), stns);
    REQUIRE(gpx.Contains(wxT("<name>41008</name>")));
}

TEST(BuildGPXString_multiple_stations) {
    ObservationList stns = {
        make_station("A", 10.0, 20.0),
        make_station("B", 11.0, 21.0),
    };
    wxString gpx = BuildGPXString(fetch_time(), stns);
    REQUIRE(gpx.Contains(wxT("<name>A</name>")));
    REQUIRE(gpx.Contains(wxT("<name>B</name>")));
}

TEST(BuildGPXString_empty_list_produces_valid_gpx) {
    ObservationList stns;
    wxString gpx = BuildGPXString(fetch_time(), stns);
    REQUIRE(gpx.Contains(wxT("<gpx")));
    REQUIRE(gpx.Contains(wxT("</gpx>")));
    REQUIRE(!gpx.Contains(wxT("<wpt")));
}

TEST(BuildGPXString_nan_latlon_station_skipped) {
    ObservationStation bad;
    bad.id  = wxT("BAD");
    bad.lat = NAN;
    bad.lon = NAN;
    bad.time.ParseISOCombined(wxT("2026-02-20T14:30:00Z"));
    ObservationList stns = {bad};
    wxString gpx = BuildGPXString(fetch_time(), stns);
    REQUIRE(!gpx.Contains(wxT("<wpt")));
}

// ---- unit conversions ------------------------------------------------------

TEST(BuildGPXString_wind_speed_converted_to_knots) {
    // 1 m/s = 1.94384 kts → 5 m/s = 9.7 kts
    ObservationStation st = make_station("S1", 0.0, 0.0, /*wind_spd=*/5.0);
    wxString gpx = BuildGPXString(fetch_time(), {st});
    REQUIRE(gpx.Contains(wxT("9.7 kts")));
}

TEST(BuildGPXString_missing_wind_speed_not_in_output) {
    ObservationStation st = make_station("S1", 0.0, 0.0, /*wind_spd=*/NAN);
    wxString gpx = BuildGPXString(fetch_time(), {st});
    REQUIRE(!gpx.Contains(wxT("Wind speed")));
}

TEST(BuildGPXString_pressure_in_hpa) {
    ObservationStation st = make_station("S1", 0.0, 0.0);
    st.pressure = 1013.0;
    wxString gpx = BuildGPXString(fetch_time(), {st});
    REQUIRE(gpx.Contains(wxT("1013.0 hPa")));
}

TEST(BuildGPXString_fetched_at_in_desc) {
    ObservationList stns = {make_station("S1", 0.0, 0.0)};
    wxString gpx = BuildGPXString(fetch_time(), stns);
    REQUIRE(gpx.Contains(wxT("2026-02-20T15:00:00Z")));
}

// ---- ISO time in <time> element --------------------------------------------

TEST(BuildGPXString_obs_time_element) {
    ObservationList stns = {make_station("S1", 0.0, 0.0)};
    wxString gpx = BuildGPXString(fetch_time(), stns);
    REQUIRE(gpx.Contains(wxT("<time>2026-02-20T14:30:00Z</time>")));
}

int main(int argc, char **argv) {
    wxLogNull null_log;
    return run_tests(argc, argv);
}
