#include "request_dialog.h"
#include "shipobs_pi.h"
#include "server_client.h"

#include <wx/sizer.h>
#include <wx/arrstr.h>
#include <wx/file.h>
#include <wx/msgdlg.h>
#include <wx/filedlg.h>
#include <wx/settings.h>
#include <wx/html/htmlwin.h>
#include <wx/utils.h>
#include "info_html.h"
#include <sstream>
#include <iomanip>
#include <cmath>

enum {
    ID_FETCH = 10001,
    ID_CLOSE_BTN,
    ID_HISTORY_LIST,
    ID_EXPORT_GPX,
    ID_DELETE_ENTRY,
    ID_GET_VIEWPORT,
    ID_LAT_MIN,
    ID_LAT_MAX,
    ID_LON_MIN,
    ID_LON_MAX
};

BEGIN_EVENT_TABLE(ShipReportsPluginDialog, wxDialog)
    EVT_BUTTON(ID_FETCH,        ShipReportsPluginDialog::OnFetch)
    EVT_BUTTON(ID_CLOSE_BTN,    ShipReportsPluginDialog::OnClose)
    EVT_CLOSE(                  ShipReportsPluginDialog::OnWindowClose)
    EVT_LIST_ITEM_SELECTED(ID_HISTORY_LIST, ShipReportsPluginDialog::OnHistorySelected)
    EVT_BUTTON(ID_EXPORT_GPX,   ShipReportsPluginDialog::OnExportGPX)
    EVT_BUTTON(ID_DELETE_ENTRY, ShipReportsPluginDialog::OnDeleteEntry)
    EVT_BUTTON(ID_GET_VIEWPORT, ShipReportsPluginDialog::OnGetFromViewport)
    EVT_SIZE(ShipReportsPluginDialog::OnSize)
END_EVENT_TABLE()


