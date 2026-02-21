#include "station_popup.h"
#include "shipobs_pi.h"
#include "observation.h"

#include <cmath>
#include <wx/sizer.h>

static const int HIT_RADIUS = 15;  // pixels

// Format a double value with units, or "--" if NaN
static wxString FmtVal(double v, const wxString &units, int decimals = 1) {
    if (std::isnan(v)) return wxT("--");
    return wxString::Format(wxT("%.*f %s"), decimals, v, units);
}

// ---------- StationPopup ----------

StationPopup::StationPopup(wxWindow *parent)
    : wxPopupWindow(parent, wxBORDER_SIMPLE) {
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
    m_text = new wxStaticText(this, wxID_ANY, wxEmptyString);
    m_text->SetForegroundColour(*wxBLACK);
    m_text->SetBackgroundColour(wxColour(255, 255, 225));  // light yellow
    sizer->Add(m_text, 1, wxALL | wxEXPAND, 6);
    SetSizer(sizer);
    SetBackgroundColour(wxColour(255, 255, 225));
}

StationPopup::~StationPopup() {}

void StationPopup::ShowStation(const ObservationStation &st,
                               const wxPoint &screen_pos) {
    wxString info;
    if (st.time.IsValid())
        info += wxString::Format(wxT("%s UTC\n"),
                                 st.time.Format(wxT("%Y-%m-%d %H:%M")));
    info += wxString::Format(wxT("Station: %s  [%s]\n"), st.id, st.type);
    if (!st.country.IsEmpty())
        info += wxString::Format(wxT("Country: %s\n"), st.country);
    info += wxString::Format(wxT("Pos: %.3f, %.3f\n"), st.lat, st.lon);
    info += wxT("\n");

    if (!std::isnan(st.wind_dir) || !std::isnan(st.wind_spd))
        info += wxString::Format(wxT("Wind: %s @ %s"),
                                 FmtVal(st.wind_dir, wxT("deg"), 0),
                                 FmtVal(st.wind_spd, wxT("kt")));
    if (!std::isnan(st.gust))
        info += wxString::Format(wxT("  Gust: %s"), FmtVal(st.gust, wxT("kt")));
    if (!std::isnan(st.wind_dir) || !std::isnan(st.wind_spd) ||
        !std::isnan(st.gust))
        info += wxT("\n");

    if (!std::isnan(st.pressure))
        info += wxString::Format(wxT("Pressure: %s\n"),
                                 FmtVal(st.pressure, wxT("hPa")));
    if (!std::isnan(st.air_temp))
        info += wxString::Format(wxT("Air Temp: %s\n"),
                                 FmtVal(st.air_temp, wxT("C")));
    if (!std::isnan(st.sea_temp))
        info += wxString::Format(wxT("Sea Temp: %s\n"),
                                 FmtVal(st.sea_temp, wxT("C")));
    if (!std::isnan(st.wave_ht))
        info += wxString::Format(wxT("Wave Ht: %s\n"),
                                 FmtVal(st.wave_ht, wxT("m")));
    if (!std::isnan(st.vis))
        info += wxString::Format(wxT("Visibility: %s\n"),
                                 FmtVal(st.vis, wxT("nm")));

    m_text->SetLabel(info);
    GetSizer()->Fit(this);

    // Position popup offset from cursor
    SetPosition(wxPoint(screen_pos.x + 15, screen_pos.y + 15));
    Show();
}

// ---------- Mouse event handler ----------

bool HandleStationPopup(shipobs_pi *plugin, wxMouseEvent &event,
                        double cursor_lat, double cursor_lon,
                        const PlugIn_ViewPort &vp,
                        StationPopup *&popup,
                        wxWindow *parent) {
    const ObservationList &stations = plugin->GetStations();
    bool hover_mode = plugin->GetHoverMode();

    // Determine if we should check for a hit
    bool check_hit = false;
    if (hover_mode && event.Moving()) {
        check_hit = true;
    } else if (!hover_mode && event.LeftDown()) {
        check_hit = true;
    }

    if (!check_hit) return false;
    if (stations.empty()) return false;

    // Convert cursor screen position to pixel coords for distance check
    wxPoint cursor_px = event.GetPosition();

    // Find nearest station within HIT_RADIUS
    int best_idx = -1;
    double best_dist_sq = HIT_RADIUS * HIT_RADIUS;

    PlugIn_ViewPort vp_copy = vp;
    for (size_t i = 0; i < stations.size(); i++) {
        const ObservationStation &st = stations[i];
        if (std::isnan(st.lat) || std::isnan(st.lon)) continue;

        wxPoint st_px;
        GetCanvasPixLL(&vp_copy, &st_px, st.lat, st.lon);

        double dx = cursor_px.x - st_px.x;
        double dy = cursor_px.y - st_px.y;
        double dist_sq = dx * dx + dy * dy;

        if (dist_sq < best_dist_sq) {
            best_dist_sq = dist_sq;
            best_idx = static_cast<int>(i);
        }
    }

    if (best_idx >= 0) {
        if (!popup)
            popup = new StationPopup(parent);

        // Convert to screen coords for popup positioning
        wxPoint screen_pos = parent->ClientToScreen(cursor_px);
        popup->ShowStation(stations[best_idx], screen_pos);

        if (!hover_mode) return true;  // consume click event
    } else {
        // Hide popup if cursor moved away from all stations
        if (popup && popup->IsShown())
            popup->Hide();
    }

    return false;  // don't consume mouse movement
}
