from app.fetchers.osmc import normalize_platform_type, parse_osmc_csv

SAMPLE_CSV = """\
platform_code,platform_type,country,latitude,longitude,time,sst,atmp,slp,windspd,winddir,wvht,waterlevel,clouds,dewpoint
,,,degrees_north,degrees_east,UTC,degree_C,degree_C,hPa,m s-1,degrees_true,m,m,oktas,degree_C
41008,MOORED BUOYS,US,31.4,-80.87,2026-02-20T14:30:00Z,18.2,14.8,1014.7,5.0,170.0,NaN,NaN,NaN,10.1
KBGJ,VOLUNTEER OBSERVING SHIPS,PA,-8.5,115.3,2026-02-20T13:00:00Z,29.5,31.0,1008.0,3.0,220.0,NaN,NaN,6,25.0
SHIP,,,-30.1,45.2,2026-02-20T12:00:00Z,22.0,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN
"""


def test_parse_basic_stations():
    stations = parse_osmc_csv(SAMPLE_CSV)
    assert len(stations) == 3


def test_buoy_fields():
    stations = parse_osmc_csv(SAMPLE_CSV)
    buoy = stations[0]
    assert buoy.platform_code == "41008"
    assert buoy.platform_type == "buoy"
    assert buoy.country == "US"
    assert buoy.lat == 31.4
    assert buoy.lon == -80.87
    assert buoy.sea_temp == 18.2
    assert buoy.air_temp == 14.8
    assert buoy.pressure == 1014.7
    assert buoy.wind_spd == 5.0
    assert buoy.wind_dir == 170.0
    assert buoy.wave_ht is None  # NaN -> None
    assert buoy.source == "osmc"


def test_ship_fields():
    stations = parse_osmc_csv(SAMPLE_CSV)
    ship = stations[1]
    assert ship.platform_code == "KBGJ"
    assert ship.platform_type == "ship"
    assert ship.country == "PA"
    assert ship.clouds == 6.0


def test_anonymous_ship_synthetic_key():
    stations = parse_osmc_csv(SAMPLE_CSV)
    anon = stations[2]
    # Should have synthetic key: SHIP_{lat}_{lon}_{timestamp}
    assert anon.platform_code.startswith("SHIP_")
    assert "-30.1" in anon.platform_code
    assert "45.2" in anon.platform_code
    assert anon.platform_type == "other"  # empty platform_type -> other


def test_nan_handling():
    stations = parse_osmc_csv(SAMPLE_CSV)
    anon = stations[2]
    assert anon.air_temp is None
    assert anon.pressure is None
    assert anon.wind_spd is None


def test_normalize_platform_type():
    assert normalize_platform_type("MOORED BUOYS") == "buoy"
    assert normalize_platform_type("VOLUNTEER OBSERVING SHIPS") == "ship"
    assert normalize_platform_type("DRIFTING BUOYS") == "drifter"
    assert normalize_platform_type("C-MAN WEATHER STATIONS") == "shore"
    assert normalize_platform_type("UNKNOWN") == "other"
    assert normalize_platform_type("SOMETHING NEW") == "other"


def test_empty_csv():
    assert parse_osmc_csv("") == []
    assert parse_osmc_csv("header\nunits\n") == []


def test_to_api_dict():
    stations = parse_osmc_csv(SAMPLE_CSV)
    d = stations[0].to_api_dict()
    assert d["id"] == "41008"
    assert d["type"] == "buoy"
    assert d["wind_spd"] == 5.0
    assert "wave_ht" not in d  # None values omitted
