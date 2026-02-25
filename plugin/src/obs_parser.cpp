#include "obs_parser.h"

#include <wx/jsonreader.h>
#include <wx/jsonval.h>
#include <wx/log.h>

bool ParseObservations(const wxString &json, ObservationList &out,
                       wxString &error_msg) {
    wxJSONValue root;
    wxJSONReader reader;
    int errors = reader.Parse(json, &root);
    if (errors > 0) {
        error_msg = _("JSON parse error");
        wxString excerpt = json.Left(300);
        wxLogError("ShipObs: JSON parse error, response: %s", excerpt);
        return false;
    }

    if (!root.HasMember(wxT("stations")) ||
        !root[wxT("stations")].IsArray()) {
        error_msg = _("Missing 'stations' array in response");
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
        obs_time.ParseISOCombined(obj[wxT("time")].AsString());
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

    if (skipped > 0)
        wxLogWarning("ShipObs: skipped %d station(s) with invalid/missing required fields", skipped);

    return true;
}
