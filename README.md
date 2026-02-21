# Ship & Buoy Observation System

Real-time ship and buoy weather observations on your chart plotter.

## Components

```
┌──────────────────────┐   HTTP/JSON   ┌──────────────────────────┐
│  shipobs_pi          │ ────────────► │  shipobs-server           │
│  OpenCPN plugin      │ ◄──────────── │  self-hosted              │
│                      │               │                           │
│  • Chart overlay     │               │  • Fetches OSMC + NDBC    │
│  • Click/hover popup │               │  • Deduplicates & merges  │
│  • GPX layer export  │               │  • Spatial bbox API       │
└──────────────────────┘               └──────────────────────────┘
```

## Server (`app/`)

Python/FastAPI service that aggregates global ship and buoy observations
from OSMC ERDDAP and NDBC, deduplicates them, and serves a compact JSON API.

**Data sources**
- [OSMC ERDDAP](https://osmc.noaa.gov/erddap/) — global GTS observations (~4 500 stations, refreshed every 15 min)
- [NDBC latest_obs](https://www.ndbc.noaa.gov/) — US buoys (~800 stations, refreshed every 5 min)

**API**
```
GET /api/v1/observations?lat_min=&lat_max=&lon_min=&lon_max=&max_age=6h&types=ship,buoy
```

### Setup

```bash
pip install -e ".[dev]"
uvicorn app.main:app --host 0.0.0.0 --port 8080
```

### Tests

```bash
pytest tests/ -v
```

### Docker

```bash
docker compose up
```

## Plugin (`plugin/`)

C++ plugin for OpenCPN that fetches observations from the server and renders
them as a chart overlay with wind barbs, click/hover popups, and GPX layer export.

**Requirements**
- OpenCPN with the `LoadGPXFileAsLayer` / `GetLayerNames` plugin API additions
  (see `plans/ocpn-add-LoadGPXFileAsLayer-api.md`)

### Build

```bash
cd plugin
mkdir -p build && cd build
cmake .. -DOPENCPN_INCLUDE_DIR=/path/to/OpenCPN/include
make -j$(nproc)
```

Or with Docker (recommended):

```bash
cd plugin
./build-docker.sh
```

### Install

```bash
cp build/libshipobs_pi.so ~/.local/lib/opencpn/
```

Add to `~/.opencpn/opencpn.conf` (while OpenCPN is closed):

```ini
[PlugIns/libshipobs_pi.so]
bEnabled=1
```

## Plans

Approved architecture and implementation plans are in [`plans/`](plans/).
