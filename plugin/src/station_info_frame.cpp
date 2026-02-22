#include "station_info_frame.h"
#include "shipobs_pi.h"

#include <cmath>
#include <wx/intl.h>
#include <wx/sizer.h>

BEGIN_EVENT_TABLE(StationInfoFrame, wxFrame)
    EVT_CLOSE(StationInfoFrame::OnClose)
    EVT_MOVE(StationInfoFrame::OnMove)
    EVT_ACTIVATE(StationInfoFrame::OnActivate)
    EVT_ENTER_WINDOW(StationInfoFrame::OnMouseEnter)
    EVT_LEAVE_WINDOW(StationInfoFrame::OnMouseLeave)
END_EVENT_TABLE()

StationInfoFrame::StationInfoFrame(wxWindow *parent, shipobs_pi *plugin,
                                   const ObservationStation &st,
                                   const wxPoint &station_screen)
    : wxFrame(parent, wxID_ANY, st.id,
              wxDefaultPosition, wxDefaultSize,
              wxFRAME_TOOL_WINDOW | wxFRAME_FLOAT_ON_PARENT |
              wxCAPTION | wxCLOSE_BOX | wxFRAME_NO_TASKBAR),
      m_plugin(plugin),
      m_station_id(st.id),
      m_lat(st.lat),
      m_lon(st.lon),
      m_last_station_px(station_screen),
      m_repositioning(false),
      m_is_active(false),
      m_is_hovered(false) {

    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
    m_text = new wxStaticText(this, wxID_ANY, wxEmptyString,
                              wxDefaultPosition, wxDefaultSize,
                              wxALIGN_LEFT);
    sizer->Add(m_text, 1, wxALL | wxEXPAND, 8);
    SetSizer(sizer);

    // Build content
    wxString info;
    if (st.time.IsValid())
        info += wxString::Format(wxT("%s UTC\n"),
                                 st.time.Format(wxT("%Y-%m-%d %H:%M")));
    info += wxString::Format(_("Station: %s  [%s]\n"), st.id, st.type);
    if (!st.country.IsEmpty())
        info += wxString::Format(_("Country: %s\n"), st.country);
    info += wxString::Format(_("Pos: %.3f\u00b0, %.3f\u00b0\n"), st.lat, st.lon);
    info += wxT("\n");

    if (!std::isnan(st.wind_dir) || !std::isnan(st.wind_spd)) {
        wxString dir = std::isnan(st.wind_dir) ? wxString(_("--"))
            : wxString::Format(wxT("%d\u00b0T"), (int)std::round(st.wind_dir));
        wxString spd = std::isnan(st.wind_spd) ? wxString(_("--"))
            : wxString::Format(_("%.1f kts"), st.wind_spd * 1.94384);
        info += wxString::Format(_("Wind: %s @ %s"), dir, spd);
        if (!std::isnan(st.gust))
            info += wxString::Format(_("  Gust: %.1f kts"), st.gust * 1.94384);
        info += wxT("\n");
    }
    if (!std::isnan(st.pressure))
        info += wxString::Format(_("Pressure: %.1f hPa\n"), st.pressure);
    if (!std::isnan(st.air_temp))
        info += wxString::Format(_("Air temp: %.1f \u00b0C\n"), st.air_temp);
    if (!std::isnan(st.sea_temp))
        info += wxString::Format(_("Sea temp: %.1f \u00b0C\n"), st.sea_temp);
    if (!std::isnan(st.wave_ht))
        info += wxString::Format(_("Wave ht: %.1f m\n"), st.wave_ht);
    if (!std::isnan(st.vis))
        info += wxString::Format(_("Visibility: %.1f nm\n"), st.vis);

    m_text->SetLabel(info);
    GetSizer()->Fit(this);

    // Default offset: slightly right of and above the station marker
    m_offset = wxPoint(20, -(GetSize().GetHeight() + 10));
    Move(station_screen + m_offset);
    Show();
}

StationInfoFrame::~StationInfoFrame() {}

void StationInfoFrame::Reposition(const wxPoint &station_screen) {
    wxPoint new_pos = station_screen + m_offset;
    m_last_station_px = station_screen;
    if (GetPosition() == new_pos) return;  // chart hasn't moved, skip Move()
    m_repositioning = true;
    Move(new_pos);
    m_repositioning = false;
}

void StationInfoFrame::OnActivate(wxActivateEvent &event) {
    bool was = IsHighlighted();
    m_is_active = event.GetActive();
    if (IsHighlighted() != was)
        RequestRefresh(m_plugin->GetParentWindow());
    event.Skip();
}

void StationInfoFrame::OnMouseEnter(wxMouseEvent &event) {
    if (!m_is_hovered) {
        m_is_hovered = true;
        RequestRefresh(m_plugin->GetParentWindow());
    }
    event.Skip();
}

void StationInfoFrame::OnMouseLeave(wxMouseEvent &event) {
    if (m_is_hovered) {
        m_is_hovered = false;
        RequestRefresh(m_plugin->GetParentWindow());
    }
    event.Skip();
}

void StationInfoFrame::OnClose(wxCloseEvent &event) {
    m_plugin->RemoveInfoFrame(this);
    event.Skip();
}

void StationInfoFrame::OnMove(wxMoveEvent &event) {
    if (!m_repositioning)
        m_offset = GetPosition() - m_last_station_px;
    event.Skip();
}
