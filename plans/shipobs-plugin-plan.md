# Plan: Add FES2014 Tidal Currents as Global Base Layer

## Context

The GRIB2 export currently has currents only from NOAA OFS regional models and CMEMS (European). When the fallback chain reaches FES2022b (global), it produces **elevation-only** GRIB2 — no currents. FES2022 currents exist but are not publicly available. FES2014 tidal currents **are** freely available (34 constituents, 1/16° grid, ~7km) and use the same harmonic synthesis math as elevations. pyfes can compute them directly — just load u/v constituent files as separate "tide" models.

**Goal**: Add FES2014 tidal currents (u, v) to the FES GRIB2 output so that every location on Earth gets currents in the GRIB2 file, not just locations covered by NOAA/CMEMS.

## Prerequisite: Download FES2014 Current Data

User needs to download from AVISO FTP (requires registration — same account as FES2022b):
- `eastward_velocity.tar.xz` → extract to `fes2014/eastward_velocity/` (34 files: `2n2.nc`, `m2.nc`, etc.)
- `northward_velocity.tar.xz` → extract to `fes2014/northward_velocity/` (34 files)

Files use variable names `Ua`/`Ug` (eastward amplitude/phase) and `Va`/`Vg` (northward). 1/16° grid, lon 0-360, NOT extrapolated to coasts (more coastal NaN than FES2022b elevations).

## Implementation Steps

### Step 1: Create YAML configs for FES2014 currents

Create two files following the pyfes convention (must use `tide:` key since pyfes.load_config only accepts `tide`/`radial`):

**`py/fes_eastward.yml`** — eastward velocity config:
```yaml
tide:
  cartesian:
    amplitude: Ua
    phase: Ug
    latitude: lat
    longitude: lon
    paths:
      2N2: ${DATASET_DIR}/2n2.nc
      Eps2: ${DATASET_DIR}/eps2.nc
      ... (34 constituents)
```

**`py/fes_northward.yml`** — northward velocity config:
```yaml
tide:
  cartesian:
    amplitude: Va
    phase: Vg
    ... (same 34 constituents from northward_velocity/)
```

Environment variable `DATASET_DIR` points to the respective data directory.

### Step 2: Add current handler loading (`webapp/app.py`)

Add config paths and cached handler loading, mirroring the existing `get_handlers_for_region()` pattern at line 908:

```python
CURRENT_EASTWARD_CONFIG = str(BASE_DIR / 'py' / 'fes_eastward.yml')
CURRENT_NORTHWARD_CONFIG = str(BASE_DIR / 'py' / 'fes_northward.yml')

# Check if FES2014 current data is available at startup
FES2014_CURRENTS_AVAILABLE = (
    (BASE_DIR / 'fes2014' / 'eastward_velocity' / 'm2.nc').exists() and
    (BASE_DIR / 'fes2014' / 'northward_velocity' / 'm2.nc').exists()
)

@lru_cache(maxsize=8)
def get_current_handlers_for_region(lon_min, lat_min, lon_max, lat_max):
    """Load FES2014 current handlers (u and v) for a bbox."""
    bbox = (normalized_lon_min, lat_min, normalized_lon_max, lat_max)
    u_handlers = pyfes.load_config(CURRENT_EASTWARD_CONFIG, bbox=bbox)
    v_handlers = pyfes.load_config(CURRENT_NORTHWARD_CONFIG, bbox=bbox)
    return u_handlers['tide'], v_handlers['tide']
```

### Step 3: Modify `predict_tides_grid()` to include currents

Add optional current prediction to the existing function (line 1131). When `FES2014_CURRENTS_AVAILABLE`, also:
1. Load current handlers for the region
2. Evaluate `pyfes.evaluate_tide()` on each (u_model, v_model) — returns cm/s
3. Convert to m/s (÷ 100)
4. Add `u_current` and `v_current` variables to the returned xarray Dataset
5. Apply GEBCO land mask to currents (same as elevations)
6. Do NOT apply datum offset to currents (only affects water level)

