#include "request_dialog.h"
#include "shipobs_pi.h"
#include "server_client.h"

#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/arrstr.h>
#include <wx/file.h>
#include <wx/msgdlg.h>
#include <wx/textdlg.h>
#include <cmath>

enum {
    ID_FETCH = 10001,
    ID_CLOSE_BTN,
    ID_HISTORY_LIST,
    ID_SAVE_LAYER,
    ID_DELETE_ENTRY
};

BEGIN_EVENT_TABLE(RequestDialog, wxDialog)
    EVT_BUTTON(ID_FETCH,     RequestDialog::OnFetch)
    EVT_BUTTON(ID_CLOSE_BTN, RequestDialog::OnClose)
    EVT_CLOSE(RequestDialog::OnWindowClose)
    EVT_LIST_ITEM_SELECTED(ID_HISTORY_LIST, RequestDialog::OnHistorySelected)
    EVT_BUTTON(ID_SAVE_LAYER,    RequestDialog::OnSaveLayer)
    EVT_BUTTON(ID_DELETE_ENTRY,  RequestDialog::OnDeleteEntry)
END_EVENT_TABLE()

RequestDialog::RequestDialog(wxWindow *parent, shipobs_pi *plugin)
    : wxDialog(parent, wxID_ANY, wxT("Fetch Observations"),
               wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_plugin(plugin),
      m_lat_min(-90), m_lat_max(90),
      m_lon_min(-180), m_lon_max(180) {

    wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);

    // Max age selection
    wxStaticBoxSizer *ageSizer =
        new wxStaticBoxSizer(wxVERTICAL, this, wxT("Max Age"));
    wxArrayString ages;
    ages.Add(wxT("1h"));
    ages.Add(wxT("3h"));
    ages.Add(wxT("6h"));
    ages.Add(wxT("12h"));
    ages.Add(wxT("24h"));
    m_max_age = new wxChoice(this, wxID_ANY, wxDefaultPosition,
                             wxDefaultSize, ages);
    m_max_age->SetSelection(2);  // default 6h
    ageSizer->Add(m_max_age, 0, wxALL | wxEXPAND, 4);
    topSizer->Add(ageSizer, 0, wxALL | wxEXPAND, 4);

    // Platform type checkboxes
    wxStaticBoxSizer *typeSizer =
        new wxStaticBoxSizer(wxVERTICAL, this, wxT("Platform Types"));
    m_chk_ship    = new wxCheckBox(this, wxID_ANY, wxT("Ships"));
    m_chk_buoy    = new wxCheckBox(this, wxID_ANY, wxT("Buoys"));
    m_chk_shore   = new wxCheckBox(this, wxID_ANY, wxT("Shore Stations"));
    m_chk_drifter = new wxCheckBox(this, wxID_ANY, wxT("Drifters"));
    m_chk_other   = new wxCheckBox(this, wxID_ANY, wxT("Other"));
    m_chk_ship->SetValue(true);
    m_chk_buoy->SetValue(true);
    typeSizer->Add(m_chk_ship,    0, wxALL, 2);
    typeSizer->Add(m_chk_buoy,    0, wxALL, 2);
    typeSizer->Add(m_chk_shore,   0, wxALL, 2);
    typeSizer->Add(m_chk_drifter, 0, wxALL, 2);
    typeSizer->Add(m_chk_other,   0, wxALL, 2);
    topSizer->Add(typeSizer, 0, wxALL | wxEXPAND, 4);

    // Area display / edit
    wxStaticBoxSizer *areaSizer =
        new wxStaticBoxSizer(wxVERTICAL, this, wxT("Area (lat_min,lat_max,lon_min,lon_max)"));
    m_area_ctrl = new wxTextCtrl(this, wxID_ANY,
                                 wxString::Format(wxT("%.2f,%.2f,%.2f,%.2f"),
                                     m_lat_min, m_lat_max, m_lon_min, m_lon_max),
                                 wxDefaultPosition, wxDefaultSize,
                                 wxTE_PROCESS_ENTER | wxHSCROLL);
    areaSizer->Add(m_area_ctrl, 0, wxALL | wxEXPAND, 4);
    topSizer->Add(areaSizer, 0, wxALL | wxEXPAND, 4);

    // Status + progress
    m_status_label = new wxStaticText(this, wxID_ANY, wxT("Ready"));
    topSizer->Add(m_status_label, 0, wxALL | wxEXPAND, 4);

    m_progress = new wxGauge(this, wxID_ANY, 100);
    m_progress->Hide();
    topSizer->Add(m_progress, 0, wxALL | wxEXPAND, 4);

    // Fetch / Close buttons
    wxBoxSizer *btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->Add(new wxButton(this, ID_FETCH,     wxT("Fetch")), 0, wxALL, 4);
    btnSizer->Add(new wxButton(this, ID_CLOSE_BTN, wxT("Close")), 0, wxALL, 4);
    topSizer->Add(btnSizer, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 4);

    // History section
    wxStaticBoxSizer *histSizer =
        new wxStaticBoxSizer(wxVERTICAL, this, wxT("History"));

    m_history_list = new wxListCtrl(this, ID_HISTORY_LIST,
                                    wxDefaultPosition, wxSize(-1, 120),
                                    wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_SUNKEN);
    m_history_list->InsertColumn(0, wxT("Time"),  wxLIST_FORMAT_LEFT, 130);
    m_history_list->InsertColumn(1, wxT("#"),     wxLIST_FORMAT_RIGHT, 45);
    m_history_list->InsertColumn(2, wxT("Area"),  wxLIST_FORMAT_LEFT, 160);
    histSizer->Add(m_history_list, 1, wxALL | wxEXPAND, 4);

    wxBoxSizer *histBtnSizer = new wxBoxSizer(wxHORIZONTAL);
    m_delete_entry_btn = new wxButton(this, ID_DELETE_ENTRY, wxT("Delete"));
    m_delete_entry_btn->Enable(false);
    histBtnSizer->Add(m_delete_entry_btn, 0, wxALL, 4);
    histBtnSizer->AddStretchSpacer();
    m_save_layer_btn = new wxButton(this, ID_SAVE_LAYER, wxT("Export to Layer..."));
    m_save_layer_btn->Enable(false);
    histBtnSizer->Add(m_save_layer_btn, 0, wxALL, 4);
    histSizer->Add(histBtnSizer, 0, wxEXPAND);

    topSizer->Add(histSizer, 1, wxALL | wxEXPAND, 4);

    SetSizer(topSizer);
    SetMinSize(wxSize(360, 400));
    Fit();
}

