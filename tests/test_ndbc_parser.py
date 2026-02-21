import pytest

from app.fetchers.ndbc import NDBCMetadata, parse_ndbc_latest_obs

SAMPLE_LATEST_OBS = """\
#STN     LAT      LON   DATE        TIME    WDIR  WSPD  GST   WVHT  DPD   APD   MWD   PRES    PTDY   ATMP   WTMP   DEWP   VIS   TIDE
#        deg      deg               hhmm    degT  m/s   m/s    m    sec   sec   degT   hPa     hPa   degC   degC   degC    mi     ft
41008   31.400  -80.870 02/20/2026 14:50    170    5.0   7.0   1.2   6.0  4.5    180  1014.7   MM    14.8   18.2   10.1    MM     MM
46002   42.640 -130.360 02/20/2026 14:00    200   10.0  14.0   2.5  10.0  7.0    210  1020.0  -1.2    8.5    9.0    5.0    MM     MM
SAQG1   31.250  -81.280 02/20/2026 13:30     MM    MM    MM    MM    MM    MM     MM  1015.0   MM    15.0    MM    11.0    MM     MM
"""


def _make_meta() -> NDBCMetadata:
    meta = NDBCMetadata()
    meta.stations = {
        "41008": {"name": "Grays Reef", "type": "buoy", "owner": "NDBC"},
        "46002": {"name": "Oregon Buoy", "type": "buoy", "owner": "NDBC"},
        "SAQG1": {"name": "St Simons", "type": "fixed", "owner": "NDBC"},
    }
    return meta


def test_parse_basic():
    meta = _make_meta()
    stations = parse_ndbc_latest_obs(SAMPLE_LATEST_OBS, meta)
    assert len(stations) == 3


def test_buoy_fields():
    meta = _make_meta()
    stations = parse_ndbc_latest_obs(SAMPLE_LATEST_OBS, meta)
    buoy = stations[0]
    assert buoy.platform_code == "41008"
    assert buoy.platform_type == "buoy"
    assert buoy.lat == 31.4
    assert buoy.lon == -80.87
    assert buoy.wind_dir == 170.0
    assert buoy.wind_spd == 5.0
    assert buoy.gust == 7.0
    assert buoy.wave_ht == 1.2
    assert buoy.wave_period == 6.0
    assert buoy.pressure == 1014.7
    assert buoy.air_temp == 14.8
    assert buoy.sea_temp == 18.2
    assert buoy.source == "ndbc"
    assert buoy.country == "US"


def test_mm_handling():
    meta = _make_meta()
    stations = parse_ndbc_latest_obs(SAMPLE_LATEST_OBS, meta)
    # 41008 has MM for PTDY, VIS, TIDE
    buoy = stations[0]
    assert buoy.pressure_tendency is None
    assert buoy.vis is None
    assert buoy.water_level is None

    # SAQG1 has MM for most fields
    shore = stations[2]
    assert shore.wind_dir is None
    assert shore.wind_spd is None
    assert shore.wave_ht is None
    assert shore.pressure == 1015.0
    assert shore.air_temp == 15.0


def test_shore_type_from_metadata():
    meta = _make_meta()
    stations = parse_ndbc_latest_obs(SAMPLE_LATEST_OBS, meta)
    shore = stations[2]
    assert shore.platform_type == "shore"  # "fixed" -> "shore"


def test_empty_input():
    meta = _make_meta()
    assert parse_ndbc_latest_obs("", meta) == []
    assert parse_ndbc_latest_obs("header\nunits\n", meta) == []


def test_metadata_loading():
    xml = """<?xml version="1.0"?>
    <stations>
      <station id="41008" lat="31.4" lon="-80.87" name="Grays Reef" type="buoy" owner="NDBC"/>
      <station id="SAQG1" lat="31.25" lon="-81.28" name="St Simons" type="fixed" owner="NDBC"/>
    </stations>
    """
    meta = NDBCMetadata()
    meta.load_from_xml(xml)
    assert len(meta.stations) == 2
    assert meta.get_type("41008") == "buoy"
    assert meta.get_type("SAQG1") == "shore"
    assert meta.get_type("UNKNOWN_ID") == "buoy"  # default


# Real NDBC format: YYYY MM DD hh mm (5 separate time columns)
SAMPLE_REAL_FORMAT = """\
#STN       LAT      LON  YYYY MM DD hh mm WDIR WSPD   GST WVHT  DPD APD MWD   PRES  PTDY  ATMP  WTMP  DEWP  VIS   TIDE
#text      deg      deg   yr mo day hr mn degT  m/s   m/s   m   sec sec degT   hPa   hPa  degC  degC  degC  nmi     ft
22101    37.24   126.02  2026 02 20 17 00 190   8.0    MM  0.5   0   MM  MM     MM    MM   5.2   3.9    MM   1.0     MM
41008    31.40   -80.87  2026 02 20 17 50 170   5.0   7.0  1.2   6  4.5 180  1014.7  MM   14.8  18.2  10.1  MM     2.0
"""


def test_parse_real_format():
    meta = _make_meta()
    meta.stations["22101"] = {"name": "Korean Buoy", "type": "buoy", "owner": "KMA"}
    stations = parse_ndbc_latest_obs(SAMPLE_REAL_FORMAT, meta)
    assert len(stations) == 2

    s0 = stations[0]
    assert s0.platform_code == "22101"
    assert s0.lat == 37.24
    assert s0.lon == 126.02
    assert s0.wind_dir == 190.0
    assert s0.wind_spd == 8.0
    assert s0.gust is None  # MM
    assert s0.air_temp == 5.2
    assert s0.sea_temp == 3.9
    assert s0.time.year == 2026
    assert s0.time.month == 2
    assert s0.time.hour == 17

    # VIS 1.0 nmi → 1852.0 m
    assert s0.vis == pytest.approx(1852.0)

    s1 = stations[1]
    assert s1.platform_code == "41008"
    assert s1.pressure == 1014.7
    assert s1.wave_ht == 1.2
    # TIDE 2.0 ft → 0.6096 m
    assert s1.water_level == pytest.approx(0.6096)
    assert s1.vis is None  # MM
