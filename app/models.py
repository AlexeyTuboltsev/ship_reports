from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime
from typing import Optional


@dataclass(slots=True)
class ObservationStation:
    """A single weather observation from a ship, buoy, or shore station."""

    platform_code: str  # WMO ID, call sign, or synthetic key
    platform_type: str  # normalized: ship, buoy, drifter, shore, other
    lat: float
    lon: float
    time: datetime
    country: Optional[str] = None

    # Wind
    wind_dir: Optional[float] = None  # degrees true
    wind_spd: Optional[float] = None  # m/s
    gust: Optional[float] = None      # m/s

    # Pressure
    pressure: Optional[float] = None  # hPa
    pressure_tendency: Optional[float] = None  # hPa

    # Temperature
    air_temp: Optional[float] = None  # deg C
    sea_temp: Optional[float] = None  # deg C
    dewpoint: Optional[float] = None  # deg C

    # Waves
    wave_ht: Optional[float] = None          # metres
    wave_period: Optional[float] = None      # seconds (dominant)
    wave_avg_period: Optional[float] = None  # seconds (average)
    wave_dir: Optional[float] = None         # degrees true (mean wave direction)

    # Other
    vis: Optional[float] = None          # metres  (NDBC sends nmi — converted at ingest)
    water_level: Optional[float] = None  # metres  (NDBC TIDE sends feet — converted at ingest)
    clouds: Optional[float] = None  # oktas or percent

    # Source tracking
    source: str = ""  # "osmc" or "ndbc"

    def normalize(self) -> None:
        """Normalize fields to the expected format in-place.

        Longitude is mapped to [-180, 180) regardless of what the source
        provides (some GTS contributors use 0–360).  Optional measurement
        fields that fall outside physically plausible ranges are cleared to
        None rather than letting bad data reach the API.
        """
        # Longitude: normalize only if outside [-180, 180].
        # Skipping in-range values avoids introducing floating-point error.
        if self.lon < -180.0 or self.lon > 180.0:
            self.lon = ((self.lon + 180.0) % 360.0) - 180.0

        # Wind direction: valid compass bearing is [0, 360].  Values outside
        # that range are data errors; drop the field rather than guessing.
        if self.wind_dir is not None and not (0.0 <= self.wind_dir <= 360.0):
            self.wind_dir = None

        # Speeds must be non-negative (unit: knots)
        if self.wind_spd is not None and self.wind_spd < 0.0:
            self.wind_spd = None
        if self.gust is not None and self.gust < 0.0:
            self.gust = None

        # Sea-level pressure: 800–1100 hPa covers all recorded extremes
        if self.pressure is not None and not (800.0 <= self.pressure <= 1100.0):
            self.pressure = None

        # Temperatures: air −90 to +60 °C, sea −5 to +40 °C
        if self.air_temp is not None and not (-90.0 <= self.air_temp <= 60.0):
            self.air_temp = None
        if self.sea_temp is not None and not (-5.0 <= self.sea_temp <= 40.0):
            self.sea_temp = None

        # Wave height and visibility must be non-negative
        if self.wave_ht is not None and self.wave_ht < 0.0:
            self.wave_ht = None
        if self.vis is not None and self.vis < 0.0:
            self.vis = None

    def is_valid(self) -> bool:
        """Return True if the mandatory fields are present and in range.

        Call normalize() before is_valid() so that lon is already in [-180, 180).
        Mandatory: non-empty platform_code, lat in [-90, 90], lon in [-180, 180].
        time is always set by construction (required dataclass field).
        Optional measurement fields are never a reason to reject a station.
        """
        if not self.platform_code:
            return False
        if not (-90.0 <= self.lat <= 90.0):
            return False
        if not (-180.0 <= self.lon <= 180.0):
            return False
        return True

    def to_api_dict(self) -> dict:
        """Serialize for the JSON API response.

        API contract — server always stores and returns SI/metric units.
        Unit conversion to display units (knots, nm, ft, °F …) is the client's job.

          id:         string
          type:       string  (ship | buoy | drifter | shore | other)
          lat:        float   degrees [-90, 90], 4 d.p.
          lon:        float   degrees [-180, 180], 4 d.p.
          time:       string  ISO-8601 UTC  "2026-02-21T14:00:00Z"
          wind_dir:   float   degrees true
          wind_spd:   float   m/s
          gust:       float   m/s
          pressure:   float   hPa (= mbar)
          air_temp:   float   °C
          sea_temp:   float   °C
          wave_ht:    float   metres
          vis:        float   metres
        Optional numeric fields, when present, are always JSON floats (never int/null).
        """
        d: dict = {
            "id":   self.platform_code,
            "type": self.platform_type,
            "lat":  float(round(self.lat, 4)),
            "lon":  float(round(self.lon, 4)),
            "time": self.time.strftime("%Y-%m-%dT%H:%M:%SZ"),
        }
        if self.country:
            d["country"] = self.country

        _optional = [
            ("wind_dir", self.wind_dir),
            ("wind_spd", self.wind_spd),
            ("gust",     self.gust),
            ("pressure", self.pressure),
            ("air_temp", self.air_temp),
            ("sea_temp", self.sea_temp),
            ("wave_ht",  self.wave_ht),
            ("vis",      self.vis),
        ]
        for key, val in _optional:
            if val is not None:
                d[key] = float(round(val, 2))

        return d
