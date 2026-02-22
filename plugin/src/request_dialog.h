#ifndef _REQUEST_DIALOG_H_
#define _REQUEST_DIALOG_H_

#include "ocpn_plugin.h"
#include "observation.h"
#include <wx/dialog.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/choice.h>
#include <wx/checkbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/listctrl.h>
#include <wx/statline.h>

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
    void OnGetFromViewport(wxCommandEvent &event);
    void OnSize(wxSizeEvent &event);

    void PopulateAreaControls();
    void AdjustColumns();

    shipobs_pi *m_plugin;

    wxNotebook *m_notebook;

    // Tab 1 – Ship Reports
    wxListCtrl *m_history_list;
    wxButton   *m_save_layer_btn;
    wxButton   *m_delete_entry_btn;

    // Tab 2 – Fetch new
    wxChoice     *m_max_age;
    wxCheckBox   *m_chk_ship;
    wxCheckBox   *m_chk_buoy;
    wxCheckBox   *m_chk_shore;
    wxCheckBox   *m_chk_drifter;
    wxCheckBox   *m_chk_other;
    wxTextCtrl   *m_lat_min_ctrl;
    wxTextCtrl   *m_lat_max_ctrl;
    wxTextCtrl   *m_lon_min_ctrl;
    wxTextCtrl   *m_lon_max_ctrl;
    wxStaticText *m_status_label;

    double m_lat_min, m_lat_max, m_lon_min, m_lon_max;

    DECLARE_EVENT_TABLE()
};

#endif // _REQUEST_DIALOG_H_
