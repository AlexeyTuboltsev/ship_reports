# Plan: Add `LoadGPXFileAsLayer` to OpenCPN Plugin API

## Goal

Expose a new plugin API function that loads a GPX file as a named layer immediately
at runtime (no restart required), the same way the "Create Persistent Layer" button
in the Route & Mark Manager does.

## Background

- The Route & Mark Manager → Layers → "Create Persistent Layer" button calls:
  `AddNewLayer(true)` → `UI_ImportGPX()` → `ImportFileArray(files, islayer=true, isPersistent=true, "")`
- `ImportFileArray` is declared **publicly** in `gui/include/gui/navutil.h`
- `gui/src/ocpn_plugin_gui.cpp` **already includes `navutil.h`** (line 61)
- So the new API function is a thin wrapper — no logic duplication

## Files to Modify

1. `include/ocpn_plugin.h` — add declaration
2. `gui/src/ocpn_plugin_gui.cpp` — add implementation

No other files need to change.

## Step 1 — Declaration in `include/ocpn_plugin.h`

Find the `AddSingleWaypoint` declaration (around line 3515) and add the new function
**after** it, following the same doc-comment style:

```cpp
/**
 * Loads a GPX file as a named layer, making it visible immediately.
 *
 * Equivalent to using Route & Mark Manager → Layers → "Create Persistent Layer"
 * and selecting the file. The layer appears on the chart immediately without
 * requiring an OpenCPN restart. The GPX file is copied to the persistent layers
 * directory so it reloads on next startup.
 *
 * @param gpx_path  Absolute path to an existing GPX file.
 * @param b_visible True (default) to show the layer on the chart immediately.
 * @return          Number of objects (waypoints + routes + tracks) loaded,
 *                  or -1 if the file could not be read.
 */
extern DECL_EXP int LoadGPXFileAsLayer(const wxString &gpx_path,
                                        bool b_visible = true);
```

Place it near the other waypoint/route/track API functions (after line ~3516).

## Step 2 — Implementation in `gui/src/ocpn_plugin_gui.cpp`

Add the following function at the **end of the file**, before the final
`EnableNotificationCanvasIcon` function (or after it — either is fine):

```cpp
int LoadGPXFileAsLayer(const wxString &gpx_path, bool b_visible) {
  if (!wxFileExists(gpx_path)) return -1;

  // Remember layer count before import so we can find the new one.
  size_t count_before = pLayerList ? pLayerList->size() : 0;

  // Temporarily override g_bShowLayers so ImportFileArray respects b_visible.
  bool saved_show = g_bShowLayers;
  g_bShowLayers = b_visible;

  wxArrayString files;
  files.Add(gpx_path);
  ImportFileArray(files, /*islayer=*/true, /*isPersistent=*/true,
                  /*dirpath=*/"");

  g_bShowLayers = saved_show;

  // ImportFileArray inserts the new layer at the front of pLayerList.
  if (!pLayerList || pLayerList->size() <= count_before) return -1;

  Layer *l = pLayerList->front();
  int n_items = (int)l->m_NoOfItems;

  // Refresh the Route Manager dialog layer list if it is open.
  if (pRouteManagerDialog && pRouteManagerDialog->IsShown())
    pRouteManagerDialog->UpdateLayListCtrl();

  // Redraw all chart canvases.
  if (gFrame) gFrame->RefreshAllCanvas();

  return n_items;
}
```

**Required headers** — all already present in `ocpn_plugin_gui.cpp`:
- `navutil.h` — declares `ImportFileArray`
- `ocpn_frame.h` — declares `gFrame` (`MyFrame*`) and `RefreshAllCanvas()`
- `routemanagerdialog.h` — declares `pRouteManagerDialog` and `UpdateLayListCtrl()`
- `gui/layer.h` — declares `Layer`, `pLayerList`

Verify these are present in the existing `#include` block at the top of the file.
If `gui/layer.h` is not already included, add:
```cpp
#include "layer.h"
```

## Step 3 — Build and Verify

```bash
cd /home/lexey/OpenCPN/build
make -j$(nproc) 2>&1 | grep -E "error:|warning:|Built"
```

Errors to watch for:
- `pLayerList` undeclared → add `#include "layer.h"`
- `g_bShowLayers` undeclared → add `#include "model/gui_vars.h"` (already present)
- `UpdateLayListCtrl` not found → check spelling in routemanagerdialog.h

## Step 4 — Update the shipobs plugin to use the new API

Once OpenCPN is rebuilt, update
`/home/lexey/ship_reports/plugin/src/request_dialog.cpp`, `OnSaveLayer`:

Replace the `AddSingleWaypoint` loop with:

```cpp
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

    wxTextEntryDialog dlg(this, wxT("Layer name:"),
                          wxT("Export to Layer"), rec.label);
    if (dlg.ShowModal() != wxID_OK) return;
    wxString name = dlg.GetValue().Trim();
    if (name.IsEmpty()) return;

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

    // Write GPX to a temp path, then let LoadGPXFileAsLayer copy it to layers/.
    wxString tmp_path = *datadir + wxFILE_SEP_PATH + safe + wxT(".gpx");

    if (!WriteGPXFile(tmp_path, name, rec.stations)) {
        m_status_label->SetLabel(wxString::Format(wxT("Failed to write %s"), tmp_path));
        return;
    }

    int loaded = LoadGPXFileAsLayer(tmp_path, /*b_visible=*/true);
    wxRemoveFile(tmp_path);   // ImportFileArray copied it to layers/ already

    if (loaded < 0) {
        m_status_label->SetLabel(wxT("Failed to create layer"));
        return;
    }

    m_status_label->SetLabel(wxString::Format(
        wxT("Layer \"%s\" created with %d objects"), name, loaded));
}
```

Also restore the `WriteGPXFile` helper (renamed from `WriteGPXLayer`) and its
`#include <wx/file.h>` / `#include <wx/filename.h>` in `request_dialog.cpp`.
The GPX writer is straightforward — just write `<wpt>` elements for each station.

Then rebuild and install the plugin:
```bash
cd /home/lexey/ship_reports/plugin/build
make -j$(nproc) && cp libshipobs_pi.so ~/.local/lib/opencpn/
```

## Summary of Changes

| File | Change |
|------|--------|
| `include/ocpn_plugin.h` | +8 lines: doc comment + declaration |
| `gui/src/ocpn_plugin_gui.cpp` | +20 lines: implementation |
| `plugin/src/request_dialog.cpp` | Replace AddSingleWaypoint loop with LoadGPXFileAsLayer call |

Total: ~30 lines of new code, zero duplication with existing layer logic.
