#include "shipobs_pi.h"
#include "request_dialog.h"
#include "render_overlay.h"
#include "station_popup.h"
#include "station_info_frame.h"
#include "settings_dialog.h"

#include <wx/app.h>
#include <wx/intl.h>
#include <wx/fileconf.h>
#include <wx/file.h>
#include <wx/log.h>
#include <wx/jsonreader.h>
#include <wx/jsonwriter.h>
#include <wx/jsonval.h>
#include <algorithm>
#include <cmath>


// Factory functions required by OpenCPN plugin loader
extern "C" DECL_EXP opencpn_plugin *create_pi(void *ppimgr) {
    return new shipobs_pi(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin *p) { delete p; }

// ---------- Construction / Destruction ----------

shipobs_pi::shipobs_pi(void *ppimgr)
    : opencpn_plugin_116(ppimgr),
      m_parent_window(nullptr),
      m_toolbar_id(-1),
      m_request_dialog(nullptr),
      m_settings_dialog(nullptr),
      m_station_popup(nullptr),
      m_cursor_lat(0), m_cursor_lon(0),
      m_server_url(wxT("http://localhost:8080")),
      m_show_wind_barbs(true),
      m_show_labels(false),
      m_info_mode(2),
      m_erase_history_after(0),
      m_vp_valid(false) {}

shipobs_pi::~shipobs_pi() {}

// ---------- Required PlugIn Methods ----------

int shipobs_pi::Init(void) {
    AddLocaleCatalog(_T("opencpn-shipobs_pi"));
    m_parent_window = GetOCPNCanvasWindow();

    // Hide the station popup when OpenCPN loses focus. On X11, wxPopupWindow
    // is override-redirect and stays on top of every app without this.
    // wxEVT_ACTIVATE_APP is more reliable than per-frame wxEVT_ACTIVATE on X11.
    wxTheApp->Bind(wxEVT_ACTIVATE_APP, &shipobs_pi::OnParentActivate, this);

    // Create a simple toolbar bitmap (32x32 blue circle)
    m_toolbar_bitmap = wxBitmap(32, 32);
    {
        wxMemoryDC dc(m_toolbar_bitmap);
        dc.SetBackground(*wxWHITE_BRUSH);
        dc.Clear();
        dc.SetBrush(*wxBLUE_BRUSH);
        dc.SetPen(*wxBLACK_PEN);
        dc.DrawCircle(16, 16, 12);
        dc.SelectObject(wxNullBitmap);
    }

    m_toolbar_id = InsertPlugInTool(
        _("Ship Obs"), &m_toolbar_bitmap, &m_toolbar_bitmap,
        wxITEM_NORMAL, _("Ship & Buoy Observations"),
        _("Display ship and buoy observation data on the chart"),
        nullptr, -1, 0, this);

    LoadConfig();
    LoadHistory();

    return WANTS_OVERLAY_CALLBACK | WANTS_OPENGL_OVERLAY_CALLBACK |
           WANTS_CURSOR_LATLON | WANTS_CONFIG | WANTS_MOUSE_EVENTS |
           INSTALLS_TOOLBAR_TOOL;
}

bool shipobs_pi::DeInit(void) {
    SaveConfig();  // history is always on disk already

    if (m_request_dialog) {
        m_request_dialog->Destroy();
        m_request_dialog = nullptr;
    }
    if (m_settings_dialog) {
        m_settings_dialog->Destroy();
        m_settings_dialog = nullptr;
    }
    if (m_station_popup) {
        m_station_popup->Destroy();
        m_station_popup = nullptr;
    }
    for (StationInfoFrame *f : m_info_frames)
        f->Destroy();
    m_info_frames.clear();

    wxTheApp->Unbind(wxEVT_ACTIVATE_APP, &shipobs_pi::OnParentActivate, this);

    RemovePlugInTool(m_toolbar_id);
    return true;
}

int shipobs_pi::GetAPIVersionMajor() { return MY_API_VERSION_MAJOR; }
int shipobs_pi::GetAPIVersionMinor() { return MY_API_VERSION_MINOR; }
int shipobs_pi::GetPlugInVersionMajor() { return PLUGIN_VERSION_MAJOR; }
int shipobs_pi::GetPlugInVersionMinor() { return PLUGIN_VERSION_MINOR; }

wxBitmap *shipobs_pi::GetPlugInBitmap() { return &m_toolbar_bitmap; }

wxString shipobs_pi::GetCommonName() { return wxT("Ship Reports"); }
wxString shipobs_pi::GetShortDescription() {
    return _("Ship & Buoy Observation Plugin for OpenCPN");
}
wxString shipobs_pi::GetLongDescription() {
    return _("Displays real-time ship and buoy meteorological/oceanographic "
             "observations on the chart. Data is fetched from a configurable "
             "observation server and rendered as color-coded markers with "
             "wind barbs and station labels.");
}

// ---------- Optional Method Overrides ----------

int shipobs_pi::GetToolbarToolCount(void) { return 1; }

void shipobs_pi::OnToolbarToolCallback(int id) {
    if (id != m_toolbar_id) return;

    if (!m_request_dialog) {
        m_request_dialog = new ShipReportsPluginDialog(m_parent_window, this);
    }
    bool will_show = !m_request_dialog->IsShown();
    if (m_vp_valid) {
        m_request_dialog->UpdateViewportBounds(m_vp);
    }
    m_request_dialog->Show(will_show);
    if (will_show) {
        m_request_dialog->RefreshHistory();
    }
}

void shipobs_pi::ShowPreferencesDialog(wxWindow *parent) {
    SettingsDialog dlg(parent, this);
    if (dlg.ShowModal() == wxID_OK) {
        SaveConfig();
        RequestRefresh(m_parent_window);
    }
}

void shipobs_pi::SetCursorLatLon(double lat, double lon) {
    m_cursor_lat = lat;
    m_cursor_lon = lon;
}

void shipobs_pi::OnParentActivate(wxActivateEvent &event) {
    if (!event.GetActive() && m_station_popup && m_station_popup->IsShown())
        m_station_popup->Hide();
    event.Skip();
}

bool shipobs_pi::MouseEventHook(wxMouseEvent &event) {
    return HandleStationPopup(this, event, m_cursor_lat, m_cursor_lon,
                              m_vp, m_station_popup, m_parent_window);
}

static void RepositionInfoFrames(std::vector<StationInfoFrame*> &frames,
                                  PlugIn_ViewPort *vp,
                                  wxWindow *parent) {
    if (frames.empty()) return;
    PlugIn_ViewPort vp_copy = *vp;
    for (StationInfoFrame *f : frames) {
        wxPoint st_px;
        GetCanvasPixLL(&vp_copy, &st_px, f->GetLat(), f->GetLon());
        f->Reposition(parent->ClientToScreen(st_px));
    }
}

bool shipobs_pi::RenderGLOverlayMultiCanvas(wxGLContext *pcontext,
                                            PlugIn_ViewPort *vp,
                                            int canvasIndex) {
    if (!vp) return false;
    m_vp = *vp;
    m_vp_valid = true;
    RenderStationsGL(this, vp);
    RepositionInfoFrames(m_info_frames, vp, m_parent_window);
    return true;
}

bool shipobs_pi::RenderOverlayMultiCanvas(wxDC &dc, PlugIn_ViewPort *vp,
                                          int canvasIndex) {
    if (!vp) return false;
    m_vp = *vp;
    m_vp_valid = true;
    RenderStationsDC(this, dc, vp);
    RepositionInfoFrames(m_info_frames, vp, m_parent_window);
    return true;
}

void shipobs_pi::OpenOrFocusInfoFrame(const ObservationStation &st,
                                      const wxPoint &station_screen) {
    for (StationInfoFrame *f : m_info_frames) {
        if (f->GetStationId() == st.id) {
            f->Raise();
            return;
        }
    }
    StationInfoFrame *frame =
        new StationInfoFrame(m_parent_window, this, st, station_screen);
    m_info_frames.push_back(frame);
}

bool shipobs_pi::IsStationHighlighted(const wxString &id) const {
    for (StationInfoFrame *f : m_info_frames)
        if (f->IsHighlighted() && f->GetStationId() == id)
            return true;
    return false;
}

void shipobs_pi::RemoveInfoFrame(StationInfoFrame *frame) {
    auto it = std::find(m_info_frames.begin(), m_info_frames.end(), frame);
    if (it != m_info_frames.end())
        m_info_frames.erase(it);
}

void shipobs_pi::SetStations(const ObservationList &stations) {
    m_stations = stations;
    InvalidateLabelCache();
    RequestRefresh(m_parent_window);
}


// ---------- Config ----------

void shipobs_pi::LoadConfig() {
    wxFileConfig *conf = GetOCPNConfigObject();
    if (!conf) return;

    conf->SetPath(wxT("/PlugIns/ShipObs"));
    conf->Read(wxT("ServerURL"), &m_server_url, wxT("http://localhost:8080"));
    conf->Read(wxT("ShowWindBarbs"), &m_show_wind_barbs, true);
    conf->Read(wxT("ShowLabels"), &m_show_labels, false);
    conf->Read(wxT("InfoMode"), &m_info_mode, 2);
    conf->Read(wxT("EraseHistoryAfter"), &m_erase_history_after, 0);
}

void shipobs_pi::SaveConfig() {
    wxFileConfig *conf = GetOCPNConfigObject();
    if (!conf) return;

    conf->SetPath(wxT("/PlugIns/ShipObs"));
    conf->Write(wxT("ServerURL"), m_server_url);
    conf->Write(wxT("ShowWindBarbs"), m_show_wind_barbs);
    conf->Write(wxT("ShowLabels"), m_show_labels);
    conf->Write(wxT("InfoMode"), m_info_mode);
    conf->Write(wxT("EraseHistoryAfter"), m_erase_history_after);
}

// ---------- History persistence ----------
// Disk is the source of truth. m_fetch_history holds metadata only (no station
// data). Stations are written on fetch and read back on demand.

// wxJSONWriter uses %.10g which strips trailing zeros: 200.0 → "200".
// wxJSONReader then stores "200" as wxJSONTYPE_INT, and AsDouble() on that
// reads the wrong union slot (m_valDouble instead of m_valLong) → ~0.
// This only affects OUR OWN history JSON (written by wxJSONWriter).
// Server responses use Python's json module which always writes 200.0 as
// "200.0", so AsDouble() works directly there.  See ParseObservations().
static double SafeDouble(const wxJSONValue &v) {
    if (v.IsDouble()) return v.AsDouble();
    if (v.IsInt())    return static_cast<double>(v.AsInt());
    if (v.IsUInt())   return static_cast<double>(v.AsUInt());
    if (v.IsLong())   return static_cast<double>(v.AsLong());
    if (v.IsULong())  return static_cast<double>(v.AsULong());
    return NAN;
}

static wxString HistoryFilePath() {
    wxString *pdir = GetpPrivateApplicationDataLocation();
    if (!pdir || pdir->IsEmpty()) return wxT("");
    return *pdir + wxFILE_SEP_PATH + wxT("shipobs_history.json");
}

// Serialise one station to a JSON object.
static wxJSONValue SerializeStation(const ObservationStation &st) {
    wxJSONValue s;
    s[wxT("id")]      = st.id;
    s[wxT("type")]    = st.type;
    s[wxT("country")] = st.country;
    s[wxT("lat")]     = st.lat;
    s[wxT("lon")]     = st.lon;
    if (st.time.IsValid())
        s[wxT("time")] = st.time.Format(wxT("%Y-%m-%dT%H:%M:%SZ"));
    if (!std::isnan(st.wind_dir)) s[wxT("wind_dir")] = st.wind_dir;
    if (!std::isnan(st.wind_spd)) s[wxT("wind_spd")] = st.wind_spd;
    if (!std::isnan(st.gust))     s[wxT("gust")]     = st.gust;
    if (!std::isnan(st.pressure)) s[wxT("pressure")] = st.pressure;
    if (!std::isnan(st.air_temp)) s[wxT("air_temp")] = st.air_temp;
    if (!std::isnan(st.sea_temp)) s[wxT("sea_temp")] = st.sea_temp;
    if (!std::isnan(st.wave_ht))  s[wxT("wave_ht")]  = st.wave_ht;
    if (!std::isnan(st.vis))      s[wxT("vis")]      = st.vis;
    return s;
}

// Deserialise one station from a JSON object.
static ObservationStation ParseStation(const wxJSONValue &s) {
    ObservationStation st;
    if (s.HasMember(wxT("id")))      st.id      = s.ItemAt(wxT("id")).AsString();
    if (s.HasMember(wxT("type")))    st.type    = s.ItemAt(wxT("type")).AsString();
    if (s.HasMember(wxT("country"))) st.country = s.ItemAt(wxT("country")).AsString();
    if (s.HasMember(wxT("lat")))     st.lat     = SafeDouble(s.ItemAt(wxT("lat")));
    if (s.HasMember(wxT("lon")))     st.lon     = SafeDouble(s.ItemAt(wxT("lon")));
    if (s.HasMember(wxT("time"))) {
        wxDateTime dt;
        dt.ParseISOCombined(s.ItemAt(wxT("time")).AsString());
        if (dt.IsValid()) st.time = dt;
    }
    if (s.HasMember(wxT("wind_dir"))) st.wind_dir = SafeDouble(s.ItemAt(wxT("wind_dir")));
    if (s.HasMember(wxT("wind_spd"))) st.wind_spd = SafeDouble(s.ItemAt(wxT("wind_spd")));
    if (s.HasMember(wxT("gust")))     st.gust     = SafeDouble(s.ItemAt(wxT("gust")));
    if (s.HasMember(wxT("pressure"))) st.pressure = SafeDouble(s.ItemAt(wxT("pressure")));
    if (s.HasMember(wxT("air_temp"))) st.air_temp = SafeDouble(s.ItemAt(wxT("air_temp")));
    if (s.HasMember(wxT("sea_temp"))) st.sea_temp = SafeDouble(s.ItemAt(wxT("sea_temp")));
    if (s.HasMember(wxT("wave_ht")))  st.wave_ht  = SafeDouble(s.ItemAt(wxT("wave_ht")));
    if (s.HasMember(wxT("vis")))      st.vis      = SafeDouble(s.ItemAt(wxT("vis")));
    return st;
}

// Read and parse the history JSON file.  If the file is missing, root is
// initialised to an empty but valid document (version=1, records=[]).
static bool ReadHistoryFile(wxJSONValue &root) {
    wxString path = HistoryFilePath();

    if (path.IsEmpty() || !wxFileExists(path)) {
        root = wxJSONValue(wxJSONTYPE_OBJECT);
        root[wxT("version")] = 1;
        root[wxT("records")] = wxJSONValue(wxJSONTYPE_ARRAY);
        return true;
    }

    wxFile f;
    if (!f.Open(path, wxFile::read)) return false;
    wxFileOffset len = f.Length();
    if (len <= 0) return false;

    wxMemoryBuffer buf;
    buf.SetBufSize(len);
    f.Read(buf.GetData(), len);
    buf.SetDataLen(len);

    wxString json = wxString::FromUTF8(
        static_cast<const char *>(buf.GetData()), buf.GetDataLen());

    wxJSONReader reader;
    if (reader.Parse(json, &root) > 0) return false;
    if (!root.HasMember(wxT("records")))
        root[wxT("records")] = wxJSONValue(wxJSONTYPE_ARRAY);
    return true;
}

// Serialise root and write it to the history file.
static bool WriteHistoryFile(const wxJSONValue &root) {
    wxString path = HistoryFilePath();
    if (path.IsEmpty()) return false;

    wxJSONWriter writer(wxJSONWRITER_NONE);
    wxString json;
    writer.Write(root, json);

    wxFile f;
    if (!f.Open(path, wxFile::write)) return false;
    wxCharBuffer buf = json.ToUTF8();
    f.Write(buf.data(), buf.length());
    return true;
}

// Populate m_fetch_history with metadata only (no station data in memory).
void shipobs_pi::LoadHistory() {
    wxJSONValue root;
    if (!ReadHistoryFile(root)) return;

    wxJSONValue records = root[wxT("records")];
    m_fetch_history.clear();

    for (int i = 0; i < records.Size(); i++) {
        wxJSONValue r = records[i];
        FetchRecord rec;
        if (r.HasMember(wxT("label")))
            rec.label = r[wxT("label")].AsString();
        if (r.HasMember(wxT("fetched_at"))) {
            wxDateTime dt;
            dt.ParseISOCombined(r[wxT("fetched_at")].AsString());
            if (dt.IsValid()) rec.fetched_at = dt;
        }
        if (r.HasMember(wxT("lat_min"))) rec.lat_min = SafeDouble(r[wxT("lat_min")]);
        if (r.HasMember(wxT("lat_max"))) rec.lat_max = SafeDouble(r[wxT("lat_max")]);
        if (r.HasMember(wxT("lon_min"))) rec.lon_min = SafeDouble(r[wxT("lon_min")]);
        if (r.HasMember(wxT("lon_max"))) rec.lon_max = SafeDouble(r[wxT("lon_max")]);
        // station_count from the array size — no station data loaded into RAM
        if (r.HasMember(wxT("stations")) && r[wxT("stations")].IsArray())
            rec.station_count = static_cast<size_t>(r[wxT("stations")].Size());
        m_fetch_history.push_back(rec);
    }
}

// Write a new fetch record (with its stations) to disk, then reload metadata.
void shipobs_pi::AppendFetch(const FetchRecord &rec,
                             const ObservationList &stations) {
    wxJSONValue root;
    ReadHistoryFile(root);  // creates empty root if file missing

    // Build record JSON
    wxJSONValue r;
    r[wxT("label")]      = rec.label;
    r[wxT("fetched_at")] = rec.fetched_at.IsValid()
        ? rec.fetched_at.Format(wxT("%Y-%m-%dT%H:%M:%SZ"))
        : wxString(wxT(""));
    r[wxT("lat_min")] = rec.lat_min;
    r[wxT("lat_max")] = rec.lat_max;
    r[wxT("lon_min")] = rec.lon_min;
    r[wxT("lon_max")] = rec.lon_max;

    wxJSONValue starray(wxJSONTYPE_ARRAY);
    for (size_t i = 0; i < stations.size(); i++)
        starray.Append(SerializeStation(stations[i]));
    r[wxT("stations")] = starray;

    // Append, then trim oldest if eraseHistoryAfter > 0
    wxJSONValue old_records = root[wxT("records")];
    wxJSONValue new_records(wxJSONTYPE_ARRAY);
    int start = 0;
    if (m_erase_history_after > 0) {
        // keep at most (m_erase_history_after - 1) existing entries so that
        // after appending the new one the total equals m_erase_history_after
        start = old_records.Size() - (m_erase_history_after - 1);
        if (start < 0) start = 0;
    }
    for (int i = start; i < old_records.Size(); i++)
        new_records.Append(old_records[i]);
    new_records.Append(r);
    root[wxT("records")] = new_records;

    if (!WriteHistoryFile(root))
        wxLogError("ShipObs: failed to write history file");
    else
        wxLogMessage("ShipObs: saved fetch record (%zu station(s))", stations.size());
    LoadHistory();
}

// Remove entry at index from disk, then reload metadata.
void shipobs_pi::RemoveFetch(size_t index) {
    wxJSONValue root;
    if (!ReadHistoryFile(root)) return;

    wxJSONValue old_records = root[wxT("records")];
    wxJSONValue new_records(wxJSONTYPE_ARRAY);
    for (int i = 0; i < old_records.Size(); i++) {
        if (i != static_cast<int>(index))
            new_records.Append(old_records[i]);
    }
    root[wxT("records")] = new_records;

    WriteHistoryFile(root);
    LoadHistory();
}

// Read stations for one history entry from disk.
bool shipobs_pi::LoadStationsForEntry(size_t index, ObservationList &out) {
    wxJSONValue root;
    if (!ReadHistoryFile(root)) return false;

    wxJSONValue records = root[wxT("records")];
    if (static_cast<int>(index) >= records.Size()) return false;

    wxJSONValue r = records[static_cast<int>(index)];
    if (!r.HasMember(wxT("stations")) || !r[wxT("stations")].IsArray())
        return false;

    wxJSONValue starray = r[wxT("stations")];
    out.clear();
    out.reserve(static_cast<size_t>(starray.Size()));
    for (int i = 0; i < starray.Size(); i++)
        out.push_back(ParseStation(starray[i]));
    return true;
}
