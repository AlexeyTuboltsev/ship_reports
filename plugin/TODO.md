# shipobs_pi TODO

## Compatibility

- [x] Removed dependency on patched OpenCPN (`LoadGPXFileAsLayer` / `GetLayerNames`);
      plugin now works with stock OpenCPN. Incompatible-host dialog no longer needed.

## Server

- [ ] Add Swagger / OpenAPI documentation for the REST API
- [ ] Input validation tests (bad bbox params, missing fields, out-of-range values, etc.)
- [ ] Docker setup for self-hosting
- [ ] Add self-hosting howto to Info tab in UI

## Plugin

- [x] Coordinate input validation with inline errors and fetch button gating
- [x] Server URL configurable in Settings dialog
- [x] Replace Save to Layer with Export as GPX (no patched OpenCPN required)
- [x] GPX export description: proper field names, knots, °T, human-readable timestamp
- [x] Settings tab inline in the main dialog (server URL, display prefs, info mode)
- [x] Station info mode: hover popup / double-click sticky frame / both
- [x] Sticky station info frames that follow stations as the chart pans/zooms
- [x] Yellow marker halo when info frame is focused or hovered
- [x] Station labels rendered as GL textures (fix for GL double-buffer issue)
- [x] Add info tab with explanation what it is and what are our sources.
- [x] Add logging for fetches and errors to OpenCPN main log
- [x] rename RequestDialog to ShipReportsPluginDialog everywhere
- [ ] how are UI strings/translations handled in ocpn?
- [ ] UI / integration tests for the plugin (layer export flow)

## Questions for the OpenCPN team

- [ ] Sort out the name/GUID problem throughout the codebase: layers are
      currently identified by their user-editable display name (used as keys in
      `g_VisibleLayers` / `g_InvisibleLayers` and as the persistent filename).
      A stable GUID should be used instead so that renaming a layer or reusing
      a name across sessions doesn't corrupt visibility state.