RequestDialog::~RequestDialog() {}

void RequestDialog::UpdateViewportBounds(const PlugIn_ViewPort &vp) {
    m_lat_min = vp.lat_min;
    m_lat_max = vp.lat_max;
    m_lon_min = vp.lon_min;
    m_lon_max = vp.lon_max;

    if (m_area_ctrl) {
        m_area_ctrl->SetValue(wxString::Format(
            wxT("%.2f,%.2f,%.2f,%.2f"),
            m_lat_min, m_lat_max, m_lon_min, m_lon_max));
    }
}

void RequestDialog::RefreshHistory() {
    m_history_list->DeleteAllItems();
    const FetchHistory &hist = m_plugin->GetFetchHistory();
    for (size_t i = 0; i < hist.size(); i++) {
        const FetchRecord &r = hist[i];
        long idx = m_history_list->InsertItem((long)i, r.label);
        m_history_list->SetItem(idx, 1,
            wxString::Format(wxT("%zu"), r.station_count));
        m_history_list->SetItem(idx, 2,
            wxString::Format(wxT("%.1f..%.1f, %.1f..%.1f"),
                r.lat_min, r.lat_max, r.lon_min, r.lon_max));
    }
    // Select the most recent entry and load its stations from disk
    if (!hist.empty()) {
        long last = (long)hist.size() - 1;
        m_history_list->SetItemState(last, wxLIST_STATE_SELECTED,
                                     wxLIST_STATE_SELECTED);
        m_history_list->EnsureVisible(last);
        m_save_layer_btn->Enable(true);
        ObservationList stations;
        if (m_plugin->LoadStationsForEntry((size_t)last, stations))
            m_plugin->SetStations(stations);
    }
}

