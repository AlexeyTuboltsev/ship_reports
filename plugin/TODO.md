# shipobs_pi TODO

## Compatibility

- [x] Removed dependency on patched OpenCPN (`LoadGPXFileAsLayer` / `GetLayerNames`);
      plugin now works with stock OpenCPN. Incompatible-host dialog no longer needed.

## Server

- [ ] Add Swagger / OpenAPI documentation for the REST API
- [ ] Input validation tests (bad bbox params, missing fields, out-of-range values, etc.)

## Plugin

- [x] Coordinate input validation with inline errors and fetch button gating
- [x] Server URL configurable in Settings dialog
- [x] Replace Save to Layer with Export as GPX (no patched OpenCPN required)
- [x] GPX export description: proper field names, knots, °T, human-readable timestamp
- [ ] UI / integration tests for the plugin (layer export flow)
- [ ] how are UI strings/translations handled in ocpn?
- [ ] Add logging for fetches and errors to OpenCPN main log
- [ ] Add info tab with explanation what it is and what are our sources. 

## Questions for the OpenCPN team

- [ ] Sort out the name/GUID problem throughout the codebase: layers are
      currently identified by their user-editable display name (used as keys in
      `g_VisibleLayers` / `g_InvisibleLayers` and as the persistent filename).
      A stable GUID should be used instead so that renaming a layer or reusing
      a name across sessions doesn't corrupt visibility state.
