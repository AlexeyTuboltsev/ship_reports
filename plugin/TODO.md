# shipobs_pi TODO

## Compatibility

- [ ] Check host OpenCPN version on `Init()` and show an incompatibility dialog
      if it is too old to provide `LoadGPXFileAsLayer` / `GetLayerNames`.
      The minimum required version is the one that merges our API PR.

## Server

- [ ] Add Swagger / OpenAPI documentation for the REST API
- [ ] Input validation tests (bad bbox params, missing fields, out-of-range values, etc.)

## OpenCPN API changes

- [ ] Unit / integration tests for `LoadGPXFileAsLayer` (visibility enforcement,
      duplicate-name behaviour, LM list refresh)
- [ ] Unit test for `GetLayerNames` (empty list, populated list, order)

## Plugin

- [x] Coordinate input validation with inline errors and fetch button gating
- [x] Server URL configurable in Settings dialog
- [ ] Duplicate layer name: improve UX (currently blocks reuse; consider rename flow)
- [ ] UI / integration tests for the plugin (layer export flow, incompatible-host dialog)
- [ ] how are UI strings/translations handled in ocpn?
- [ ] Add logging for fetches and errors to OpenCPN main log

## Questions for the OpenCPN team

- [ ] Sort out the name/GUID problem throughout the codebase: layers are
      currently identified by their user-editable display name (used as keys in
      `g_VisibleLayers` / `g_InvisibleLayers` and as the persistent filename).
      A stable GUID should be used instead so that renaming a layer or reusing
      a name across sessions doesn't corrupt visibility state.
