#ifndef _OBSERVATION_H_
#define _OBSERVATION_H_

#include <cmath>
#include <vector>
#include <wx/datetime.h>
#include <wx/string.h>

struct ObservationStation {
    wxString id;       // Station identifier (e.g., "KBOS", "44013")
    wxString type;     // Platform type: "ship", "buoy", "shore", "drifter", "other"
    wxString country;  // ISO country code

    double lat;
    double lon;
    wxDateTime time;   // Observation time (UTC)

    // Meteorological / oceanographic values. NaN means missing.
    // Units match the server API (SI/metric). Convert to display units in the UI layer.
    double wind_dir;   // degrees true
    double wind_spd;   // m/s
    double gust;       // m/s
    double pressure;   // hPa (mbar)
    double air_temp;   // degrees C
    double sea_temp;   // degrees C
    double wave_ht;    // metres
    double vis;        // metres

    ObservationStation()
        : lat(NAN), lon(NAN),
          wind_dir(NAN), wind_spd(NAN), gust(NAN),
          pressure(NAN), air_temp(NAN), sea_temp(NAN),
          wave_ht(NAN), vis(NAN) {}
};

typedef std::vector<ObservationStation> ObservationList;

struct FetchRecord {
    wxString label;       // display name (defaults to fetch time)
    wxDateTime fetched_at;
    double lat_min, lat_max, lon_min, lon_max;
    size_t station_count; // for display; stations are on disk, not in memory
    FetchRecord() : lat_min(0), lat_max(0), lon_min(0), lon_max(0),
                    station_count(0) {}
};

typedef std::vector<FetchRecord> FetchHistory;

#endif // _OBSERVATION_H_
