from datetime import datetime, timedelta, timezone

from app.dedup import merge_station
from app.models import ObservationStation
from app.store import StationStore


def _now() -> datetime:
    return datetime.now(timezone.utc)


def _ago(**kwargs) -> datetime:
    return _now() - timedelta(**kwargs)


def _make_station(
    code: str = "41008",
    ptype: str = "buoy",
    time: datetime | None = None,
    source: str = "osmc",
    air_temp: float | None = None,
    wind_spd: float | None = None,
) -> ObservationStation:
    return ObservationStation(
        platform_code=code,
        platform_type=ptype,
        lat=31.4,
        lon=-80.9,
        time=time if time is not None else _ago(hours=1),
        source=source,
        air_temp=air_temp,
        wind_spd=wind_spd,
    )


def test_merge_keeps_newer():
    old = _make_station(time=_ago(hours=3), source="osmc", air_temp=10.0)
    new = _make_station(time=_ago(hours=1), source="ndbc", air_temp=15.0)
    result = merge_station(old, new)
    assert result.source == "ndbc"
    assert result.air_temp == 15.0


def test_merge_keeps_existing_if_newer():
    old = _make_station(time=_ago(hours=1), source="ndbc", air_temp=15.0)
    new = _make_station(time=_ago(hours=3), source="osmc", air_temp=10.0)
    result = merge_station(old, new)
    assert result.source == "ndbc"
    assert result.air_temp == 15.0


def test_store_ndbc_enriches_osmc():
    store = StationStore()
    osmc_stations = [
        _make_station(code="41008", time=_ago(hours=3), source="osmc"),
        _make_station(code="46002", time=_ago(hours=3), source="osmc"),
    ]
    store.update_from_osmc(osmc_stations)
    assert store.count == 2

    ndbc_stations = [
        _make_station(code="41008", time=_ago(hours=1), source="ndbc", air_temp=20.0),
    ]
    store.update_from_ndbc(ndbc_stations)
    assert store.count == 2  # still 2, 41008 was updated not added

    results = store.query(max_age_hours=6)
    s41008 = [s for s in results if s.platform_code == "41008"][0]
    assert s41008.source == "ndbc"
    assert s41008.air_temp == 20.0


def test_store_synthetic_ship_keys_preserved():
    store = StationStore()
    ships = [
        _make_station(code="SHIP_-30.1_45.2_1000", ptype="ship"),
        _make_station(code="SHIP_-31.0_46.0_1001", ptype="ship"),
    ]
    store.update_from_osmc(ships)
    assert store.count == 2  # both preserved (different keys)


def test_store_purge():
    store = StationStore()
    stations = [
        _make_station(code="OLD", time=_ago(hours=25)),
        _make_station(code="NEW", time=_ago(hours=1)),
    ]
    store.update_from_osmc(stations)
    assert store.count == 2

    purged = store.purge_old(max_age_hours=12)
    assert purged == 1
    assert store.count == 1


def test_store_query_bbox():
    store = StationStore()
    in_station  = _make_station(code="IN")   # lat=31.4, lon=-80.9
    out_station = _make_station(code="OUT")
    out_station.lat = 60.0
    out_station.lon = 10.0
    store.update_from_osmc([in_station, out_station])

    results = store.query(lat_min=30, lat_max=35, lon_min=-85, lon_max=-75, max_age_hours=6)
    assert len(results) == 1
    assert results[0].platform_code == "IN"


def test_store_query_type_filter():
    store = StationStore()
    stations = [
        _make_station(code="BUOY1", ptype="buoy"),
        _make_station(code="SHIP1", ptype="ship"),
    ]
    store.update_from_osmc(stations)

    results = store.query(types={"ship"}, max_age_hours=6)
    assert len(results) == 1
    assert results[0].platform_code == "SHIP1"
