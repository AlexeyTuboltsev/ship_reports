from __future__ import annotations

import logging
import xml.etree.ElementTree as ET
from datetime import datetime, timezone

import httpx

from app.config import (
    HTTP_TIMEOUT_SECONDS,
    NDBC_ACTIVE_STATIONS_URL,
    NDBC_LATEST_OBS_URL,
)
from app.models import ObservationStation

logger = logging.getLogger(__name__)

# NDBC station type -> normalized type
NDBC_TYPE_MAP: dict[str, str] = {
    "buoy": "buoy",
    "fixed": "shore",
    "dart": "buoy",
    "other": "other",
    "usv": "drifter",
    "oilrig": "shore",
    "tao": "buoy",
}


class NDBCMetadata:
    """Station metadata loaded from activestations.xml."""

    def __init__(self) -> None:
        self.stations: dict[str, dict] = {}  # station_id -> {name, type, owner}

    def load_from_xml(self, xml_text: str) -> None:
        """Parse activestations.xml and populate station metadata."""
        root = ET.fromstring(xml_text)
        count = 0
        for station_el in root.iter("station"):
            sid = station_el.get("id", "").strip()
            if not sid:
                continue
            stype = (station_el.get("type") or "other").strip().lower()
            self.stations[sid] = {
                "name": station_el.get("name", ""),
                "type": NDBC_TYPE_MAP.get(stype, "other"),
                "owner": station_el.get("owner", ""),
            }
            count += 1
        logger.info("Loaded NDBC metadata for %d stations", count)

    def get_type(self, station_id: str) -> str:
        meta = self.stations.get(station_id)
        if not meta:
            return "buoy"  # default to buoy for NDBC
        raw_type = meta["type"]
        return NDBC_TYPE_MAP.get(raw_type, raw_type)


def _parse_mm(val: str) -> float | None:
    """Parse a float from NDBC fixed-width field. MM means missing."""
    val = val.strip()
    if not val or val == "MM":
        return None
    try:
        return float(val)
    except ValueError:
        return None


def _ft_to_m(v: float | None) -> float | None:
    """Convert feet to metres. NDBC TIDE field is in feet above/below MLLW."""
    return v * 0.3048 if v is not None else None


def _nmi_to_m(v: float | None) -> float | None:
    """Convert nautical miles to metres. NDBC VIS field is in nautical miles."""
    return v * 1852.0 if v is not None else None


def _parse_ndbc_time_ymd(yyyy: str, mm: str, dd: str, hh: str, mn: str) -> datetime | None:
    """Parse YYYY MM DD hh mm fields from NDBC latest_obs.txt."""
    try:
        dt = datetime(int(yyyy), int(mm), int(dd), int(hh), int(mn),
                      tzinfo=timezone.utc)
        return dt
    except (ValueError, TypeError):
        return None


def _parse_ndbc_time(date_str: str, time_str: str) -> datetime | None:
    """Parse date (MM/DD/YYYY) and time (hh:mm) from NDBC (legacy format)."""
    try:
        dt = datetime.strptime(f"{date_str} {time_str}", "%m/%d/%Y %H:%M")
        return dt.replace(tzinfo=timezone.utc)
    except ValueError:
        return None


# latest_obs.txt column layout (space-separated).
# Header row 1: column names
# Header row 2: units
# Data rows: space-separated
#
# Actual columns (as of 2026):
# STN  LAT  LON  YYYY MM DD hh mm  WDIR WSPD GST WVHT DPD APD MWD PRES PTDY ATMP WTMP DEWP VIS TIDE
# 0    1    2    3    4  5  6  7    8    9    10  11   12  13  14  15   16   17   18   19   20  21
#
# Legacy format (used in tests):
# STN  LAT  LON  DATE       TIME   WDIR WSPD GST WVHT DPD APD MWD PRES PTDY ATMP WTMP DEWP VIS TIDE
# 0    1    2    3          4      5    6    7   8    9   10  11  12   13   14   15   16   17  18


def _detect_format(header_line: str) -> str:
    """Detect whether this is YYYY-MM-DD-hh-mm format or legacy DATE TIME."""
    cols = header_line.lstrip("#").split()
    # Real NDBC data has YYYY as 4th column header
    if len(cols) > 4 and cols[3] == "YYYY":
        return "ymd"
    return "legacy"


