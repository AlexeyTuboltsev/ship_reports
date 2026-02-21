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

- [ ] UI / integration tests for the plugin (layer export flow, duplicate-name
      warning dialog, incompatible-host dialog)

## Questions for the OpenCPN team

- [ ] Sort out the name/GUID problem throughout the codebase: layers are
      currently identified by their user-editable display name (used as keys in
      `g_VisibleLayers` / `g_InvisibleLayers` and as the persistent filename).
      A stable GUID should be used instead so that renaming a layer or reusing
      a name across sessions doesn't corrupt visibility state.
