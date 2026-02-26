#ifndef _STATION_INFO_FRAME_H_
#define _STATION_INFO_FRAME_H_

#include "observation.h"
#include <wx/frame.h>
#include <wx/stattext.h>

class shipobs_pi;

class StationInfoFrame : public wxFrame {
public:
  StationInfoFrame(wxWindow *parent, shipobs_pi *plugin,
                   const ObservationStation &st, const wxPoint &station_screen);
  ~StationInfoFrame();

  const wxString &GetStationId() const { return m_station_id; }
  double GetLat() const { return m_lat; }
  double GetLon() const { return m_lon; }

  void Reposition(const wxPoint &station_screen);
  bool IsHighlighted() const { return m_is_active || m_is_hovered; }

private:
  void OnClose(wxCloseEvent &event);
  void OnMove(wxMoveEvent &event);
  void OnActivate(wxActivateEvent &event);
  void OnMouseEnter(wxMouseEvent &event);
  void OnMouseLeave(wxMouseEvent &event);

  shipobs_pi *m_plugin;
  wxString m_station_id;
  double m_lat;
  double m_lon;
  wxStaticText *m_text;
  wxPoint m_offset;
  wxPoint m_last_station_px;
  bool m_repositioning;
  bool m_is_active;
  bool m_is_hovered;

  DECLARE_EVENT_TABLE()
};

#endif  // _STATION_INFO_FRAME_H_
