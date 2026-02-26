#ifndef _STATION_POPUP_H_
#define _STATION_POPUP_H_

#include "ocpn_plugin.h"
#include <wx/popupwin.h>
#include <wx/stattext.h>

class shipobs_pi;
struct ObservationStation;

class StationPopup : public wxPopupWindow {
public:
  StationPopup(wxWindow *parent);
  ~StationPopup();

  void ShowStation(const ObservationStation &st, const wxPoint &pos);

private:
  wxStaticText *m_text;
};

bool HandleStationPopup(shipobs_pi *plugin, wxMouseEvent &event,
                        double cursor_lat, double cursor_lon,
                        const PlugIn_ViewPort &vp, StationPopup *&popup,
                        wxWindow *parent);

#endif  // _STATION_POPUP_H_