ShipReportsPluginDialog::ShipReportsPluginDialog(wxWindow *parent, shipobs_pi *plugin)
    : wxDialog(parent, wxID_ANY, wxT("Ship Reports"),
               wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_plugin(plugin),
      m_lat_min(-90), m_lat_max(90),
      m_lon_min(-180), m_lon_max(180) {

    wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);

    m_notebook = new wxNotebook(this, wxID_ANY);

    // ── Tab 1: Ship Reports ───────────────────────────────────────────────────

    wxPanel *p1 = new wxPanel(m_notebook, wxID_ANY);
    wxBoxSizer *p1Sizer = new wxBoxSizer(wxVERTICAL);

    m_history_list = new wxListCtrl(p1, ID_HISTORY_LIST,
                                    wxDefaultPosition, wxDefaultSize,
                                    wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_SUNKEN);
    m_history_list->InsertColumn(0, wxT("Time (UTC)"), wxLIST_FORMAT_LEFT,  140);
    m_history_list->InsertColumn(1, wxT("Area"),        wxLIST_FORMAT_LEFT,  100);
    m_history_list->InsertColumn(2, wxT("Objects"),      wxLIST_FORMAT_RIGHT,  60);
    p1Sizer->Add(m_history_list, 1, wxALL | wxEXPAND, 6);

    wxBoxSizer *p1BtnSizer = new wxBoxSizer(wxHORIZONTAL);
    m_delete_entry_btn = new wxButton(p1, ID_DELETE_ENTRY, wxT("Delete"));
    m_delete_entry_btn->Enable(false);
    p1BtnSizer->Add(m_delete_entry_btn, 0, wxLEFT | wxBOTTOM, 6);
    p1BtnSizer->AddStretchSpacer();
    m_export_gpx_btn = new wxButton(p1, ID_EXPORT_GPX, wxT("Export as GPX"));
    m_export_gpx_btn->Enable(false);
    p1BtnSizer->Add(m_export_gpx_btn, 0, wxRIGHT | wxBOTTOM, 6);
    p1Sizer->Add(p1BtnSizer, 0, wxEXPAND);

    p1->SetSizer(p1Sizer);
    m_notebook->AddPage(p1, wxT("Ship Reports"));

    // ── Tab 2: Fetch new ──────────────────────────────────────────────────────

    wxPanel *p2 = new wxPanel(m_notebook, wxID_ANY);
    wxBoxSizer *p2Sizer = new wxBoxSizer(wxVERTICAL);

    // Max Observation Age
    p2Sizer->Add(new wxStaticText(p2, wxID_ANY, wxT("Max Observation Age")),
                 0, wxLEFT | wxTOP, 8);
    wxArrayString ages;
    ages.Add(wxT("1h")); ages.Add(wxT("3h")); ages.Add(wxT("6h"));
    ages.Add(wxT("12h")); ages.Add(wxT("24h"));
    m_max_age = new wxChoice(p2, wxID_ANY, wxDefaultPosition, wxDefaultSize, ages);
    m_max_age->SetSelection(2);  // default 6h
    p2Sizer->Add(m_max_age, 0, wxALL | wxEXPAND, 6);

    // Platform Types
    p2Sizer->Add(new wxStaticText(p2, wxID_ANY, wxT("Platform Types")),
                 0, wxLEFT | wxTOP, 8);
    m_chk_ship    = new wxCheckBox(p2, wxID_ANY, wxT("Ships"));
    m_chk_buoy    = new wxCheckBox(p2, wxID_ANY, wxT("Buoys"));
    m_chk_shore   = new wxCheckBox(p2, wxID_ANY, wxT("Shore Stations"));
    m_chk_drifter = new wxCheckBox(p2, wxID_ANY, wxT("Drifters"));
    m_chk_other   = new wxCheckBox(p2, wxID_ANY, wxT("Other"));
    m_chk_ship->SetValue(true);
    m_chk_buoy->SetValue(true);
    wxFlexGridSizer *chkGrid = new wxFlexGridSizer(3, 2, 2, 16);
    chkGrid->Add(m_chk_ship,    0, wxALIGN_CENTER_VERTICAL);
    chkGrid->Add(m_chk_buoy,    0, wxALIGN_CENTER_VERTICAL);
    chkGrid->Add(m_chk_shore,   0, wxALIGN_CENTER_VERTICAL);
    chkGrid->Add(m_chk_drifter, 0, wxALIGN_CENTER_VERTICAL);
    chkGrid->Add(m_chk_other,   0, wxALIGN_CENTER_VERTICAL);
    chkGrid->AddSpacer(0);
    p2Sizer->Add(chkGrid, 0, wxALL | wxEXPAND, 6);

    // Area
    p2Sizer->Add(new wxStaticText(p2, wxID_ANY, wxT("Area")),
                 0, wxLEFT | wxTOP, 8);

    wxSize coordSize(80, -1);
    m_lat_min_ctrl = new wxTextCtrl(p2, ID_LAT_MIN, wxT(""), wxDefaultPosition, coordSize);
    m_lat_max_ctrl = new wxTextCtrl(p2, ID_LAT_MAX, wxT(""), wxDefaultPosition, coordSize);
    m_lon_min_ctrl = new wxTextCtrl(p2, ID_LON_MIN, wxT(""), wxDefaultPosition, coordSize);
    m_lon_max_ctrl = new wxTextCtrl(p2, ID_LON_MAX, wxT(""), wxDefaultPosition, coordSize);

    // Coordinate grid: header row + from row + to row
    wxFlexGridSizer *coordGrid = new wxFlexGridSizer(3, 3, 4, 6);
    coordGrid->AddGrowableCol(1, 1);
    coordGrid->AddGrowableCol(2, 1);
    coordGrid->AddSpacer(0);
    coordGrid->Add(new wxStaticText(p2, wxID_ANY, wxT("Lat")), 0, wxALIGN_CENTER);
    coordGrid->Add(new wxStaticText(p2, wxID_ANY, wxT("Lon")), 0, wxALIGN_CENTER);
    coordGrid->Add(new wxStaticText(p2, wxID_ANY, wxT("from")), 0, wxALIGN_CENTER_VERTICAL);
    coordGrid->Add(m_lat_min_ctrl, 1, wxEXPAND);
    coordGrid->Add(m_lon_min_ctrl, 1, wxEXPAND);
    coordGrid->Add(new wxStaticText(p2, wxID_ANY, wxT("to")), 0, wxALIGN_CENTER_VERTICAL);
    coordGrid->Add(m_lat_max_ctrl, 1, wxEXPAND);
    coordGrid->Add(m_lon_max_ctrl, 1, wxEXPAND);

    wxButton *btn_vp = new wxButton(p2, ID_GET_VIEWPORT, wxT("Get from\nViewport"));
    wxBoxSizer *areaHbox = new wxBoxSizer(wxHORIZONTAL);
    areaHbox->Add(coordGrid, 1, wxEXPAND);
    areaHbox->Add(btn_vp, 0, wxEXPAND | wxLEFT, 6);
    p2Sizer->Add(areaHbox, 0, wxALL | wxEXPAND, 6);

    m_coord_error = new wxStaticText(p2, wxID_ANY,
                                     wxT("Latitude: \u221290 to 90  \u00b7  Longitude: \u2212180 to 180"));
    m_coord_error->SetForegroundColour(
        wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    p2Sizer->Add(m_coord_error, 0, wxLEFT | wxBOTTOM | wxEXPAND, 6);

    // Flexible space above Fetch button (min 12px so it can't collapse to zero)
    p2Sizer->Add(0, 12, 1);

    m_fetch_btn = new wxButton(p2, ID_FETCH, wxT("Fetch"));
    p2Sizer->Add(m_fetch_btn, 0, wxLEFT | wxRIGHT | wxEXPAND, 8);

    // Flexible space below Fetch button (min 12px)
    p2Sizer->Add(0, 12, 1);

    m_status_label = new wxStaticText(p2, wxID_ANY, wxT("Ready"));
    p2Sizer->Add(m_status_label, 0, wxALL | wxEXPAND, 6);

    p2->SetSizer(p2Sizer);
    m_notebook->AddPage(p2, wxT("Fetch new"));

    // ── Tab 3: Settings ───────────────────────────────────────────────────────

    wxPanel *p3 = new wxPanel(m_notebook, wxID_ANY);
    wxBoxSizer *p3Sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticBoxSizer *serverBox =
        new wxStaticBoxSizer(wxVERTICAL, p3, wxT("Server"));
    serverBox->Add(new wxStaticText(p3, wxID_ANY, wxT("Server URL:")),
                   0, wxALL, 2);
    m_settings_url = new wxTextCtrl(p3, wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxSize(300, -1));
    serverBox->Add(m_settings_url, 0, wxALL | wxEXPAND, 4);
    p3Sizer->Add(serverBox, 0, wxALL | wxEXPAND, 6);

    wxStaticBoxSizer *dispBox =
        new wxStaticBoxSizer(wxVERTICAL, p3, wxT("Display"));
    m_settings_wind_barbs = new wxCheckBox(p3, wxID_ANY, wxT("Show wind barbs"));
    m_settings_labels     = new wxCheckBox(p3, wxID_ANY, wxT("Show station labels"));
    dispBox->Add(m_settings_wind_barbs, 0, wxALL, 4);
    dispBox->Add(m_settings_labels,     0, wxALL, 4);
    p3Sizer->Add(dispBox, 0, wxALL | wxEXPAND, 6);

    wxArrayString infoModes;
    infoModes.Add(wxT("Hover popup"));
    infoModes.Add(wxT("Double-click sticky window"));
    infoModes.Add(wxT("Both"));
    m_settings_info_mode = new wxRadioBox(p3, wxID_ANY, wxT("Station info"),
                                          wxDefaultPosition, wxDefaultSize,
                                          infoModes, 1, wxRA_SPECIFY_ROWS);
    p3Sizer->Add(m_settings_info_mode, 0, wxALL | wxEXPAND, 6);

    p3->SetSizer(p3Sizer);
    m_notebook->AddPage(p3, wxT("Settings"));

    // ── Tab 4: Info ───────────────────────────────────────────────────────────

    wxPanel *p4 = new wxPanel(m_notebook, wxID_ANY);
    wxBoxSizer *p4Sizer = new wxBoxSizer(wxVERTICAL);

    wxHtmlWindow *info_html = new wxHtmlWindow(
        p4, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHW_SCROLLBAR_AUTO);
    info_html->SetPage(wxString::FromUTF8(kInfoHTML));
    info_html->Bind(wxEVT_HTML_LINK_CLICKED, [](wxHtmlLinkEvent &evt) {
        wxLaunchDefaultBrowser(evt.GetLinkInfo().GetHref());
    });

    p4Sizer->Add(info_html, 1, wxALL | wxEXPAND, 4);

    p4->SetSizer(p4Sizer);
    m_notebook->AddPage(p4, wxT("Info"));

    // ── Common area ───────────────────────────────────────────────────────────

    topSizer->Add(m_notebook, 1, wxEXPAND);
    topSizer->Add(new wxStaticLine(this), 0, wxEXPAND);
    wxBoxSizer *closeSizer = new wxBoxSizer(wxHORIZONTAL);
    closeSizer->AddStretchSpacer();
    closeSizer->Add(new wxButton(this, ID_CLOSE_BTN, wxT("Close")), 0, wxALL, 6);
    topSizer->Add(closeSizer, 0, wxEXPAND);

    m_lat_min_ctrl->Bind(wxEVT_KILL_FOCUS, &ShipReportsPluginDialog::OnCoordBlur, this);
    m_lat_max_ctrl->Bind(wxEVT_KILL_FOCUS, &ShipReportsPluginDialog::OnCoordBlur, this);
    m_lon_min_ctrl->Bind(wxEVT_KILL_FOCUS, &ShipReportsPluginDialog::OnCoordBlur, this);
    m_lon_max_ctrl->Bind(wxEVT_KILL_FOCUS, &ShipReportsPluginDialog::OnCoordBlur, this);

    m_settings_url->Bind(wxEVT_KILL_FOCUS, &ShipReportsPluginDialog::OnSettingsUrlBlur, this);
    m_settings_wind_barbs->Bind(wxEVT_CHECKBOX,
        [this](wxCommandEvent&) { ApplySettings(); });
    m_settings_labels->Bind(wxEVT_CHECKBOX,
        [this](wxCommandEvent&) { ApplySettings(); });
    m_settings_info_mode->Bind(wxEVT_RADIOBOX,
        [this](wxCommandEvent&) { ApplySettings(); });

    PopulateAreaControls();
    ValidateCoords();
    PopulateSettingsControls();

    // Initial tab: history list if non-empty, fetch form otherwise
    m_notebook->SetSelection(m_plugin->GetFetchHistory().empty() ? 1 : 0);

    SetSizer(topSizer);
    topSizer->SetSizeHints(this);  // sets minimum size from sizer + fits window
    wxSize sz = GetSize();
    SetSize(wxSize(sz.GetWidth() * 14 / 10, sz.GetHeight()));
}

ShipReportsPluginDialog::~ShipReportsPluginDialog() {}

void ShipReportsPluginDialog::PopulateSettingsControls() {
    m_settings_url->SetValue(m_plugin->GetServerURL());
    m_settings_wind_barbs->SetValue(m_plugin->GetShowWindBarbs());
    m_settings_labels->SetValue(m_plugin->GetShowLabels());
    m_settings_info_mode->SetSelection(m_plugin->GetInfoMode());
}

void ShipReportsPluginDialog::ApplySettings() {
    m_plugin->SetServerURL(m_settings_url->GetValue());
    m_plugin->SetShowWindBarbs(m_settings_wind_barbs->GetValue());
    m_plugin->SetShowLabels(m_settings_labels->GetValue());
    m_plugin->SetInfoMode(m_settings_info_mode->GetSelection());
    m_plugin->SaveConfig();
    RequestRefresh(m_plugin->GetParentWindow());
}

void ShipReportsPluginDialog::OnSettingsUrlBlur(wxFocusEvent &event) {
    ApplySettings();
    event.Skip();
}

void ShipReportsPluginDialog::PopulateAreaControls() {
    auto fmt = [](double v) -> wxString {
        std::ostringstream ss;
        ss.imbue(std::locale::classic());
        ss << std::fixed << std::setprecision(2) << v;
        return wxString(ss.str());
    };
    m_lat_min_ctrl->ChangeValue(fmt(m_lat_min));
    m_lat_max_ctrl->ChangeValue(fmt(m_lat_max));
    m_lon_min_ctrl->ChangeValue(fmt(m_lon_min));
    m_lon_max_ctrl->ChangeValue(fmt(m_lon_max));
}

void ShipReportsPluginDialog::UpdateViewportBounds(const PlugIn_ViewPort &vp) {
    m_lat_min = vp.lat_min;
    m_lat_max = vp.lat_max;
    m_lon_min = vp.lon_min;
    m_lon_max = vp.lon_max;
    PopulateAreaControls();
    ValidateCoords();
}

void ShipReportsPluginDialog::RefreshHistory() {
    m_history_list->DeleteAllItems();
    const FetchHistory &hist = m_plugin->GetFetchHistory();
    for (size_t i = 0; i < hist.size(); i++) {
        const FetchRecord &r = hist[i];
        long idx = m_history_list->InsertItem((long)i, r.label);
        m_history_list->SetItem(idx, 1,
            wxString::Format(wxT("%.1f..%.1f, %.1f..%.1f"),
                r.lat_min, r.lat_max, r.lon_min, r.lon_max));
        m_history_list->SetItem(idx, 2,
            wxString::Format(wxT("%zu"), r.station_count));
    }
    if (!hist.empty()) {
        long last = (long)hist.size() - 1;
        m_history_list->SetItemState(last, wxLIST_STATE_SELECTED,
                                     wxLIST_STATE_SELECTED);
        m_history_list->EnsureVisible(last);
        m_export_gpx_btn->Enable(true);
        ObservationList stations;
        if (m_plugin->LoadStationsForEntry((size_t)last, stations))
            m_plugin->SetStations(stations);
        m_notebook->SetSelection(0);  // show Ship Reports tab
    } else {
        m_notebook->SetSelection(1);  // show Fetch new tab
    }
}

void ShipReportsPluginDialog::OnGetFromViewport(wxCommandEvent & /*event*/) {
    PlugIn_ViewPort vp = m_plugin->GetCurrentViewPort();
    m_lat_min = vp.lat_min;
    m_lat_max = vp.lat_max;
    m_lon_min = vp.lon_min;
    m_lon_max = vp.lon_max;
    PopulateAreaControls();
    ValidateCoords();
}

void ShipReportsPluginDialog::OnCoordBlur(wxFocusEvent &event) {
    event.Skip();
    ValidateCoords();
}

bool ShipReportsPluginDialog::ValidateCoords() {
    auto parse = [](wxTextCtrl *ctrl, double &val) -> bool {
        wxString s = ctrl->GetValue().Trim();
        s.Replace(wxT(","), wxT("."));
        return s.ToDouble(&val);
    };

    double lat_min, lat_max, lon_min, lon_max;

    auto setError = [&](const wxString &msg) {
        m_coord_error->SetLabel(msg);
        m_coord_error->SetForegroundColour(*wxRED);
        m_coord_error->GetParent()->Layout();
        m_fetch_btn->Enable(false);
    };

    if (!parse(m_lat_min_ctrl, lat_min) || !parse(m_lat_max_ctrl, lat_max) ||
        !parse(m_lon_min_ctrl, lon_min) || !parse(m_lon_max_ctrl, lon_max)) {
        setError(wxT("Please enter valid coordinates: Latitude \u221290 to 90, Longitude \u2212180 to 180"));
        return false;
    }
    if (lat_min < -90.0 || lat_min > 90.0 || lat_max < -90.0 || lat_max > 90.0 ||
        lon_min < -180.0 || lon_min > 180.0 || lon_max < -180.0 || lon_max > 180.0) {
        setError(wxT("Please enter valid coordinates: Latitude \u221290 to 90, Longitude \u2212180 to 180"));
        return false;
    }
    if (lat_min >= lat_max) {
        m_coord_error->SetLabel(wxT("Latitude min \u2265 Latitude max \u2014 area may be empty"));
        m_coord_error->SetForegroundColour(wxColour(180, 100, 0));
        m_coord_error->GetParent()->Layout();
        m_fetch_btn->Enable(true);
        return true;
    }

    m_coord_error->SetLabel(wxT("Latitude: \u221290 to 90  \u00b7  Longitude: \u2212180 to 180"));
    m_coord_error->SetForegroundColour(
        wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    m_coord_error->GetParent()->Layout();
    m_fetch_btn->Enable(true);
    return true;
}

void ShipReportsPluginDialog::AdjustColumns() {
    if (!m_history_list) return;
    int total = m_history_list->GetClientSize().GetWidth();
    int w1 = m_history_list->GetColumnWidth(1);  // Area – keep as-is
    int w2 = m_history_list->GetColumnWidth(2);  // Objects – keep as-is
    int w0 = total - w1 - w2;
    if (w0 > 60) m_history_list->SetColumnWidth(0, w0);
}

void ShipReportsPluginDialog::OnSize(wxSizeEvent &event) {
    event.Skip();  // Layout() runs after this handler returns
    CallAfter([this]() { AdjustColumns(); });
}

void ShipReportsPluginDialog::OnFetch(wxCommandEvent & /*event*/) {
    // Build types string
    wxString types;
    if (m_chk_ship->GetValue())    { if (!types.IsEmpty()) types += wxT(","); types += wxT("ship"); }
    if (m_chk_buoy->GetValue())    { if (!types.IsEmpty()) types += wxT(","); types += wxT("buoy"); }
    if (m_chk_shore->GetValue())   { if (!types.IsEmpty()) types += wxT(","); types += wxT("shore"); }
    if (m_chk_drifter->GetValue()) { if (!types.IsEmpty()) types += wxT(","); types += wxT("drifter"); }
    if (m_chk_other->GetValue())   { if (!types.IsEmpty()) types += wxT(","); types += wxT("other"); }

    if (types.IsEmpty()) {
        m_status_label->SetLabel(wxT("Select at least one platform type"));
        return;
    }

    // Coords are pre-validated (Fetch button is disabled when invalid)
    double lat_min, lat_max, lon_min, lon_max;
    auto parseCoord = [](wxTextCtrl *ctrl, double &val) -> bool {
        wxString s = ctrl->GetValue().Trim();
        s.Replace(wxT(","), wxT("."));
        return s.ToDouble(&val);
    };
    if (!parseCoord(m_lat_min_ctrl, lat_min) ||
        !parseCoord(m_lat_max_ctrl, lat_max) ||
        !parseCoord(m_lon_min_ctrl, lon_min) ||
        !parseCoord(m_lon_max_ctrl, lon_max)) {
        return;  // should not happen — button is disabled when invalid
    }

    wxString max_age = m_max_age->GetString(m_max_age->GetSelection());

    m_status_label->SetLabel(wxT("Fetching..."));
    Update();

    ObservationList stations;
    wxString error;
    bool ok = FetchObservations(
        m_plugin->GetServerURL(),
        lat_min, lat_max, lon_min, lon_max,
        max_age, types, stations, error);

    if (ok) {
        FetchRecord rec;
        rec.fetched_at    = wxDateTime::Now().ToUTC();
        rec.label         = rec.fetched_at.Format(wxT("%Y-%m-%d %H:%M"));
        rec.lat_min       = lat_min;
        rec.lat_max       = lat_max;
        rec.lon_min       = lon_min;
        rec.lon_max       = lon_max;
        rec.station_count = stations.size();

        m_plugin->AppendFetch(rec, stations);
        m_plugin->SetStations(stations);
        m_status_label->SetLabel(wxT("Ready"));
        RefreshHistory();  // also switches to Tab 1
    } else {
        m_status_label->SetLabel(wxString::Format(wxT("Error: %s"), error));
    }
}

void ShipReportsPluginDialog::OnClose(wxCommandEvent & /*event*/) {
    m_plugin->SetStations(ObservationList());
    Hide();
}

void ShipReportsPluginDialog::OnWindowClose(wxCloseEvent & /*event*/) {
    m_plugin->SetStations(ObservationList());
    Hide();
}

void ShipReportsPluginDialog::OnHistorySelected(wxListEvent &event) {
    long idx = event.GetIndex();
    const FetchHistory &hist = m_plugin->GetFetchHistory();
    if (idx >= 0 && idx < (long)hist.size()) {
        ObservationList stations;
        if (m_plugin->LoadStationsForEntry((size_t)idx, stations))
            m_plugin->SetStations(stations);
        m_export_gpx_btn->Enable(true);
        m_delete_entry_btn->Enable(true);
    }
}

void ShipReportsPluginDialog::OnDeleteEntry(wxCommandEvent & /*event*/) {
    long sel = m_history_list->GetNextItem(-1, wxLIST_NEXT_ALL,
                                           wxLIST_STATE_SELECTED);
    if (sel == -1) return;

    m_plugin->RemoveFetch((size_t)sel);
    m_plugin->SetStations(ObservationList());

    RefreshHistory();

    if (m_plugin->GetFetchHistory().empty()) {
        m_export_gpx_btn->Enable(false);
        m_delete_entry_btn->Enable(false);
    }
}

// Format a double observation value with a label, or return empty if NaN.
static wxString FmtObs(const wxString &label, double val, const wxString &unit) {
    if (std::isnan(val)) return wxT("");
    return wxString::Format(wxT("%s: %.1f %s\n"), label, val, unit);
}

// Write stations as GPX waypoints to a file.
static bool WriteGPXFile(const wxString &filepath, const wxDateTime &fetched_at,
                         const ObservationList &stations) {
    wxFile f;
    if (!f.Open(filepath, wxFile::write)) return false;

    wxString gpx;
    gpx += wxT("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    gpx += wxT("<gpx version=\"1.1\" creator=\"shipobs_pi\"\n");
    gpx += wxT("  xmlns=\"http://www.topografix.com/GPX/1/1\">\n");

    for (size_t i = 0; i < stations.size(); i++) {
        const ObservationStation &st = stations[i];
        if (std::isnan(st.lat) || std::isnan(st.lon)) continue;

        gpx += wxString::Format(
            wxT("  <wpt lat=\"%.6f\" lon=\"%.6f\">\n"), st.lat, st.lon);
        gpx += wxString::Format(wxT("    <name>%s</name>\n"), st.id);
        gpx += wxT("    <sym>Float</sym>\n");

        wxString desc;
        if (st.time.IsValid())
            desc += wxString::Format(wxT("Timestamp: %s UTC\n"),
                                     st.time.Format(wxT("%b %d, %Y %H:%M")));
        desc += wxString::Format(wxT("Station: %s (%s)\n"), st.id, st.type);
        if (!st.country.IsEmpty())
            desc += wxString::Format(wxT("Country: %s\n"), st.country);
        if (!std::isnan(st.wind_dir))
            desc += wxString::Format(wxT("Wind direction: %d\u00b0T\n"),
                                     (int)std::round(st.wind_dir));
        if (!std::isnan(st.wind_spd))
            desc += wxString::Format(wxT("Wind speed: %.1f kts\n"),
                                     st.wind_spd * 1.94384);
        if (!std::isnan(st.gust))
            desc += wxString::Format(wxT("Gust: %.1f kts\n"),
                                     st.gust * 1.94384);
        desc += FmtObs(wxT("Pressure"),      st.pressure, wxT("hPa"));
        if (!std::isnan(st.air_temp))
            desc += wxString::Format(wxT("Air temperature: %.1f \u00b0C\n"), st.air_temp);
        if (!std::isnan(st.sea_temp))
            desc += wxString::Format(wxT("Sea temperature: %.1f \u00b0C\n"), st.sea_temp);
        desc += FmtObs(wxT("Wave height"),   st.wave_ht,  wxT("m"));
        desc += FmtObs(wxT("Visibility"),    st.vis,      wxT("nm"));
        desc.Trim();
        if (fetched_at.IsValid())
            desc += wxString::Format(wxT("\n\nFetched: %s"),
                                     fetched_at.Format(wxT("%Y-%m-%dT%H:%M:%SZ")));

        gpx += wxString::Format(wxT("    <desc>%s</desc>\n"), desc);

        if (st.time.IsValid())
            gpx += wxString::Format(wxT("    <time>%s</time>\n"),
                                    st.time.Format(wxT("%Y-%m-%dT%H:%M:%SZ")));
        gpx += wxT("  </wpt>\n");
    }
    gpx += wxT("</gpx>\n");

    wxCharBuffer buf = gpx.ToUTF8();
    f.Write(buf.data(), buf.length());
    return true;
}

void ShipReportsPluginDialog::OnExportGPX(wxCommandEvent & /*event*/) {
    long sel = m_history_list->GetNextItem(-1, wxLIST_NEXT_ALL,
                                           wxLIST_STATE_SELECTED);
    if (sel == -1) return;

    const FetchHistory &hist = m_plugin->GetFetchHistory();
    if (sel >= (long)hist.size()) return;
    const FetchRecord &rec = hist[sel];

    ObservationList stations;
    if (!m_plugin->LoadStationsForEntry((size_t)sel, stations)) {
        wxMessageBox(wxT("Failed to load station data from disk"),
                     wxT("Error"), wxOK | wxICON_ERROR, this);
        return;
    }

    wxString default_name = rec.label;
    default_name.Replace(wxT("/"),  wxT("_"));
    default_name.Replace(wxT("\\"), wxT("_"));
    default_name.Replace(wxT(":"),  wxT("-"));

    wxFileDialog dlg(this, wxT("Export as GPX"), wxT(""),
                     default_name + wxT(".gpx"),
                     wxT("GPX files (*.gpx)|*.gpx"),
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;

    if (!WriteGPXFile(dlg.GetPath(), rec.fetched_at, stations)) {
        wxMessageBox(wxString::Format(wxT("Failed to write %s"), dlg.GetPath()),
                     wxT("Error"), wxOK | wxICON_ERROR, this);
    }
}
