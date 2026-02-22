# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Tool Usage Rules — Always Follow

### Code Analysis
- ALWAYS use LSP tools for code analysis, diagnostics, type checking, and symbol resolution.
- Never guess at types, definitions, or errors when LSP tools are available. Use them first.

### Web Search
- ALWAYS use Firecrawl for any web search, URL fetching, or documentation lookup.
- Do not use generic Bash curl/wget for web content retrieval if Firecrawl is available.

### Git Operations
- ALWAYS use git-mcp for any git operations: commits, diffs, logs, branches, status, etc.
- Do not fall back to raw `git` Bash commands unless git-mcp explicitly fails.

## Environment & Installation Rules

### Never install directly on the host system
- If ANY task requires installing packages, runtimes, compilers, dependencies, or system tools,
  ALWAYS assume the work should happen inside a container (Docker or similar).
- Do NOT run `apt install`, `brew install`, `npm install -g`, `pip install` (system-wide),
  or any other system-level installation directly on the host machine.
- Instead: automatically propose a Dockerfile or docker-compose.yml that covers the requirement,
  and wait for approval before proceeding.
- This applies even if the install command looks harmless or temporary.
- When in doubt, ask "should this go in a container?" — the default answer is YES.

### Detect and respect existing container setup
- At the start of any task, check if a Dockerfile, docker-compose.yml, or .dockerignore
  exists in the repo root or any parent directory.
- If found AND the task involves running, building, installing, or testing anything:
  STOP and ask before proceeding.
- Do not assume the answer is yes automatically — always ask explicitly, every time.
- Only proceed after receiving a clear answer.
- If the answer is yes: all commands, builds, installs, and test runs must happen
  inside that container, not on the host.

These are standing instructions. Do not wait to be reminded. Apply them every session.

## Project Overview

**Ship & Buoy Observation System** — two components that work together:

1. **shipobs-server** (`/home/lexey/ship_reports/app/`) — Python/FastAPI server that aggregates ship/buoy observation data from OSMC ERDDAP and NDBC, deduplicates, and serves a compact JSON API. Designed for self-hosting on a shore server.

2. **shipobs_pi** (`/home/lexey/ship_reports/plugin/`) — C++ plugin for the OpenCPN chart plotter that fetches observations from the server and renders them as chart overlays. Standalone build, references OpenCPN plugin API headers from `/home/lexey/OpenCPN/include/ocpn_plugin.h`.
   **Naming:** the internal/technical name is `shipobs` (binary: `libshipobs_pi.so`, class: `shipobs_pi`). The human-readable display name shown to users is always **Ship Reports**.

## Architecture

```
┌──────────────────────┐          ┌──────────────────────────┐
│  OpenCPN Plugin      │  HTTP    │  shipobs-server           │
│  (bandwidth-         │ ───────► │  (self-hosted)            │
│   constrained boat)  │  JSON    │                          │
│                      │ ◄─────── │  Fetches OSMC, NDBC...   │
│  • Toolbar → dialog  │          │  Deduplicates & merges   │
│  • Render markers    │          │  Serves compact JSON     │
│  • Click/hover popup │          │  Spatial bbox queries    │
└──────────────────────┘          └──────────────────────────┘
```

Plugin sends: `GET /api/v1/observations?lat_min=..&lat_max=..&lon_min=..&lon_max=..&max_age=6h&types=ship,buoy`
Server responds: compact JSON with station array (~2-10 KB per viewport).

## Plans

Approved implementation plans are in `plans/`:
- `plans/shipobs-server-plan.md` — server architecture, data sources, API design, dedup strategy
- `plans/shipobs-plugin-plan.md` — plugin phases, rendering, popups, settings

## Server (Python/FastAPI)

### Structure
```
app/
├── main.py             # FastAPI app, endpoints, scheduled fetching
├── models.py           # ObservationStation dataclass
├── store.py            # In-memory station store with bbox query
├── fetchers/
│   ├── osmc.py         # OSMC ERDDAP CSV fetcher (every 15 min)
│   └── ndbc.py         # NDBC latest_obs + activestations (every 5 min)
├── dedup.py            # Merge logic, platform type normalization
└── config.py           # Settings (fetch intervals, max age, etc.)
```

### Commands
```bash
# Install
pip install -e .

# Run
uvicorn app.main:app --host 0.0.0.0 --port 8080

# Tests
pytest tests/ -v

# Docker
docker compose up
```

### Data Sources
- **OSMC ERDDAP** (`osmc.noaa.gov/erddap/tabledap/OSMC_flattened.csv`) — global GTS obs, ~4500 stations, 15-min refresh
- **NDBC latest_obs** (`ndbc.noaa.gov/data/latest_obs/latest_obs.txt`) — US buoys, ~800 stations, 5-min refresh
- **NDBC activestations.xml** — station metadata, loaded at startup

### Dedup: same `platform_code` → keep newest observation. Platform types normalized to: ship, buoy, drifter, shore, other.

## Plugin (C++ / OpenCPN)

### Location
`/home/lexey/ship_reports/plugin/`

### Build

**Always build via Docker** — do not build directly on the host:

```bash
cd /home/lexey/ship_reports/plugin
./build-docker.sh
```

This uses `shipobs-builder:latest` (extends `opencpn-builder:latest` which already has all deps: libwxgtk3.2-dev, cmake, libcurl4-openssl-dev, libGL-dev, etc.). The `.so` is written to `plugin/build/libshipobs_pi.so`.

To install after building:
```bash
cp /home/lexey/ship_reports/plugin/build/libshipobs_pi.so ~/.local/lib/opencpn/
```

The `Dockerfile` and `build-docker.sh` live in `plugin/`. The script mounts `plugin/` as `/plugin` and `~/OpenCPN/include` as `/ocpn-include` (read-only) inside the container.

### Structure
```
plugin/
├── CMakeLists.txt
├── src/
│   ├── shipobs_pi.h/.cpp     # Plugin class (opencpn_plugin_116)
│   ├── observation.h          # ObservationStation struct
│   ├── server_client.h/.cpp   # HTTP fetch + JSON parse
│   ├── request_dialog.h/.cpp  # Fetch request dialog
│   ├── render_overlay.h/.cpp  # GL/DC marker + wind barb rendering
│   ├── station_popup.h/.cpp   # Click/hover popup window
│   ├── settings_dialog.h/.cpp # Server URL, display prefs
│   └── wxJSON/                # JSON parser (from dashboard_pi)
└── include/wx/                # wxJSON headers
```

### Plugin API
- Inherits `opencpn_plugin_116` (see `include/ocpn_plugin.h`)
- Capabilities: `WANTS_OVERLAY_CALLBACK | WANTS_CURSOR_LATLON | WANTS_CONFIG | WANTS_MOUSE_EVENTS`
- Rendering: `RenderGLOverlayMultiCanvas()` draws markers + wind barbs
- Coordinate conversion: `GetCanvasPixLL()` for lat/lon → screen
- Config persistence: `GetOCPNConfigObject()`
- HTTP fetch: `OCPN_downloadFileBackground()`

### Pattern reference
Follow existing bundled plugins (especially `dashboard_pi`) for CMake structure, wxWidgets usage, and JSON parsing (wxJSON copied from `dashboard_pi/src/wxJSON/`).

### OpenCPN build prerequisites
The OpenCPN tree is already built at `/home/lexey/OpenCPN/build/`. `compile_commands.json` is at `build/compile_commands.json`.
