#include "settings_dialog.h"
#include "shipobs_pi.h"

#include <wx/intl.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/button.h>

enum { ID_SETTINGS_OK = 20001 };

BEGIN_EVENT_TABLE(SettingsDialog, wxDialog)
EVT_BUTTON(ID_SETTINGS_OK, SettingsDialog::OnOK)
END_EVENT_TABLE()

SettingsDialog::SettingsDialog(wxWindow *parent, shipobs_pi *plugin)
    : wxDialog(parent, wxID_ANY, _("Ship Obs Settings"), wxDefaultPosition,
               wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_plugin(plugin) {
  wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);

  wxStaticBoxSizer *serverSizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Server"));
  serverSizer->Add(new wxStaticText(this, wxID_ANY, _("Server URL:")), 0, wxALL,
                   2);
  m_server_url = new wxTextCtrl(this, wxID_ANY, plugin->GetServerURL(),
                                wxDefaultPosition, wxSize(300, -1));
  serverSizer->Add(m_server_url, 0, wxALL | wxEXPAND, 4);
  topSizer->Add(serverSizer, 0, wxALL | wxEXPAND, 4);

  wxStaticBoxSizer *dispSizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Display"));
  m_wind_barbs = new wxCheckBox(this, wxID_ANY, _("Show wind barbs"));
  m_wind_barbs->SetValue(plugin->GetShowWindBarbs());
  m_labels = new wxCheckBox(this, wxID_ANY, _("Show station labels"));
  m_labels->SetValue(plugin->GetShowLabels());
  dispSizer->Add(m_wind_barbs, 0, wxALL, 4);
  dispSizer->Add(m_labels, 0, wxALL, 4);
  topSizer->Add(dispSizer, 0, wxALL | wxEXPAND, 4);

  wxArrayString triggers;
  triggers.Add(_("Hover popup"));
  triggers.Add(_("Double-click sticky window"));
  triggers.Add(_("Both"));
  m_info_trigger =
      new wxRadioBox(this, wxID_ANY, _("Station info"), wxDefaultPosition,
                     wxDefaultSize, triggers, 1, wxRA_SPECIFY_ROWS);
  m_info_trigger->SetSelection(plugin->GetInfoMode());
  topSizer->Add(m_info_trigger, 0, wxALL | wxEXPAND, 4);

  wxBoxSizer *btnSizer = new wxBoxSizer(wxHORIZONTAL);
  btnSizer->Add(new wxButton(this, ID_SETTINGS_OK, _("OK")), 0, wxALL, 4);
  btnSizer->Add(new wxButton(this, wxID_CANCEL, _("Cancel")), 0, wxALL, 4);
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
