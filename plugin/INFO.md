# Ship Reports Plugin

Ship Reports is an [OpenCPN](https://opencpn.org) plugin that displays near real-time
meteorological and oceanographic observations from ships, buoys, and coastal
stations directly on the chart.

---

## What are ship reports

The observations displayed by this plugin come from a global network of
platforms that have been collecting and sharing surface marine weather data for
over a century, coordinated by the World Meteorological Organization (WMO) and
national meteorological agencies.

### Voluntary Observing Ships (VOS)

Merchant vessels, ferries, research ships, and yachts can enlist with their
national meteorological service to become Voluntary Observing Ships. Crew
members take regular observations — wind speed and direction, air and sea
temperature, pressure, visibility, wave height — and transmit them in
standardised WMO format over the Global Telecommunication System (GTS). The
programme is coordinated by the WMO Ship Observations Team and has been running
since the 1850s. Around 3 000–4 000 VOS ships are active at any one time,
providing invaluable coverage of otherwise data-sparse ocean regions.

### Moored buoys

Moored buoys are anchored to the seabed and carry automated sensor suites that
measure wind, waves, sea surface temperature, salinity, and atmospheric
pressure continuously. Networks are operated by national agencies — NOAA's
National Data Buoy Center (NDBC) covers US waters, while the international
Data Buoy Cooperation Panel (DBCP), a joint WMO/IOC body, coordinates global
deployment. Moored buoys are particularly important for storm and wave
monitoring and for validating satellite data.

### Drifting buoys

Drifting buoys are free-floating platforms that drift with ocean currents while
measuring sea surface temperature and atmospheric pressure. NOAA's Global
Drifter Program (part of the Global Ocean Observing System) maintains a target
array of around 1 250 drifters spread evenly across all ocean basins. Their
tracks also provide direct measurements of near-surface ocean currents.

### Coastal and shore stations

Fixed automated weather stations are installed on lighthouses, offshore oil
platforms, piers, and island outposts. NOAA's Coastal-Marine Automated Network
(C-MAN) and similar national networks provide high-frequency observations from
coastal chokepoints and port approaches that are critical for near-shore
navigation and safety.

---

## Data sources

We collect data from multiple sources trying to get the best coverage possible.
Please get in touch if you can suggest additional data sources.

### OSMC — Ocean Surface Marine Observations
**URL:** <https://osmc.noaa.gov>
**Data endpoint:** <https://osmc.noaa.gov/erddap/tabledap/OSMC_flattened.csv>

The OSMC (Observing System Monitoring Center) collects and quality-controls
real-time in-situ surface marine observations from the Global Telecommunication
System (GTS). It covers ships, moored and drifting buoys, coastal tide gauges,
and C-MAN stations worldwide. The server refreshes OSMC data every 15 minutes.

Note: GTS observations have an inherent reporting delay of roughly 1–2 hours
between measurement and availability. Displayed OSMC observations are therefore
typically 1–2 hours old regardless of when the last fetch occurred.

Typical coverage: ~4 000–5 000 active stations globally.

### NDBC — National Data Buoy Center
**URL:** <https://www.ndbc.noaa.gov>
**Data endpoint:** <https://www.ndbc.noaa.gov/data/latest_obs/latest_obs.txt>
**Station metadata:** <https://www.ndbc.noaa.gov/activestations.xml>

NDBC (operated by NOAA) maintains a network of moored buoys and coastal
stations along US coasts, the Great Lakes, and open ocean. Observations include
wind, wave height, sea temperature, and atmospheric pressure. The server
refreshes NDBC data every 5 minutes; NDBC publishes observations with only a
few minutes of delay, so displayed data is near-real-time.

Typical coverage: ~800–900 active US buoys and coastal stations.

---

## User manual

### Getting the data

This plugin requires a dedicated server that fetches and aggregates data from
the sources, deduplicates overlapping reports, and serves a compact JSON API
optimised for low-bandwidth connections. A publicly available server is planned
for the near future. In the meantime, you can use the provided Docker image to
self-host the server for testing purposes. See instructions below.

### Chart overlay

Each station is rendered as a colour-coded marker:

| Symbol   | Type            | Colour |
|----------|-----------------|--------|
| Triangle | Ship            | Blue   |
| Circle   | Buoy            | Yellow |
| Square   | Shore station   | Green  |
| Diamond  | Drifter         | Cyan   |

Marker opacity fades with observation age (fully opaque = fresh, 15 % at 24 h).

### Station info

Observations can be displayed as:
- **Hover popup** — move the mouse over a marker to see a summary of the latest
observation for that station. The popup follows the cursor and disappears when
the cursor moves away.

- **Sticky info window** — double-click a marker to open a floating window with
the full observation. The window follows the station as the chart pans and
zooms. Drag it to reposition. Multiple windows can be open at once. When a
window is focused or hovered, a yellow halo appears on the corresponding marker.

You can set the preferred mode in settings.

### Ship Reports tab

Lists all previous fetches. Select an entry to enable the **Export as GPX**
and **Delete** buttons.

- **Export as GPX** — saves the selected fetch's stations as GPX waypoints with
  full observation data in the description field.
- **Delete** — removes the entry from history.

### Fetch new tab

- **Max observation age** — only return stations that reported within this
  window (1 h – 24 h).
- **Platform types** — filter by station type (Ship, Buoy, Shore, Drifter,
  Other).
- **Area** — bounding box in decimal degrees. Use **Get from Viewport** to
  pre-fill with the current chart view.

### Settings tab

- **Server URL** — address of the shipobs-server instance. Applied on
  focus-out.
- **Show wind barbs** — draw wind barbs on the chart overlay. Defaults to ON.
- **Show station labels** — draw station ID labels next to each marker. Defaults to OFF.
- **Station info** — controls how station details are shown:
  - *Hover popup* — transient popup while the mouse is over a marker.
  - *Double-click sticky window* — pinned window that follows the station.
  - *Both* — enable both modes simultaneously.
  - Defaults to Both.

---

## Self-hosting the server
todo

---

## Authors

- Vibe-coder: Alexey Tubotlsev 
  [tblz@proton.me](mailto:tblz@proton.me)
  [github.com/AlexeyTubotlsev](https://github.com/AlexeyTubotlsev)

- The real coder: Claude Code
  [anthropic.com](https://anthropic.com)