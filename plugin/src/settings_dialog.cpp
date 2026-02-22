#include "settings_dialog.h"
#include "shipobs_pi.h"

#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/button.h>

enum {
    ID_SETTINGS_OK = 20001
};

BEGIN_EVENT_TABLE(SettingsDialog, wxDialog)
    EVT_BUTTON(ID_SETTINGS_OK, SettingsDialog::OnOK)
END_EVENT_TABLE()

SettingsDialog::SettingsDialog(wxWindow *parent, shipobs_pi *plugin)
    : wxDialog(parent, wxID_ANY, wxT("Ship Obs Settings"),
               wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_plugin(plugin) {

    wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);

    // Server URL
    wxStaticBoxSizer *serverSizer =
        new wxStaticBoxSizer(wxVERTICAL, this, wxT("Server"));
    serverSizer->Add(new wxStaticText(this, wxID_ANY, wxT("Server URL:")),
                     0, wxALL, 2);
    m_server_url = new wxTextCtrl(this, wxID_ANY, plugin->GetServerURL(),
                                  wxDefaultPosition, wxSize(300, -1));
    serverSizer->Add(m_server_url, 0, wxALL | wxEXPAND, 4);
    topSizer->Add(serverSizer, 0, wxALL | wxEXPAND, 4);

    // Display options
    wxStaticBoxSizer *dispSizer =
        new wxStaticBoxSizer(wxVERTICAL, this, wxT("Display"));
    m_wind_barbs = new wxCheckBox(this, wxID_ANY, wxT("Show wind barbs"));
    m_wind_barbs->SetValue(plugin->GetShowWindBarbs());
    m_labels = new wxCheckBox(this, wxID_ANY, wxT("Show station labels"));
    m_labels->SetValue(plugin->GetShowLabels());
    dispSizer->Add(m_wind_barbs, 0, wxALL, 4);
    dispSizer->Add(m_labels, 0, wxALL, 4);
    topSizer->Add(dispSizer, 0, wxALL | wxEXPAND, 4);

    // Info trigger
    wxArrayString triggers;
    triggers.Add(wxT("Hover popup"));
    triggers.Add(wxT("Double-click sticky window"));
    triggers.Add(wxT("Both"));
    m_info_trigger = new wxRadioBox(this, wxID_ANY, wxT("Station info"),
                                    wxDefaultPosition, wxDefaultSize,
                                    triggers, 1, wxRA_SPECIFY_ROWS);
    m_info_trigger->SetSelection(plugin->GetInfoMode());
    topSizer->Add(m_info_trigger, 0, wxALL | wxEXPAND, 4);

    // Buttons
    wxBoxSizer *btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->Add(new wxButton(this, ID_SETTINGS_OK, wxT("OK")), 0, wxALL, 4);
    btnSizer->Add(new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, wxALL, 4);
    topSizer->Add(btnSizer, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 4);

    SetSizer(topSizer);
    topSizer->SetSizeHints(this);
}

SettingsDialog::~SettingsDialog() {}

void SettingsDialog::OnOK(wxCommandEvent &event) {
    m_plugin->SetServerURL(m_server_url->GetValue());
    m_plugin->SetShowWindBarbs(m_wind_barbs->GetValue());
    m_plugin->SetShowLabels(m_labels->GetValue());
    m_plugin->SetInfoMode(m_info_trigger->GetSelection());

    EndModal(wxID_OK);
}
