# Plan: Ship & Buoy Observation Server (shipobs-server)

## Context

Self-hosted server that aggregates ship/buoy observation data from multiple public sources, deduplicates, merges, and serves a lightweight JSON API. Designed to sit on a shore-based server (home, VPS, etc.) and be queried by the OpenCPN plugin over whatever link the boat has.

The server absorbs all data source complexity so that clients (the OpenCPN plugin, or any HTTP client) receive clean, compact, pre-processed data.

## Data Source Research Summary

### Sources to Ingest

| Source | Endpoint | What | Refresh | Size |
|--------|----------|------|---------|------|
| **OSMC ERDDAP** | `https://osmc.noaa.gov/erddap/tabledap/OSMC_flattened.csv` | Global GTS observations — ships, buoys, drifters, shore stations, tide gauges. 28 platform types, ~3400 VOS ships + ~2000 buoys per 6h window. | Every 15 min | ~500KB CSV |
| **NDBC latest_obs** | `https://www.ndbc.noaa.gov/data/latest_obs/latest_obs.txt` | US buoy/shore stations. ~800 stations. Fresher than OSMC (5-min updates vs hourly). | Every 5 min | ~100KB |
| **NDBC station metadata** | `https://www.ndbc.noaa.gov/activestations.xml` | Station names, owners, types for 1360 NDBC stations. | Once at startup (or daily) | ~200KB XML |

### Source Details

**OSMC ERDDAP** (primary, global):
- No authentication required
- CSV, JSON, GeoJSON output
- Query by time range, bbox, platform_type
- `platform_code` = WMO ID or ship call sign (dedup key)
- `platform_type` categories: `VOLUNTEER OBSERVING SHIPS`, `SHIPS (GENERIC)`, `MOORED BUOYS`, `WEATHER BUOYS`, `DRIFTING BUOYS`, `C-MAN WEATHER STATIONS`, `TIDE GAUGE STATIONS`, `TROPICAL MOORED BUOYS`, etc.
- Fields: platform_code, platform_type, country, lat, lon, time, sst, atmp, slp, windspd, winddir, wvht, waterlevel, clouds, dewpoint
- Some ships have `platform_code = "SHIP"` with no call sign — keep all, use lat+lon+time as synthetic key

**NDBC latest_obs.txt** (US enrichment):
- Fixed-width text, 2 header lines
- Fields: STN, LAT, LON, WDIR, WSPD, GST, WVHT, DPD, APD, MWD, PRES, PTDY, ATMP, WTMP, DEWP, VIS, TIDE
- Missing values: `MM`
- Station IDs overlap OSMC platform_codes (41008, 46002, SAQG1, etc.)

### Sources Evaluated but NOT Used

| Source | Why Not |
|--------|---------|
| CMEMS In-Situ TAC | Requires registration + Python `copernicusmarine` toolkit. Not suitable for simple HTTP fetch. Could add later as optional enrichment. |
| IEM/GTS | Ship/buoy endpoints returned empty or 404. Not a viable API for marine obs. |
| E-SURFMAR | EU VOS monitoring only — HTML statistics pages, no data API. |
| NDBC ship_obs.php | HTML page, no machine-readable format. OSMC has the same data in CSV. |

### Deduplication Strategy

Key: `platform_code` (WMO station ID or ship call sign)

1. OSMC is the **base layer** — provides global coverage
2. NDBC **enriches** US stations with fresher data (5-min vs hourly)
3. Merge rule: for matching `platform_code`, keep the **newest** observation
4. Ships with `platform_code = "SHIP"` (no call sign): generate synthetic key `SHIP_{lat}_{lon}_{time}` — no dedup possible, show all
5. Platform type normalization: map 28 OSMC types → 5 simple categories:
   - `ship` ← VOLUNTEER OBSERVING SHIPS, SHIPS, SHIPS (GENERIC), SHIP FISHING VESSEL, VOSCLIM
   - `buoy` ← MOORED BUOYS, WEATHER BUOYS, TROPICAL MOORED BUOYS, TSUNAMI WARNING STATIONS, *_GENERIC variants
   - `drifter` ← DRIFTING BUOYS, ICE BUOYS, UNCREWED SURFACE VEHICLE, TAGGED ANIMAL
   - `shore` ← C-MAN WEATHER STATIONS, SHORE AND BOTTOM STATIONS, TIDE GAUGE STATIONS, GLOSS
   - `other` ← RESEARCH, PROFILING FLOATS AND GLIDERS, GLIDERS, UNKNOWN, WEATHER OBS, WEATHER AND OCEAN OBS

