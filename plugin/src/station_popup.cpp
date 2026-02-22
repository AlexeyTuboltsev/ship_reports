#include "station_popup.h"
#include "shipobs_pi.h"
#include "station_info_frame.h"
#include "observation.h"

#include <cmath>
#include <wx/sizer.h>

static const int HIT_RADIUS = 15;  // pixels

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
                                 st.time.Format(wxT("%b %d, %Y %H:%M")));
    info += wxString::Format(wxT("Station: %s  [%s]\n"), st.id, st.type);
    if (!st.country.IsEmpty())
        info += wxString::Format(wxT("Country: %s\n"), st.country);
    info += wxString::Format(wxT("Pos: %.3f\u00b0, %.3f\u00b0\n"),
                             st.lat, st.lon);
    info += wxT("\n");

    if (!std::isnan(st.wind_dir) || !std::isnan(st.wind_spd)) {
        wxString dir = std::isnan(st.wind_dir) ? wxString(wxT("--"))
            : wxString::Format(wxT("%d\u00b0T"), (int)std::round(st.wind_dir));
        wxString spd = std::isnan(st.wind_spd) ? wxString(wxT("--"))
            : wxString::Format(wxT("%.1f kts"), st.wind_spd * 1.94384);
        info += wxString::Format(wxT("Wind: %s @ %s"), dir, spd);
        if (!std::isnan(st.gust))
            info += wxString::Format(wxT("  Gust: %.1f kts"),
                                     st.gust * 1.94384);
        info += wxT("\n");
    }
    if (!std::isnan(st.pressure))
        info += wxString::Format(wxT("Pressure: %.1f hPa\n"), st.pressure);
    if (!std::isnan(st.air_temp))
        info += wxString::Format(wxT("Air temp: %.1f \u00b0C\n"), st.air_temp);
    if (!std::isnan(st.sea_temp))
        info += wxString::Format(wxT("Sea temp: %.1f \u00b0C\n"), st.sea_temp);
    if (!std::isnan(st.wave_ht))
        info += wxString::Format(wxT("Wave ht: %.1f m\n"), st.wave_ht);
    if (!std::isnan(st.vis))
        info += wxString::Format(wxT("Visibility: %.1f nm\n"), st.vis);

    m_text->SetLabel(info);
    GetSizer()->Fit(this);

    SetPosition(wxPoint(screen_pos.x + 15, screen_pos.y + 15));
    Show();
}

// ---------- Mouse event handler ----------

// Returns the index of the nearest station within HIT_RADIUS of cursor_px,
// or -1 if none found. If found and st_screen_out is non-null, sets it to
// the station's screen-coordinate position.
static int FindNearestStation(const ObservationList &stations,
                              const PlugIn_ViewPort &vp,
                              const wxPoint &cursor_px,
                              wxWindow *parent,
                              wxPoint *st_screen_out) {
    int best_idx = -1;
    double best_dist_sq = HIT_RADIUS * HIT_RADIUS;
    wxPoint best_st_px;

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
            best_st_px = st_px;
        }
    }

    if (best_idx >= 0 && st_screen_out)
        *st_screen_out = parent->ClientToScreen(best_st_px);

    return best_idx;
}

bool HandleStationPopup(shipobs_pi *plugin, wxMouseEvent &event,
                        double cursor_lat, double cursor_lon,
                        const PlugIn_ViewPort &vp,
                        StationPopup *&popup,
                        wxWindow *parent) {
    const ObservationList &stations = plugin->GetStations();
    int info_mode = plugin->GetInfoMode();  // 0=hover, 1=dblclick, 2=both
    wxPoint cursor_px = event.GetPosition();

    bool want_hover   = (info_mode == 0 || info_mode == 2);
    bool want_dblclick = (info_mode == 1 || info_mode == 2);

    // Double-click → open (or raise) a sticky info frame
    if (want_dblclick && event.LeftDClick()) {
        if (!stations.empty()) {
            wxPoint st_screen;
            int idx = FindNearestStation(stations, vp, cursor_px, parent,
                                         &st_screen);
            if (idx >= 0) {
                plugin->OpenOrFocusInfoFrame(stations[idx], st_screen);
                return true;  // consume event
            }
        }
        return false;
    }

    // Hover popup
    if (want_hover && event.Moving()) {
        if (stations.empty()) return false;

        wxPoint st_screen;
        int best_idx = FindNearestStation(stations, vp, cursor_px, parent,
                                          &st_screen);
        if (best_idx >= 0) {
            if (!popup)
                popup = new StationPopup(parent);
            wxPoint screen_pos = parent->ClientToScreen(cursor_px);
            popup->ShowStation(stations[best_idx], screen_pos);
        } else {
            if (popup && popup->IsShown())
                popup->Hide();
        }
    } else if (!want_hover && popup && popup->IsShown()) {
        // hover disabled — hide any leftover popup
        popup->Hide();
    }

    return false;
}
