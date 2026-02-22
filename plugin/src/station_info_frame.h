#ifndef _STATION_INFO_FRAME_H_
#define _STATION_INFO_FRAME_H_

#include "observation.h"
#include <wx/frame.h>
#include <wx/stattext.h>

class shipobs_pi;

class StationInfoFrame : public wxFrame {
public:
    StationInfoFrame(wxWindow *parent, shipobs_pi *plugin,
                     const ObservationStation &st,
                     const wxPoint &station_screen);
    ~StationInfoFrame();

    const wxString &GetStationId() const { return m_station_id; }
    double GetLat() const { return m_lat; }
    double GetLon() const { return m_lon; }

    // Called from the render loop to track the station as the chart moves.
    // station_screen is the station's current screen pixel position.
    void Reposition(const wxPoint &station_screen);

    // True when the frame is active (focused) or mouse is over it — used by the
    // render loop to draw a yellow highlight on the corresponding marker.
    bool IsHighlighted() const { return m_is_active || m_is_hovered; }

private:
    void OnClose(wxCloseEvent &event);
    void OnMove(wxMoveEvent &event);
    void OnActivate(wxActivateEvent &event);
    void OnMouseEnter(wxMouseEvent &event);
    void OnMouseLeave(wxMouseEvent &event);

    shipobs_pi   *m_plugin;
    wxString      m_station_id;
    double        m_lat;
    double        m_lon;
    wxStaticText *m_text;
    wxPoint       m_offset;          // frame position relative to station screen pos
    wxPoint       m_last_station_px; // station screen pos from last Reposition call
    bool          m_repositioning;   // true while Reposition() is calling Move()
    bool          m_is_active;       // frame has OS focus
    bool          m_is_hovered;      // mouse is over the frame

    DECLARE_EVENT_TABLE()
};

#endif // _STATION_INFO_FRAME_H_
