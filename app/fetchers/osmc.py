from __future__ import annotations

import csv
import io
import logging
from datetime import datetime, timedelta, timezone

import httpx

from app.config import (
    HTTP_TIMEOUT_SECONDS,
    OSMC_BASE_URL,
    OSMC_FIELDS,
    OSMC_LOOKBACK_HOURS,
)
from app.models import ObservationStation

logger = logging.getLogger(__name__)

# OSMC platform_type -> normalized type
PLATFORM_TYPE_MAP: dict[str, str] = {
    # ship
    "VOLUNTEER OBSERVING SHIPS": "ship",
    "SHIPS": "ship",
    "SHIPS (GENERIC)": "ship",
    "SHIP FISHING VESSEL": "ship",
    "VOSCLIM": "ship",
    # buoy
    "MOORED BUOYS": "buoy",
    "WEATHER BUOYS": "buoy",
    "TROPICAL MOORED BUOYS": "buoy",
    "TSUNAMI WARNING STATIONS": "buoy",
    "MOORED BUOYS (GENERIC)": "buoy",
    "WEATHER BUOYS (GENERIC)": "buoy",
    # drifter
    "DRIFTING BUOYS": "drifter",
    "DRIFTING BUOYS (GENERIC)": "drifter",
    "ICE BUOYS": "drifter",
    "UNCREWED SURFACE VEHICLE": "drifter",
    "TAGGED ANIMAL": "drifter",
    # shore
    "C-MAN WEATHER STATIONS": "shore",
    "SHORE AND BOTTOM STATIONS": "shore",
    "TIDE GAUGE STATIONS": "shore",
    "GLOSS": "shore",
    # other
    "RESEARCH": "other",
    "PROFILING FLOATS AND GLIDERS": "other",
    "GLIDERS": "other",
    "UNKNOWN": "other",
    "WEATHER OBS": "other",
    "WEATHER AND OCEAN OBS": "other",
}


def normalize_platform_type(raw: str) -> str:
    """Map OSMC platform_type string to one of: ship, buoy, drifter, shore, other."""
    return PLATFORM_TYPE_MAP.get(raw.strip().upper(), "other")


def _build_url() -> str:
    """Build the OSMC ERDDAP CSV request URL with a time filter."""
    cutoff = datetime.now(timezone.utc) - timedelta(hours=OSMC_LOOKBACK_HOURS)
    time_filter = cutoff.strftime("%Y-%m-%dT%H:%M:%SZ")
    return f"{OSMC_BASE_URL}?{OSMC_FIELDS}&time>={time_filter}"


def _parse_float(val: str) -> float | None:
    """Parse a float, returning None for empty/NaN values."""
    if not val or val.strip() == "" or val.strip().upper() == "NAN":
        return None
    try:
        return float(val)
    except ValueError:
        return None


def _parse_time(val: str) -> datetime | None:
    """Parse an ISO-8601 timestamp from OSMC."""
    if not val or not val.strip():
        return None
    try:
        return datetime.fromisoformat(val.replace("Z", "+00:00"))
    except ValueError:
        return None


def parse_osmc_csv(text: str) -> list[ObservationStation]:
    """Parse OSMC ERDDAP CSV text into ObservationStation objects.

    The CSV has two header rows: column names and units. We skip the units row.
    """
    lines = text.strip().split("\n")
    if len(lines) < 3:
        logger.warning("OSMC CSV has fewer than 3 lines (header+units+data)")
        return []

    # Skip the units row (second line)
    filtered = lines[0:1] + lines[2:]
    reader = csv.DictReader(io.StringIO("\n".join(filtered)))

    stations: list[ObservationStation] = []
    for row in reader:
        time = _parse_time(row.get("time", ""))
        lat = _parse_float(row.get("latitude", ""))
        lon = _parse_float(row.get("longitude", ""))
        if time is None or lat is None or lon is None:
            continue

        platform_code = (row.get("platform_code") or "").strip()
        if not platform_code:
            continue

        # Synthetic key for unidentified ships
        if platform_code.upper() == "SHIP":
            platform_code = f"SHIP_{lat:.1f}_{lon:.1f}_{int(time.timestamp())}"

        raw_type = (row.get("platform_type") or "").strip()

        wind_dir = _parse_float(row.get("winddir", ""))
        # WMO FM 13: dd=00 means calm/variable, not "from north" (dd=36 → 360°).
        # OSMC ships report 0.0 when direction is unavailable.
        if wind_dir == 0.0:
            wind_dir = None

        station = ObservationStation(
            platform_code=platform_code,
            platform_type=normalize_platform_type(raw_type),
            lat=lat,
            lon=lon,
            time=time,
            country=(row.get("country") or "").strip() or None,
            sea_temp=_parse_float(row.get("sst", "")),
            air_temp=_parse_float(row.get("atmp", "")),
            pressure=_parse_float(row.get("slp", "")),
            wind_spd=_parse_float(row.get("windspd", "")),  # m/s, stored as-is
            wind_dir=wind_dir,
            wave_ht=_parse_float(row.get("wvht", "")),
            water_level=_parse_float(row.get("waterlevel", "")),
            clouds=_parse_float(row.get("clouds", "")),
            dewpoint=_parse_float(row.get("dewpoint", "")),
            source="osmc",
        )
        station.normalize()
        if not station.is_valid():
            logger.debug("Dropping invalid OSMC station %s", platform_code)
            continue
        stations.append(station)

    logger.info("Parsed %d stations from OSMC CSV", len(stations))
    return stations


async def fetch_osmc(client: httpx.AsyncClient) -> list[ObservationStation]:
    """Fetch and parse OSMC ERDDAP data."""
    url = _build_url()
    logger.info("Fetching OSMC: %s", url[:120])
    resp = await client.get(url, timeout=HTTP_TIMEOUT_SECONDS)
    resp.raise_for_status()
    return parse_osmc_csv(resp.text)