If FES2014 data is not present, the function works exactly as before (elevation-only).

### Step 4: Modify `write_grib2()` to include currents

Extend the function (line 1053) to check for `u_current`/`v_current` in the Dataset and write additional GRIB2 messages per timestep:

- Sea level: discipline=10, category=3, number=1 (existing)
- u-current: discipline=10, category=1, number=2 (same as cmems_to_grib2)
- v-current: discipline=10, category=1, number=3 (same as cmems_to_grib2)

This reuses the exact same GRIB2 parameter codes already used in `cmems_to_grib2()` (line 610).

### Step 5: Update `.gitignore`

Add `fes2014/` directory (same pattern as existing `fes2022b/` entry).

### Step 6: Update tests

- **`conftest.py`**: Mock `FES2014_CURRENTS_AVAILABLE` flag. Add fixture for mocking current handler loading.
- **`test_grib2_endpoint.py`**: Add test verifying FES GRIB2 output includes u/v current messages when FES2014 data is available, and elevation-only when not.
- Existing tests should pass unchanged since FES2014 currents are optional (graceful fallback).

## Files to Modify

| File | Changes |
|------|---------|
| `py/fes_eastward.yml` | **New** — pyfes config for FES2014 eastward velocity |
| `py/fes_northward.yml` | **New** — pyfes config for FES2014 northward velocity |
| `webapp/app.py` | Add `FES2014_CURRENTS_AVAILABLE`, `get_current_handlers_for_region()`. Modify `predict_tides_grid()` to compute u/v. Modify `write_grib2()` to write u/v GRIB2 messages. |
| `.gitignore` | Add `fes2014/` |
| `webapp/tests/conftest.py` | Mock FES2014 current availability |
| `webapp/tests/test_grib2_endpoint.py` | Test current inclusion in FES GRIB2 |
| `webapp/tests/verification_stations.gpx` | **New** — 15 verification stations as GPX waypoints |
| `webapp/tests/validate_stations.py` | **New** — Validation script: data source, predictions, GRIB2 content checks for all 15 stations |

## Key Patterns to Reuse

- `get_handlers_for_region()` (app.py:908) — caching pattern for pyfes handlers
- `cmems_to_grib2()` (app.py:610) — GRIB2 parameter codes for u/v currents (disc=10, cat=1, num=2/3)
- `predict_tides_grid()` (app.py:1131) — grid generation, pyfes evaluation loop, land masking

## Verification

### Step 7: Create verification station GPX file

Create `webapp/tests/verification_stations.gpx` with 15 stations covering all edge cases. Each station is a waypoint with name, coordinates, and description noting which test category it belongs to.

### Step 8: Create validation script

Create `webapp/tests/validate_stations.py` — a standalone script (not pytest) that:
1. Starts the app (or uses a running instance)
2. For each station: calls `/data-source`, `/predict`, and `/export/grib2`
3. Verifies correct data source detection
4. Checks GRIB2 contains expected parameters (elevation, u-current, v-current where applicable)
5. Reports results as a table

### Verification Stations (15 total)

#### Category 1: Coordinate Edge Cases

| # | Station | Lat | Lon | Tests | Expected Source |
|---|---------|-----|-----|-------|-----------------|
| 1 | **Dover, England** | 51.1144 | 1.3226 | 0° lon crossing, CMEMS NWS | CMEMS NWS |
| 2 | **Suva, Fiji** | -18.1333 | 178.4419 | 180° lon crossing, S hemisphere | FES (global fallback) |
| 3 | **Singapore** | 1.2615 | 103.8518 | Equator crossing | FES (global fallback) |

#### Category 2: Inside NOAA OFS Overlays (Tier 1 priority)