def parse_ndbc_latest_obs(
    text: str, metadata: NDBCMetadata
) -> list[ObservationStation]:
    """Parse NDBC latest_obs.txt into ObservationStation objects."""
    lines = text.strip().split("\n")
    if len(lines) < 3:
        logger.warning("NDBC latest_obs has fewer than 3 lines")
        return []

    fmt = _detect_format(lines[0])

    # Skip 2 header lines
    stations: list[ObservationStation] = []
    for line in lines[2:]:
        parts = line.split()

        if fmt == "ymd":
            if len(parts) < 20:
                continue
            stn = parts[0].strip()
            lat = _parse_mm(parts[1])
            lon = _parse_mm(parts[2])
            if lat is None or lon is None:
                continue
            time = _parse_ndbc_time_ymd(parts[3], parts[4], parts[5],
                                        parts[6], parts[7])
            if time is None:
                continue
            # Data fields start at index 8
            # WSPD/GST: m/s, stored as-is.  VIS: nmi → km.  TIDE: feet → metres.
            station = ObservationStation(
                platform_code=stn,
                platform_type=metadata.get_type(stn),
                lat=lat,
                lon=lon,
                time=time,
                country="US",
                wind_dir=_parse_mm(parts[8]),
                wind_spd=_parse_mm(parts[9]),
                gust=_parse_mm(parts[10]),
                wave_ht=_parse_mm(parts[11]),
                wave_period=_parse_mm(parts[12]),
                wave_avg_period=_parse_mm(parts[13]),
                wave_dir=_parse_mm(parts[14]),
                pressure=_parse_mm(parts[15]),
                pressure_tendency=_parse_mm(parts[16]),
                air_temp=_parse_mm(parts[17]),
                sea_temp=_parse_mm(parts[18]),
                dewpoint=_parse_mm(parts[19]),
                vis=_nmi_to_m(_parse_mm(parts[20])) if len(parts) > 20 else None,
                water_level=_ft_to_m(_parse_mm(parts[21])) if len(parts) > 21 else None,
                source="ndbc",
            )
        else:
            # Legacy format: DATE TIME as two columns
            if len(parts) < 17:
                continue
            stn = parts[0].strip()
            lat = _parse_mm(parts[1])
            lon = _parse_mm(parts[2])
            if lat is None or lon is None:
                continue
            time = _parse_ndbc_time(parts[3], parts[4])
            if time is None:
                continue
            # WSPD/GST: m/s, stored as-is.  VIS: nmi → km.  TIDE: feet → metres.
            station = ObservationStation(
                platform_code=stn,
                platform_type=metadata.get_type(stn),
                lat=lat,
                lon=lon,
                time=time,
                country="US",
                wind_dir=_parse_mm(parts[5]),
                wind_spd=_parse_mm(parts[6]),
                gust=_parse_mm(parts[7]),
                wave_ht=_parse_mm(parts[8]),
                wave_period=_parse_mm(parts[9]),
                wave_avg_period=_parse_mm(parts[10]),
                wave_dir=_parse_mm(parts[11]),
                pressure=_parse_mm(parts[12]),
                pressure_tendency=_parse_mm(parts[13]),
                air_temp=_parse_mm(parts[14]),
                sea_temp=_parse_mm(parts[15]),
                dewpoint=_parse_mm(parts[16]),
                vis=_nmi_to_m(_parse_mm(parts[17])) if len(parts) > 17 else None,
                water_level=_ft_to_m(_parse_mm(parts[18])) if len(parts) > 18 else None,
                source="ndbc",
            )
        station.normalize()
        if not station.is_valid():
            logger.debug("Dropping invalid NDBC station %s", station.platform_code)
            continue
        stations.append(station)

    logger.info("Parsed %d stations from NDBC latest_obs", len(stations))
    return stations


async def fetch_ndbc_metadata(client: httpx.AsyncClient) -> NDBCMetadata:
    """Fetch and parse NDBC activestations.xml."""
    logger.info("Fetching NDBC metadata: %s", NDBC_ACTIVE_STATIONS_URL)
    resp = await client.get(NDBC_ACTIVE_STATIONS_URL, timeout=HTTP_TIMEOUT_SECONDS)
    resp.raise_for_status()
    meta = NDBCMetadata()
    meta.load_from_xml(resp.text)
    return meta


async def fetch_ndbc_latest(
    client: httpx.AsyncClient, metadata: NDBCMetadata
) -> list[ObservationStation]:
    """Fetch and parse NDBC latest observations."""
    logger.info("Fetching NDBC latest_obs: %s", NDBC_LATEST_OBS_URL)
    resp = await client.get(NDBC_LATEST_OBS_URL, timeout=HTTP_TIMEOUT_SECONDS)
    resp.raise_for_status()
    return parse_ndbc_latest_obs(resp.text, metadata)