## Architecture

```
┌─────────────────────────────────────────────────────┐
│  shipobs-server                                      │
│                                                      │
│  ┌───────────────┐    ┌──────────────────────┐       │
│  │  Fetchers     │    │  In-Memory Store     │       │
│  │               │    │                      │       │
│  │  OSMC  ──────►├───►│  StationMap          │       │
│  │  (15 min)     │    │  {platform_code →    │       │
│  │               │    │   ObservationStation} │       │
│  │  NDBC  ──────►├───►│                      │──┐    │
│  │  (5 min)      │    │  Dedup + merge       │  │    │
│  │               │    │  Age-out > 12h       │  │    │
│  │  NDBC meta ──►│    └──────────────────────┘  │    │
│  │  (startup)    │                              │    │
│  └───────────────┘    ┌──────────────────────┐  │    │
│                       │  HTTP API            │◄─┘    │
│                       │                      │       │
│                       │  GET /api/v1/obs     │       │
│                       │  ?bbox=...&age=...   │       │
│                       │                      │       │
│                       │  Returns compact JSON│       │
│                       └──────────────────────┘       │
└─────────────────────────────────────────────────────┘
```

## API Design

### `GET /api/v1/observations`

Query parameters:
| Param | Type | Default | Description |
|-------|------|---------|-------------|
| `lat_min` | float | -90 | Southern bbox boundary |
| `lat_max` | float | 90 | Northern bbox boundary |
| `lon_min` | float | -180 | Western bbox boundary |
| `lon_max` | float | 180 | Eastern bbox boundary |
| `max_age` | string | `6h` | Max observation age (e.g., `3h`, `6h`, `12h`, `24h`) |
| `types` | string | `all` | Comma-separated: `ship,buoy,shore,drifter,other` |

Response:
```json
{
  "generated": "2026-02-20T15:00:00Z",
  "count": 47,
  "stations": [
    {
      "id": "41008",
      "type": "buoy",
      "country": "US",
      "lat": 31.4,
      "lon": -80.9,
      "time": "2026-02-20T14:30:00Z",
      "wind_dir": 170,
      "wind_spd": 5.0,
      "gust": 7.0,
      "pressure": 1014.7,
      "air_temp": 14.8,
      "sea_temp": null,
      "wave_ht": null,
      "vis": null
    }
  ]
}
```

Nulls omitted from wire when possible (or use a compact binary format later). Typical response for a viewport: **2-10 KB**.

### `GET /api/v1/status`

Returns server health, data freshness, station counts:
```json
{
  "uptime": "2d 5h",
  "sources": {
    "osmc": {"last_fetch": "2026-02-20T14:55:00Z", "stations": 4200, "status": "ok"},
    "ndbc": {"last_fetch": "2026-02-20T14:58:00Z", "stations": 803, "status": "ok"}
  },
  "total_stations": 4650,
  "oldest_observation": "2026-02-20T03:00:00Z"
}
```

## Implementation Plan

### Tech Stack Decision

Python (FastAPI) or Go or Rust — your call. Python is fastest to prototype with, Go/Rust better for deployment. The server is simple enough that language doesn't matter much.

Below assumes **Python + FastAPI** as the likely choice given the existing Python ecosystem in the tides project:

### Step 1: Project scaffold

```
shipobs-server/
├── pyproject.toml          # or requirements.txt
├── app/
│   ├── __init__.py
│   ├── main.py             # FastAPI app, endpoints
│   ├── models.py           # ObservationStation dataclass
│   ├── store.py            # In-memory station store with bbox query
│   ├── fetchers/
│   │   ├── __init__.py
│   │   ├── osmc.py         # OSMC ERDDAP fetcher + CSV parser
│   │   └── ndbc.py         # NDBC latest_obs + activestations parser
│   ├── dedup.py            # Merge logic, type normalization
│   └── config.py           # Settings (fetch intervals, max age, etc.)
├── tests/
│   ├── test_osmc_parser.py
│   ├── test_ndbc_parser.py
│   ├── test_dedup.py
│   └── test_api.py
├── Dockerfile
└── docker-compose.yml
```

