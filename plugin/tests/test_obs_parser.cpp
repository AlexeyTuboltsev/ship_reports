#include "test_runner.h"
#include "../src/obs_parser.h"

#include <wx/app.h>
#include <wx/log.h>

// ---- helpers ---------------------------------------------------------------

static ObservationList parse(const char *json_str) {
    ObservationList out;
    wxString err;
    ParseObservations(wxString::FromUTF8(json_str), out, err);
    return out;
}

static const char *FULL_STATION = R"({
    "generated": "2026-02-20T15:00:00Z",
    "count": 1,
    "stations": [{
        "id": "41008",
        "type": "buoy",
        "country": "US",
        "lat": 31.4,
        "lon": -80.87,
        "time": "2026-02-20T14:30:00Z",
        "wind_dir": 170.0,
        "wind_spd": 5.0,
        "gust": 7.0,
        "pressure": 1014.7,
        "air_temp": 14.8,
        "sea_temp": 18.2,
        "wave_ht": 1.2,
        "vis": 10000.0
    }]
})";

// ---- basic parsing ---------------------------------------------------------

TEST(ParseObservations_full_station_fields) {
    auto stns = parse(FULL_STATION);
    REQUIRE_EQ((int)stns.size(), 1);
    const ObservationStation &s = stns[0];
    REQUIRE_EQ(std::string(s.id.mb_str()), "41008");
    REQUIRE_EQ(std::string(s.type.mb_str()), "buoy");
    REQUIRE_EQ(std::string(s.country.mb_str()), "US");
    REQUIRE_NEAR(s.lat, 31.4, 1e-6);
    REQUIRE_NEAR(s.lon, -80.87, 1e-6);
    REQUIRE_NEAR(s.wind_dir, 170.0, 1e-6);
    REQUIRE_NEAR(s.wind_spd, 5.0, 1e-6);
    REQUIRE_NEAR(s.gust, 7.0, 1e-6);
    REQUIRE_NEAR(s.pressure, 1014.7, 1e-6);
    REQUIRE_NEAR(s.air_temp, 14.8, 1e-6);
    REQUIRE_NEAR(s.sea_temp, 18.2, 1e-6);
    REQUIRE_NEAR(s.wave_ht, 1.2, 1e-6);
    REQUIRE_NEAR(s.vis, 10000.0, 1e-6);
    REQUIRE(s.time.IsValid());
    REQUIRE_EQ(s.time.GetYear(), 2026);
    REQUIRE_EQ((int)s.time.GetMonth(), (int)wxDateTime::Feb);
    REQUIRE_EQ(s.time.GetDay(), 20);
    REQUIRE_EQ(s.time.GetHour(), 14);
    REQUIRE_EQ(s.time.GetMinute(), 30);
}

TEST(ParseObservations_missing_optional_fields_are_nan) {
    const char *json = R"({"stations": [{
        "id": "SHORE1", "type": "shore",
        "lat": 50.0, "lon": 0.0, "time": "2026-02-20T12:00:00Z"
    }]})";
    auto stns = parse(json);
    REQUIRE_EQ((int)stns.size(), 1);
    REQUIRE(std::isnan(stns[0].wind_spd));
    REQUIRE(std::isnan(stns[0].pressure));
    REQUIRE(std::isnan(stns[0].wave_ht));
}

TEST(ParseObservations_empty_stations_array) {
    auto stns = parse(R"({"stations": []})");
    REQUIRE_EQ((int)stns.size(), 0);
}

TEST(ParseObservations_multiple_stations) {
    const char *json = R"({"stations": [
        {"id": "A", "type": "ship", "lat": 10.0, "lon": 20.0, "time": "2026-01-01T00:00:00Z"},
        {"id": "B", "type": "buoy", "lat": 11.0, "lon": 21.0, "time": "2026-01-01T00:00:00Z"}
    ]})";
    auto stns = parse(json);
    REQUIRE_EQ((int)stns.size(), 2);
}

// ---- invalid / missing required fields drop station -----------------------

TEST(ParseObservations_missing_id_drops_station) {
    const char *json = R"({"stations": [
        {"type": "buoy", "lat": 10.0, "lon": 20.0, "time": "2026-01-01T00:00:00Z"}
    ]})";
    REQUIRE_EQ((int)parse(json).size(), 0);
}

TEST(ParseObservations_empty_id_drops_station) {
    const char *json = R"({"stations": [
        {"id": "", "type": "buoy", "lat": 10.0, "lon": 20.0, "time": "2026-01-01T00:00:00Z"}
    ]})";
    REQUIRE_EQ((int)parse(json).size(), 0);
}

TEST(ParseObservations_missing_lat_drops_station) {
    const char *json = R"({"stations": [
        {"id": "X", "type": "buoy", "lon": 20.0, "time": "2026-01-01T00:00:00Z"}
    ]})";
    REQUIRE_EQ((int)parse(json).size(), 0);
}

TEST(ParseObservations_lat_out_of_range_drops_station) {
    const char *json = R"({"stations": [
        {"id": "X", "type": "buoy", "lat": 91.0, "lon": 20.0, "time": "2026-01-01T00:00:00Z"}
    ]})";
    REQUIRE_EQ((int)parse(json).size(), 0);
}

TEST(ParseObservations_lon_out_of_range_drops_station) {
    const char *json = R"({"stations": [
        {"id": "X", "type": "buoy", "lat": 10.0, "lon": 181.0, "time": "2026-01-01T00:00:00Z"}
    ]})";
    REQUIRE_EQ((int)parse(json).size(), 0);
}

TEST(ParseObservations_invalid_time_drops_station) {
    const char *json = R"({"stations": [
        {"id": "X", "type": "buoy", "lat": 10.0, "lon": 20.0, "time": "not-a-date"}
    ]})";
    REQUIRE_EQ((int)parse(json).size(), 0);
}

TEST(ParseObservations_valid_and_invalid_mixed) {
    const char *json = R"({"stations": [
        {"id": "GOOD", "type": "ship", "lat": 10.0, "lon": 20.0, "time": "2026-01-01T00:00:00Z"},
        {"type": "buoy", "lat": 10.0, "lon": 20.0, "time": "2026-01-01T00:00:00Z"}
    ]})";
    auto stns = parse(json);
    REQUIRE_EQ((int)stns.size(), 1);
    REQUIRE_EQ(std::string(stns[0].id.mb_str()), "GOOD");
}

// ---- structural errors -----------------------------------------------------

TEST(ParseObservations_invalid_json_returns_false) {
    ObservationList out;
    wxString err;
    bool ok = ParseObservations(wxT("{not json}"), out, err);
    REQUIRE(!ok);
    REQUIRE(!err.IsEmpty());
}

TEST(ParseObservations_missing_stations_key_returns_false) {
    ObservationList out;
    wxString err;
    bool ok = ParseObservations(wxT("{\"count\": 0}"), out, err);
    REQUIRE(!ok);
}

int main(int argc, char **argv) {
    // Suppress wx log output during tests
    wxLogNull null_log;
    return run_tests(argc, argv);
}
