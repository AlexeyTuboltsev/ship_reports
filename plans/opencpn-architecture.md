# OpenCPN Architecture Reference

## Directory Structure
- `gui/src/`, `gui/include/gui/` — UI, canvas, rendering, plugin manager GUI
- `model/src/`, `model/include/model/` — data model, plugin loader, comms
- `include/ocpn_plugin.h` — plugin API (~4000 lines, versions up to 1.21)
- `plugins/` — built-in plugins: grib_pi, dashboard_pi, wmm_pi, chartdldr_pi, demo

## Rendering Pipeline (chart file → pixels)

### Chart class hierarchy
- `ChartBase` (gui/include/gui/chartbase.h:126) — abstract base
  - `ChartBaseBSB` (gui/include/gui/chartimg.h) — raster (BSB/KAP)
    - `ChartPlugInWrapper` (gui/include/gui/chartimg.h:389) — adapter for plugin charts
  - `s57chart` (gui/include/gui/s57chart.h) — S57/ENC vector
  - `cm93compchart` — CM93 vector
  - MBTiles — tile-based raster

### Key subsystems
- `ChartDB` (gui/include/gui/chartdb.h:95) — global chart database singleton `ChartData`
- `ViewPort` (gui/include/gui/viewport.h:56) — geo↔pixel transforms, projection, zoom
- `Quilt` (gui/include/gui/quilt.h:82) — multi-chart blending, QuiltPatch per chart
- `ocpnDC` (gui/include/gui/ocpndc.h:60) — drawing abstraction (wxDC + OpenGL)
- `glTextureManager` (gui/include/gui/gl_texture_mgr.h) — GPU texture pool + caching

### GL render loop: glChartCanvas::Render() (gui/src/gl_chart_canvas.cpp:~3801)
1. GL setup + FBO pan cache
2. `RenderCharts()` (~3442) → `RenderQuiltViewGL()` (~3138) iterates quilt patches:
   - Raster: `RenderRasterChartRegionGL()` → texture manager → `GetChartBits()`
   - Vector (s57): `Chs57->RenderRegionViewOnGLNoText()`
   - Vector (plugin): `ChPI->RenderRegionViewOnGLNoText()`
3. Text pass: `RenderQuiltViewGLText()` (~3390)
4. Overlay ENC pass (~3274)
5. Navigation objects: routes/tracks/waypoints (~1515), ownship (~2180), compass/grid (~2638)
6. Plugin overlays: `g_pi_manager->RenderAllGLCanvasOverlayPlugIns()`
7. `SwapBuffers()`

## Plugin System

### Plugin API contract (include/ocpn_plugin.h)
- Plugins subclass `opencpn_plugin` (versioned to `opencpn_plugin_121`)
- `Init()` returns capability bitmask: `WANTS_OVERLAY_CALLBACK`, `INSTALLS_TOOLBAR_TOOL`, `WANTS_NMEA_SENTENCES`, `INSTALLS_PLUGIN_CHART`, etc.
- Key callbacks: `RenderOverlay()`, `RenderGLOverlay()`, `SetCursorLatLon()`, `SetNMEASentence()`, `OnToolbarToolCallback()`, `SetPluginMessage()`

### Plugin loading
- `PluginLoader` (model/include/model/plugin_loader.h:150) — discovery, dlopen, factory
- `PlugInManager` (gui/include/gui/pluginmanager.h:172) — GUI integration, render dispatch, toolbar/menu, message routing
- Communication: `plugin_comm.h` — `SendMessageToAllPlugins()`, `SendNMEASentenceToAllPlugIns()`, etc.

### Plugin chart rendering (e.g., o-charts)
- Plugin returns class names via `GetDynamicChartClassNameArray()`
- `ChartPlugInWrapper` bridges plugin chart objects into ChartBase hierarchy
- Raster plugins: provide raw pixels via `GetChartBits()`, OpenCPN handles texturing
- Vector plugins: draw directly into GL context via `RenderRegionViewOnGL()`
- Plugin chart base classes: `PlugInChartBase` (:507), `PlugInChartBaseGL` (:3778), `PlugInChartBaseExtended` (:3910)

## External Plugins of Interest
- **ocpn_draw_pi** — drawing plugin, installed at `~/.local/lib/opencpn/libocpn_draw_pi.so`, source: github.com/jongough/ocpn_draw_pi
- **o-charts** — commercial chart plugin, uses PlugInChartBaseExtended

## Build
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```
`compile_commands.json` at `build/compile_commands.json`

