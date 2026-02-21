#ifndef _SETTINGS_DIALOG_H_
#define _SETTINGS_DIALOG_H_

#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/checkbox.h>
#include <wx/radiobox.h>

class shipobs_pi;

class SettingsDialog : public wxDialog {
public:
    SettingsDialog(wxWindow *parent, shipobs_pi *plugin);
    ~SettingsDialog();

private:
    void OnOK(wxCommandEvent &event);

    shipobs_pi *m_plugin;

    wxTextCtrl *m_server_url;
    wxCheckBox *m_wind_barbs;
    wxCheckBox *m_labels;
    wxRadioBox *m_info_trigger;

    DECLARE_EVENT_TABLE()
};

#endif // _SETTINGS_DIALOG_H_
