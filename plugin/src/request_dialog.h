#ifndef _REQUEST_DIALOG_H_
#define _REQUEST_DIALOG_H_

#include "ocpn_plugin.h"
#include "observation.h"
#include <wx/dialog.h>
#include <wx/choice.h>
#include <wx/checkbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/gauge.h>
#include <wx/listctrl.h>

class shipobs_pi;

class RequestDialog : public wxDialog {
public:
    RequestDialog(wxWindow *parent, shipobs_pi *plugin);
    ~RequestDialog();

    void UpdateViewportBounds(const PlugIn_ViewPort &vp);
    void RefreshHistory();

private:
    void OnFetch(wxCommandEvent &event);
    void OnClose(wxCommandEvent &event);
    void OnWindowClose(wxCloseEvent &event);
    void OnHistorySelected(wxListEvent &event);
    void OnSaveLayer(wxCommandEvent &event);
    void OnDeleteEntry(wxCommandEvent &event);

    shipobs_pi *m_plugin;

    wxChoice     *m_max_age;
    wxCheckBox   *m_chk_ship;
    wxCheckBox   *m_chk_buoy;
    wxCheckBox   *m_chk_shore;
    wxCheckBox   *m_chk_drifter;
    wxCheckBox   *m_chk_other;
    wxTextCtrl   *m_area_ctrl;
    wxStaticText *m_status_label;
    wxGauge      *m_progress;
    wxListCtrl   *m_history_list;
    wxButton     *m_save_layer_btn;
    wxButton     *m_delete_entry_btn;

    double m_lat_min, m_lat_max, m_lon_min, m_lon_max;

    DECLARE_EVENT_TABLE()
};

#endif // _REQUEST_DIALOG_H_