void RequestDialog::OnFetch(wxCommandEvent & /*event*/) {
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

    // Parse area from text field (lat_min,lat_max,lon_min,lon_max)
    double lat_min, lat_max, lon_min, lon_max;
    wxString area = m_area_ctrl->GetValue();
    wxArrayString parts = wxSplit(area, ',');
    if (parts.Count() != 4 ||
        !parts[0].ToDouble(&lat_min) ||
        !parts[1].ToDouble(&lat_max) ||
        !parts[2].ToDouble(&lon_min) ||
        !parts[3].ToDouble(&lon_max)) {
        m_status_label->SetLabel(wxT("Invalid area format"));
        return;
    }

    wxString max_age = m_max_age->GetString(m_max_age->GetSelection());

    m_status_label->SetLabel(wxT("Fetching..."));
    m_progress->Show();
    m_progress->Pulse();
    Update();

    ObservationList stations;
    wxString error;
    bool ok = FetchObservations(
        m_plugin->GetServerURL(),
        lat_min, lat_max, lon_min, lon_max,
        max_age, types, stations, error);

    m_progress->Hide();

    if (ok) {
        FetchRecord rec;
        rec.fetched_at    = wxDateTime::Now().ToUTC();
        rec.label         = rec.fetched_at.Format(wxT("%Y-%m-%d %H:%M UTC"));
        rec.lat_min       = lat_min;
        rec.lat_max       = lat_max;
        rec.lon_min       = lon_min;
        rec.lon_max       = lon_max;
        rec.station_count = stations.size();

        m_plugin->AppendFetch(rec, stations);  // write to disk, reload metadata
        m_plugin->SetStations(stations);       // show fetched data immediately
        RefreshHistory();

        m_status_label->SetLabel(
            wxString::Format(wxT("Loaded %zu observations"), stations.size()));
    } else {
        m_status_label->SetLabel(wxString::Format(wxT("Error: %s"), error));
    }
}

void RequestDialog::OnClose(wxCommandEvent & /*event*/) {
    m_plugin->SetStations(ObservationList());
    Hide();
}

void RequestDialog::OnWindowClose(wxCloseEvent & /*event*/) {
    m_plugin->SetStations(ObservationList());
    Hide();
}

void RequestDialog::OnHistorySelected(wxListEvent &event) {
    long idx = event.GetIndex();
    const FetchHistory &hist = m_plugin->GetFetchHistory();
    if (idx >= 0 && idx < (long)hist.size()) {
        ObservationList stations;
        if (m_plugin->LoadStationsForEntry((size_t)idx, stations))
            m_plugin->SetStations(stations);
        m_save_layer_btn->Enable(true);
        m_delete_entry_btn->Enable(true);
    }
}

void RequestDialog::OnDeleteEntry(wxCommandEvent & /*event*/) {
    long sel = m_history_list->GetNextItem(-1, wxLIST_NEXT_ALL,
                                           wxLIST_STATE_SELECTED);
    if (sel == -1) return;

    m_plugin->RemoveFetch((size_t)sel);  // remove from disk, reload metadata
    m_plugin->SetStations(ObservationList());  // clear chart

    RefreshHistory();  // will reload last entry if any remain

    if (m_plugin->GetFetchHistory().empty()) {
        m_save_layer_btn->Enable(false);
        m_delete_entry_btn->Enable(false);
        m_status_label->SetLabel(wxT("Ready"));
    }
}

// Format a double observation value with a label, or return empty if NaN.
static wxString FmtObs(const wxString &label, double val, const wxString &unit) {
    if (std::isnan(val)) return wxT("");
    return wxString::Format(wxT("%s: %.1f %s\n"), label, val, unit);
}

