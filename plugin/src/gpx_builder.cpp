#include "gpx_builder.h"

#include <cmath>
#include <wx/intl.h>

static wxString FmtObs(const wxString &label, double val,
                       const wxString &unit) {
  if (std::isnan(val)) return wxT("");
  return wxString::Format(wxT("%s: %.1f %s\n"), label, val, unit);
}

wxString BuildGPXString(const wxDateTime &fetched_at,
                        const ObservationList &stations) {
  wxString gpx;
  gpx += wxT("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  gpx += wxT("<gpx version=\"1.1\" creator=\"shipobs_pi\"\n");
  gpx += wxT("  xmlns=\"http://www.topografix.com/GPX/1/1\">\n");

  for (size_t i = 0; i < stations.size(); i++) {
    const ObservationStation &st = stations[i];
    if (std::isnan(st.lat) || std::isnan(st.lon)) continue;

    gpx += wxString::Format(wxT("  <wpt lat=\"%.6f\" lon=\"%.6f\">\n"), st.lat,
                            st.lon);
    gpx += wxString::Format(wxT("    <name>%s</name>\n"), st.id);
    gpx += wxT("    <sym>Float</sym>\n");

    wxString desc;
    if (st.time.IsValid())
      desc += wxString::Format(_("Timestamp: %s UTC\n"),
                               st.time.Format(wxT("%b %d, %Y %H:%M")));
    desc += wxString::Format(_("Station: %s (%s)\n"), st.id, st.type);
    if (!st.country.IsEmpty())
      desc += wxString::Format(_("Country: %s\n"), st.country);
    if (!std::isnan(st.wind_dir))
      desc += wxString::Format(_("Wind direction: %d\u00b0T\n"),
                               (int)std::round(st.wind_dir));
    if (!std::isnan(st.wind_spd))
      desc +=
          wxString::Format(_("Wind speed: %.1f kts\n"), st.wind_spd * 1.94384);
    if (!std::isnan(st.gust))
      desc += wxString::Format(_("Gust: %.1f kts\n"), st.gust * 1.94384);
    desc += FmtObs(_("Pressure"), st.pressure, wxT("hPa"));
    if (!std::isnan(st.air_temp))
      desc +=
          wxString::Format(_("Air temperature: %.1f \u00b0C\n"), st.air_temp);
    if (!std::isnan(st.sea_temp))
      desc +=
          wxString::Format(_("Sea temperature: %.1f \u00b0C\n"), st.sea_temp);
    desc += FmtObs(_("Wave height"), st.wave_ht, wxT("m"));
    desc += FmtObs(_("Visibility"), st.vis, wxT("nm"));
    desc.Trim();
    if (fetched_at.IsValid())
      desc += wxString::Format(_("\n\nFetched: %s"),
                               fetched_at.Format(wxT("%Y-%m-%dT%H:%M:%SZ")));

    gpx += wxString::Format(wxT("    <desc>%s</desc>\n"), desc);

    if (st.time.IsValid())
      gpx += wxString::Format(wxT("    <time>%s</time>\n"),
                              st.time.Format(wxT("%Y-%m-%dT%H:%M:%SZ")));
    gpx += wxT("  </wpt>\n");
  }
  gpx += wxT("</gpx>\n");
  return gpx;
}