### Step 2: OSMC fetcher (`app/fetchers/osmc.py`)

- Build ERDDAP CSV URL:
  ```
  https://osmc.noaa.gov/erddap/tabledap/OSMC_flattened.csv?
    platform_code,platform_type,country,latitude,longitude,time,
    sst,atmp,slp,windspd,winddir,wvht,waterlevel,clouds,dewpoint
    &time>={now - 6h}
  ```
- Fetch with `httpx` or `aiohttp` (async)
- Parse CSV rows into `ObservationStation` objects
- Normalize platform_type → simple category (ship/buoy/shore/drifter/other)
- Handle OSMC quirks: NaN values, "SHIP" as platform_code

### Step 3: NDBC fetcher (`app/fetchers/ndbc.py`)

- Fetch `latest_obs.txt`, parse fixed-width columns
- Fetch `activestations.xml` at startup for station metadata (names, types)
- Map NDBC station types (buoy/fixed/dart/other/usv) to simple categories
- Parse `MM` as null

### Step 4: Store + dedup (`app/store.py`, `app/dedup.py`)

- `StationStore`: dict keyed by `platform_code`
  - `update_from_osmc(stations)` — bulk replace OSMC data
  - `update_from_ndbc(stations)` — merge, preferring newer observation per station
  - `query(bbox, max_age, types)` — return filtered list
  - `purge_old(max_age)` — remove observations older than threshold
- Dedup: same `platform_code` → keep newest. SHIP_* synthetic keys → keep all.

### Step 5: Scheduled fetching (`app/main.py`)

- Use `asyncio` background tasks or `apscheduler`:
  - OSMC: every 15 minutes
  - NDBC: every 5 minutes
  - Purge old: every 15 minutes
- Fetch failures: log warning, keep serving stale data, retry next cycle

### Step 6: API endpoint (`app/main.py`)

- `GET /api/v1/observations` — bbox + filters → JSON response
- `GET /api/v1/status` — health check
- CORS headers if needed for web clients
- Optional: gzip compression (FastAPI middleware)

### Step 7: Docker deployment

```dockerfile
FROM python:3.12-slim
COPY . /app
WORKDIR /app
RUN pip install -e .
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8080"]
```

```yaml
# docker-compose.yml
services:
  shipobs:
    build: .
    ports:
      - "8080:8080"
    restart: unless-stopped
```

### Step 8: Tests

- `test_osmc_parser.py` — parse sample OSMC CSV, verify field extraction, NaN handling
- `test_ndbc_parser.py` — parse sample NDBC fixed-width text, verify MM → null
- `test_dedup.py` — verify NDBC overwrites older OSMC data for same station, SHIP_* keys preserved
- `test_api.py` — FastAPI TestClient, verify bbox filtering, type filtering, age filtering

## Data Flow Example

1. Server starts → fetches NDBC `activestations.xml` (station names)
2. Every 15 min: fetch OSMC CSV for last 6h → parse → store ~4500 stations
3. Every 5 min: fetch NDBC `latest_obs.txt` → parse → merge ~800 US stations (preferring newer)
4. Every 15 min: purge stations older than 12h
5. Plugin sends: `GET /api/v1/observations?lat_min=30&lat_max=45&lon_min=-80&lon_max=-60&max_age=6h`
6. Server filters in-memory store → returns 47 stations as 5KB JSON
7. Plugin renders markers on chart

## Future Enhancements (v2+)

- **CMEMS integration**: Add optional fetcher for EU in-situ data (requires user to provide credentials)
- **MessagePack**: Even more compact binary encoding instead of JSON
- **Spatial indexing**: R-tree or geohash for faster bbox queries at scale
- **Historical data**: SQLite/Postgres backend for trend analysis
- **WebSocket**: Push updates to connected clients (for monitoring use case)
- **Multi-user**: API keys, rate limiting if exposing publicly
