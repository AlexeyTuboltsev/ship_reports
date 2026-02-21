from __future__ import annotations

import logging
import threading
from datetime import datetime, timedelta, timezone

from app.dedup import merge_station
from app.models import ObservationStation

logger = logging.getLogger(__name__)


class StationStore:
    """Thread-safe in-memory store of observation stations, keyed by platform_code."""

    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._stations: dict[str, ObservationStation] = {}

    @property
    def count(self) -> int:
        with self._lock:
            return len(self._stations)

    def update_from_osmc(self, stations: list[ObservationStation]) -> None:
        """Bulk load OSMC stations. For each station, merge with existing if present."""
        with self._lock:
            for s in stations:
                existing = self._stations.get(s.platform_code)
                if existing is None:
                    self._stations[s.platform_code] = s
                else:
                    self._stations[s.platform_code] = merge_station(existing, s)
        logger.info("Store updated from OSMC: %d incoming, %d total", len(stations), self.count)

    def update_from_ndbc(self, stations: list[ObservationStation]) -> None:
        """Merge NDBC stations. NDBC enriches/overrides OSMC for matching platform_codes."""
        with self._lock:
            for s in stations:
                existing = self._stations.get(s.platform_code)
                if existing is None:
                    self._stations[s.platform_code] = s
                else:
                    self._stations[s.platform_code] = merge_station(existing, s)
        logger.info("Store updated from NDBC: %d incoming, %d total", len(stations), self.count)

    def purge_old(self, max_age_hours: int) -> int:
        """Remove observations older than max_age_hours. Returns number purged."""
        cutoff = datetime.now(timezone.utc) - timedelta(hours=max_age_hours)
        with self._lock:
            before = len(self._stations)
            self._stations = {
                k: v for k, v in self._stations.items() if v.time >= cutoff
            }
            purged = before - len(self._stations)
        if purged:
            logger.info("Purged %d stale observations (older than %dh)", purged, max_age_hours)
        return purged

    def query(
        self,
        lat_min: float = -90,
        lat_max: float = 90,
        lon_min: float = -180,
        lon_max: float = 180,
        max_age_hours: float = 6,
        types: set[str] | None = None,
    ) -> list[ObservationStation]:
        """Return stations matching bbox, age, and type filters."""
        cutoff = datetime.now(timezone.utc) - timedelta(hours=max_age_hours)
        results: list[ObservationStation] = []
        with self._lock:
            for s in self._stations.values():
                if s.time < cutoff:
                    continue
                if not (lat_min <= s.lat <= lat_max):
                    continue
                if not (lon_min <= s.lon <= lon_max):
                    continue
                if types and s.platform_type not in types:
                    continue
                results.append(s)
        return results

    def oldest_observation(self) -> datetime | None:
        """Return the timestamp of the oldest observation in the store."""
        with self._lock:
            if not self._stations:
                return None
            return min(s.time for s in self._stations.values())