| # | Station | Lat | Lon | NOAA ID | OFS Model | Expected Source |
|---|---------|-----|-----|---------|-----------|-----------------|
| 4 | **San Francisco** | 37.8067 | -122.4660 | 9414290 | SFBOFS | SFBOFS (has currents) |
| 5 | **Baltimore** | 39.2667 | -76.5783 | 8574680 | CBOFS | CBOFS (has currents) |
| 6 | **The Battery, NYC** | 40.7000 | -74.0150 | 8518750 | NYOFS | NYOFS (has currents) |
| 7 | **Portland, ME** | 43.6583 | -70.2433 | 8418150 | GOMOFS | GOMOFS (has currents) |

#### Category 3: STOFS-3D Only (no regional OFS)

| # | Station | Lat | Lon | NOAA ID | Expected Source |
|---|---------|-----|-----|---------|-----------------|
| 8 | **Fort Pulaski (Savannah)** | 32.0333 | -80.9017 | 8670870 | STOFS-3D East (no currents) |

#### Category 4: CMEMS Overlays

| # | Station | Lat | Lon | Tests | Expected Source |
|---|---------|-----|-----|-------|-----------------|
| 9 | **Brest, France** | 48.3829 | -4.4948 | In NWS+IBI overlap, NWS preferred | CMEMS NWS (has currents) |
| 10 | **Cadiz, Spain** | 36.5278 | -6.3114 | Below NWS, inside IBI | CMEMS IBI (has currents) |

#### Category 5: Overlay Boundary Crossings

| # | Station | Lat | Lon | Tests | Expected Source |
|---|---------|-----|-----|-------|-----------------|
| 11 | **Monterey, CA** | 36.6050 | -121.8883 | Just south of SFBOFS (37.4), inside STOFS-3D | STOFS-3D East |
| 12 | **Reykjavik, Iceland** | 64.1500 | -21.9333 | North of STOFS-3D (52.8) and CMEMS NWS (62.75) | FES (global fallback) |
| 13 | **Dakar, Senegal** | 14.6767 | -17.4200 | South of STOFS-3D (20.2) and CMEMS IBI (26.17) | FES (global fallback) |

#### Category 6: Mid-Ocean (pure FES, no coastal effects)

| # | Station | Lat | Lon | Tests | Expected Source |
|---|---------|-----|-----|-------|-----------------|
| 14 | **Mid-Atlantic** | 45.0000 | -30.0000 | Open ocean, no overlays, no land mask | FES + FES2014 currents |
| 15 | **South Pacific** | -30.0000 | -150.0000 | Open ocean, S hemisphere | FES + FES2014 currents |

### Validation Checks Per Station

For each station, the validation script verifies:

1. **Data source detection** (`/data-source`): Returns expected source name
2. **Prediction** (`/predict`): Returns 24 hourly values, no unexpected NaN
3. **GRIB2 content** (`/export/grib2`):
   - Sea level messages present (discipline=10, cat=3, num=1)
   - u-current messages present where expected (discipline=10, cat=1, num=2)
   - v-current messages present where expected (discipline=10, cat=1, num=3)
   - For FES-fallback stations: currents present only if FES2014 data is available
4. **Boundary correctness**: Stations just outside an overlay do NOT get that overlay's source
5. **No interpolation artifacts**: Values at overlay boundaries are reasonable (no sudden jumps to 0 or extreme values)

### Expected Currents by Source

| Source | Has Currents in GRIB2? |
|--------|----------------------|
| NOAA OFS (SFBOFS, CBOFS, etc.) | Yes (from THREDDS regulargrid, when server is up) |
| NOAA STOFS-3D | No (currents not on regular grid) |
| CMEMS NWS/IBI | Yes (uo, vo) |
| FES2022b + FES2014 | Yes (after this implementation) |
| FES2022b without FES2014 | No (elevation only) |

### Running Verification

```bash
# Unit tests (must pass)
conda run -n tides python -m pytest webapp/tests/ -m "not e2e and not visual" -v

# Validation script (requires running app + live data access)
conda run -n tides python webapp/tests/validate_stations.py

# No-data fallback test
mv fes2014 fes2014.bak && conda run -n tides python webapp/tests/validate_stations.py && mv fes2014.bak fes2014
```
