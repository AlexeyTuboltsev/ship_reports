from datetime import datetime, timedelta, timezone
from unittest.mock import AsyncMock, patch

import pytest
from fastapi.testclient import TestClient

from app.main import app, store
from app.models import ObservationStation


@pytest.fixture(autouse=True)
def _clear_store():
    """Reset the store before each test."""
    store._stations.clear()
    yield
    store._stations.clear()


def _add_stations():
    """Populate the store with test data (timestamps relative to now)."""
    recent = datetime.now(timezone.utc) - timedelta(hours=1)
    stations = [
        ObservationStation(
            platform_code="41008",
            platform_type="buoy",
            lat=31.4,
            lon=-80.9,
            time=recent,
            country="US",
            wind_spd=5.0,
            wind_dir=170.0,
            pressure=1014.7,
            air_temp=14.8,
            source="ndbc",
        ),
        ObservationStation(
            platform_code="KBGJ",
            platform_type="ship",
            lat=-8.5,
            lon=115.3,
            time=recent,
            country="PA",
            wind_spd=3.0,
            source="osmc",
        ),
        ObservationStation(
            platform_code="SAQG1",
            platform_type="shore",
            lat=31.25,
            lon=-81.28,
            time=recent,
            country="US",
            pressure=1015.0,
            source="ndbc",
        ),
    ]
    for s in stations:
        store._stations[s.platform_code] = s


# Use TestClient without lifespan (we don't want real HTTP fetches in tests)
client = TestClient(app, raise_server_exceptions=True)


def test_observations_returns_json():
    _add_stations()
    resp = client.get("/api/v1/observations?max_age=24h")
    assert resp.status_code == 200
    data = resp.json()
    assert "generated" in data
    assert "count" in data
    assert "stations" in data
    assert data["count"] == 3


def test_observations_bbox_filter():
    _add_stations()
    resp = client.get(
        "/api/v1/observations?lat_min=30&lat_max=35&lon_min=-85&lon_max=-75&max_age=24h"
    )
    data = resp.json()
    # Both 41008 (31.4, -80.9) and SAQG1 (31.25, -81.28) are in this bbox
    assert data["count"] == 2
    ids = {s["id"] for s in data["stations"]}
    assert "41008" in ids
    assert "SAQG1" in ids
    # Ship KBGJ at (-8.5, 115.3) should be excluded
    assert "KBGJ" not in ids


def test_observations_type_filter():
    _add_stations()
    resp = client.get("/api/v1/observations?types=ship&max_age=24h")
    data = resp.json()
    assert data["count"] == 1
    assert data["stations"][0]["type"] == "ship"


def test_observations_multiple_types():
    _add_stations()
    resp = client.get("/api/v1/observations?types=buoy,shore&max_age=24h")
    data = resp.json()
    assert data["count"] == 2
    types = {s["type"] for s in data["stations"]}
    assert types == {"buoy", "shore"}


def test_observations_empty_bbox():
    _add_stations()
    resp = client.get(
        "/api/v1/observations?lat_min=70&lat_max=80&lon_min=0&lon_max=10&max_age=24h"
    )
    data = resp.json()
    assert data["count"] == 0
    assert data["stations"] == []


def test_status_endpoint():
    _add_stations()
    resp = client.get("/api/v1/status")
    assert resp.status_code == 200
    data = resp.json()
    assert "uptime" in data
    assert "sources" in data
    assert "total_stations" in data
    assert data["total_stations"] == 3


def test_observation_fields_omit_none():
    _add_stations()
    resp = client.get("/api/v1/observations?max_age=24h")
    data = resp.json()
    buoy = [s for s in data["stations"] if s["id"] == "41008"][0]
    assert "wind_spd" in buoy
    assert "wave_ht" not in buoy  # None values should be omitted
    assert "sea_temp" not in buoy
