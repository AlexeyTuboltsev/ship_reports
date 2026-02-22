#include "server_client.h"

#include <curl/curl.h>
#include <wx/jsonreader.h>
#include <wx/jsonval.h>
#include <wx/log.h>
#include <sstream>
#include <iomanip>
#include <locale>

static size_t CurlWriteCallback(char *ptr, size_t size, size_t nmemb,
                                 void *userdata) {
    std::string *buf = static_cast<std::string *>(userdata);
    buf->append(ptr, size * nmemb);
    return size * nmemb;
}

// API contract (server guarantees):
//   id:   non-empty string
//   type: string
//   lat:  JSON float, [-90, 90]
//   lon:  JSON float, [-180, 180]
//   time: ISO-8601 UTC string ("2026-02-21T14:00:00Z")
//   Optional numeric fields, when present, are always JSON floats (never int/null).
// Stations that violate the contract are silently dropped.
static bool ParseObservations(const wxString &json, ObservationList &out,
                              wxString &error_msg) {
    wxJSONValue root;
    wxJSONReader reader;
    int errors = reader.Parse(json, &root);
    if (errors > 0) {
        error_msg = wxT("JSON parse error");
        wxString excerpt = json.Left(300);
        wxLogError("ShipObs: JSON parse error, response: %s", excerpt);
        return false;
    }

    if (!root.HasMember(wxT("stations")) ||
        !root[wxT("stations")].IsArray()) {
        error_msg = wxT("Missing 'stations' array in response");
        wxString excerpt = json.Left(300);
        wxLogError("ShipObs: missing 'stations' array, response: %s", excerpt);
        return false;
    }

    wxJSONValue arr = root[wxT("stations")];
    int count = arr.Size();
    out.clear();
    out.reserve(count);
    int skipped = 0;

    for (int i = 0; i < count; i++) {
        wxJSONValue obj = arr[i];

        // --- Required fields ---
        if (!obj.HasMember(wxT("id")) || !obj[wxT("id")].IsString()) { skipped++; continue; }
        wxString id = obj[wxT("id")].AsString();
        if (id.IsEmpty()) { skipped++; continue; }

        if (!obj.HasMember(wxT("lat")) || !obj[wxT("lat")].IsDouble()) { skipped++; continue; }
        double lat = obj[wxT("lat")].AsDouble();
        if (lat < -90.0 || lat > 90.0) { skipped++; continue; }

        if (!obj.HasMember(wxT("lon")) || !obj[wxT("lon")].IsDouble()) { skipped++; continue; }
        double lon = obj[wxT("lon")].AsDouble();
        if (lon < -180.0 || lon > 180.0) { skipped++; continue; }

        if (!obj.HasMember(wxT("time")) || !obj[wxT("time")].IsString()) { skipped++; continue; }
        wxDateTime obs_time;
        obs_time.ParseISOCombined(obj[wxT("time")].AsString());  // Z-suffix ok, check IsValid()
        if (!obs_time.IsValid()) { skipped++; continue; }

        // --- Build station ---
        ObservationStation st;
        st.id   = id;
        st.lat  = lat;
        st.lon  = lon;
        st.time = obs_time;

        if (obj.HasMember(wxT("type")) && obj[wxT("type")].IsString())
            st.type = obj[wxT("type")].AsString();
        if (obj.HasMember(wxT("country")) && obj[wxT("country")].IsString())
            st.country = obj[wxT("country")].AsString();

        // Optional numeric fields: server contract guarantees JSON float when present
#define READ_OPT(key, field) \
        if (obj.HasMember(wxT(key)) && obj[wxT(key)].IsDouble()) \
            st.field = obj[wxT(key)].AsDouble();
        READ_OPT("wind_dir", wind_dir)
        READ_OPT("wind_spd", wind_spd)
        READ_OPT("gust",     gust)
        READ_OPT("pressure", pressure)
        READ_OPT("air_temp", air_temp)
        READ_OPT("sea_temp", sea_temp)
        READ_OPT("wave_ht",  wave_ht)
        READ_OPT("vis",      vis)
#undef READ_OPT

        out.push_back(st);
    }

    wxLogMessage("ShipObs: received %d station(s)", (int)out.size());
    if (skipped > 0)
        wxLogWarning("ShipObs: skipped %d station(s) with invalid/missing required fields", skipped);

    return true;
}

// Format a double with period decimal separator regardless of system locale.
static std::string FmtDbl(double d) {
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << std::fixed << std::setprecision(4) << d;
    return oss.str();
}

bool FetchObservations(const wxString &server_url,
                       double lat_min, double lat_max,
                       double lon_min, double lon_max,
                       const wxString &max_age,
                       const wxString &types,
                       ObservationList &out,
                       wxString &error_msg) {
    // OpenCPN viewports can have longitudes outside [-180,180] when panned
    // past the antimeridian. Clamp everything to valid server ranges.
    lat_min = std::max(-90.0, std::min(90.0, lat_min));
    lat_max = std::max(-90.0, std::min(90.0, lat_max));
    if (lon_max - lon_min >= 360.0) {
        lon_min = -180.0; lon_max = 180.0;
    } else {
        while (lon_min < -180.0) { lon_min += 360.0; lon_max += 360.0; }
        while (lon_min >  180.0) { lon_min -= 360.0; lon_max -= 360.0; }
        lon_min = std::max(-180.0, lon_min);
        lon_max = std::min( 180.0, lon_max);
    }

    std::string s_max_age = std::string(max_age.mb_str(wxConvUTF8));
    std::string s_types   = std::string(types.mb_str(wxConvUTF8));

    std::string url =
        std::string(server_url.mb_str(wxConvUTF8)) +
        "/api/v1/observations?lat_min=" + FmtDbl(lat_min) +
        "&lat_max=" + FmtDbl(lat_max) +
        "&lon_min=" + FmtDbl(lon_min) +
        "&lon_max=" + FmtDbl(lon_max) +
        "&max_age=" + s_max_age +
        "&types="   + s_types;

    wxLogMessage("ShipObs: fetch  lat=[%.4f, %.4f]  lon=[%.4f, %.4f]  age=%s  types=%s",
                 lat_min, lat_max, lon_min, lon_max,
                 s_max_age.c_str(), s_types.c_str());
    wxLogMessage("ShipObs: URL: %s", url.c_str());

    CURL *curl = curl_easy_init();
    if (!curl) {
        error_msg = wxT("Failed to initialize HTTP client");
        wxLogError("ShipObs: failed to initialize curl");
        return false;
    }

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        error_msg = wxString::Format(
            wxT("Failed to connect to server: %s"), server_url);
        wxLogError("ShipObs: fetch failed: %s", curl_easy_strerror(res));
        return false;
    }

    if (response.empty()) {
        error_msg = wxT("Empty response from server");
        wxLogError("ShipObs: HTTP %ld, empty response", http_code);
        return false;
    }

    wxLogMessage("ShipObs: HTTP %ld, %zu bytes", http_code, response.size());

    if (http_code != 200) {
        std::string excerpt = response.substr(0, 300);
        wxLogError("ShipObs: server error body: %s", excerpt.c_str());
        error_msg = wxString::Format(wxT("Server returned HTTP %ld"), http_code);
        return false;
    }

    wxString json = wxString::FromUTF8(response.c_str(), response.size());
    return ParseObservations(json, out, error_msg);
}