// Write stations as GPX waypoints to a file.
// Each waypoint is named by the station ID and includes all observation data
// in its description. Layer name comes from the filename stem (set by caller).
static bool WriteGPXFile(const wxString &filepath, const wxString &fetch_label,
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

        // Build description with all available data
        wxString desc;
        if (st.time.IsValid())
            desc += wxString::Format(wxT("Obs time: %s UTC\n"),
                                     st.time.Format(wxT("%Y-%m-%d %H:%M")));
        desc += wxString::Format(wxT("Fetched: %s\n"), fetch_label);
        desc += wxString::Format(wxT("Station: %s (%s)\n"), st.id, st.type);
        if (!st.country.IsEmpty())
            desc += wxString::Format(wxT("Country: %s\n"), st.country);
        desc += FmtObs(wxT("Wind dir"),  st.wind_dir, wxT("deg"));
        desc += FmtObs(wxT("Wind spd"),  st.wind_spd, wxT("m/s"));
        desc += FmtObs(wxT("Gust"),      st.gust,     wxT("m/s"));
        desc += FmtObs(wxT("Pressure"),  st.pressure, wxT("hPa"));
        desc += FmtObs(wxT("Air temp"),  st.air_temp, wxT("°C"));
        desc += FmtObs(wxT("Sea temp"),  st.sea_temp, wxT("°C"));
        desc += FmtObs(wxT("Wave ht"),   st.wave_ht,  wxT("m"));
        desc += FmtObs(wxT("Visibility"),st.vis,      wxT("nm"));
        desc.Trim();

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

void RequestDialog::OnSaveLayer(wxCommandEvent & /*event*/) {
    long sel = m_history_list->GetNextItem(-1, wxLIST_NEXT_ALL,
                                           wxLIST_STATE_SELECTED);
    if (sel == -1) {
        m_status_label->SetLabel(wxT("Select a history entry first"));
        return;
    }

    const FetchHistory &hist = m_plugin->GetFetchHistory();
    if (sel >= (long)hist.size()) return;
    const FetchRecord &rec = hist[sel];

    // Load station data from disk for this entry
    ObservationList stations;
    if (!m_plugin->LoadStationsForEntry((size_t)sel, stations)) {
        m_status_label->SetLabel(wxT("Failed to load station data from disk"));
        return;
    }

    wxTextEntryDialog dlg(this, wxT("Layer name:"),
                          wxT("Export to Layer"), rec.label);
    if (dlg.ShowModal() != wxID_OK) return;
    wxString name = dlg.GetValue().Trim();
    if (name.IsEmpty()) return;

    // Warn if a layer with this name already exists (name collisions cause
    // unpredictable visibility behavior in OpenCPN's g_VisibleLayers system).
    wxArrayString existing = GetLayerNames();
    if (existing.Index(name) != wxNOT_FOUND) {
        int answer = wxMessageBox(
            wxString::Format(
                wxT("A layer named \"%s\" already exists.\n\n"
                    "Using the same name may cause unexpected visibility "
                    "behavior.\n\nContinue anyway?"),
                name),
            wxT("Duplicate Layer Name"),
            wxYES_NO | wxICON_WARNING, this);
        if (answer != wxYES) return;
    }

    // Build safe filename
    wxString safe = name;
    safe.Replace(wxT("/"),  wxT("_"));
    safe.Replace(wxT("\\"), wxT("_"));
    safe.Replace(wxT(":"),  wxT("-"));

    wxString *datadir = GetpPrivateApplicationDataLocation();
    if (!datadir || datadir->IsEmpty()) {
        m_status_label->SetLabel(wxT("Cannot locate OpenCPN data directory"));
        return;
    }

    // Write GPX to a staging path named after the layer so that OpenCPN
    // derives the correct layer name from the filename stem.
    // LoadGPXFileAsLayer copies it to layers/ and shows it immediately.
    wxString gpx_path = *datadir + wxFILE_SEP_PATH + safe + wxT(".gpx");
    if (!WriteGPXFile(gpx_path, rec.label, stations)) {
        m_status_label->SetLabel(
            wxString::Format(wxT("Failed to write %s"), gpx_path));
        return;
    }

    int loaded = LoadGPXFileAsLayer(gpx_path, /*b_visible=*/false);
    wxRemoveFile(gpx_path);

    if (loaded < 0) {
        m_status_label->SetLabel(wxT("Failed to create layer"));
        return;
    }

    m_status_label->SetLabel(wxString::Format(
        wxT("Layer \"%s\" created (%d objects)"),
        name, loaded));
}
