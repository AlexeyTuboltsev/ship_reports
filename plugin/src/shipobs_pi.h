#ifndef _SHIPOBS_PI_H_
#define _SHIPOBS_PI_H_

#include "ocpn_plugin.h"
#include "observation.h"

#define PLUGIN_VERSION_MAJOR 0
#define PLUGIN_VERSION_MINOR 1
#define MY_API_VERSION_MAJOR 1
#define MY_API_VERSION_MINOR 16

// Forward declarations
class RequestDialog;
class SettingsDialog;
class StationPopup;
class StationInfoFrame;

class shipobs_pi : public opencpn_plugin_116 {
public:
    shipobs_pi(void *ppimgr);
    ~shipobs_pi();

    // Required PlugIn methods
    int Init(void);
    bool DeInit(void);

    int GetAPIVersionMajor();
    int GetAPIVersionMinor();
    int GetPlugInVersionMajor();
    int GetPlugInVersionMinor();
    wxBitmap *GetPlugInBitmap();
    wxString GetCommonName();
    wxString GetShortDescription();
    wxString GetLongDescription();

    // Optional method overrides
    int GetToolbarToolCount(void);
    void OnToolbarToolCallback(int id);
    void ShowPreferencesDialog(wxWindow *parent);

    void SetCursorLatLon(double lat, double lon);
    bool MouseEventHook(wxMouseEvent &event);
    void OnParentActivate(wxActivateEvent &event);

    // Sticky station info frames (opened on double-click)
    void OpenOrFocusInfoFrame(const ObservationStation &st,
                              const wxPoint &station_screen);
    void RemoveInfoFrame(StationInfoFrame *frame);
    bool IsStationHighlighted(const wxString &id) const;

    bool RenderGLOverlayMultiCanvas(wxGLContext *pcontext,
                                    PlugIn_ViewPort *vp, int canvasIndex);
    bool RenderOverlayMultiCanvas(wxDC &dc, PlugIn_ViewPort *vp,
                                  int canvasIndex);

    // Data access
    ObservationList &GetStations() { return m_stations; }
    const ObservationList &GetStations() const { return m_stations; }
    void SetStations(const ObservationList &stations);

    // History — disk is the source of truth; these do read-modify-write
    void AppendFetch(const FetchRecord &rec, const ObservationList &stations);
    void RemoveFetch(size_t index);
    bool LoadStationsForEntry(size_t index, ObservationList &out);
    const FetchHistory &GetFetchHistory() const { return m_fetch_history; }

    // Settings accessors
    wxString GetServerURL() const { return m_server_url; }
    void SetServerURL(const wxString &url) { m_server_url = url; }
    bool GetShowWindBarbs() const { return m_show_wind_barbs; }
    void SetShowWindBarbs(bool b) { m_show_wind_barbs = b; }
    bool GetShowLabels() const { return m_show_labels; }
    void SetShowLabels(bool b) { m_show_labels = b; }
    // Info display mode: 0=hover popup, 1=double-click sticky frame, 2=both
    int  GetInfoMode() const { return m_info_mode; }
    void SetInfoMode(int m)  { m_info_mode = m; }
    int GetEraseHistoryAfter() const { return m_erase_history_after; }
    void SetEraseHistoryAfter(int n) { m_erase_history_after = n; }

    PlugIn_ViewPort GetCurrentViewPort() const { return m_vp; }
    wxWindow *GetParentWindow() const { return m_parent_window; }
    void SaveConfig();

private:
    void LoadConfig();
    void LoadHistory();        // reads metadata from disk into m_fetch_history

    wxWindow *m_parent_window;
    int m_toolbar_id;
    wxBitmap m_toolbar_bitmap;

    // Dialogs
    RequestDialog *m_request_dialog;
    SettingsDialog *m_settings_dialog;
    StationPopup *m_station_popup;
    std::vector<StationInfoFrame*> m_info_frames;

    // Data
    ObservationList m_stations;
    FetchHistory m_fetch_history;

    // Current state
    double m_cursor_lat;
    double m_cursor_lon;
    PlugIn_ViewPort m_vp;

    // Settings
    wxString m_server_url;
    bool m_show_wind_barbs;
    bool m_show_labels;
    int  m_info_mode;   // 0=hover popup, 1=double-click sticky frame, 2=both
    // 0 = never erase; N = drop oldest entries once count exceeds N
    int  m_erase_history_after;
    bool m_vp_valid;
};

#endif // _SHIPOBS_PI_H_
