from __future__ import annotations

OSMC_BASE_URL = (
    "https://osmc.noaa.gov/erddap/tabledap/OSMC_flattened.csv"
)
OSMC_FIELDS = (
    "platform_code,platform_type,country,latitude,longitude,time,"
    "sst,atmp,slp,windspd,winddir,wvht,waterlevel,clouds,dewpoint"
)
OSMC_LOOKBACK_HOURS = 6
OSMC_FETCH_INTERVAL_SECONDS = 15 * 60  # 15 minutes

NDBC_LATEST_OBS_URL = (
    "https://www.ndbc.noaa.gov/data/latest_obs/latest_obs.txt"
)
NDBC_ACTIVE_STATIONS_URL = (
    "https://www.ndbc.noaa.gov/activestations.xml"
)
NDBC_FETCH_INTERVAL_SECONDS = 5 * 60  # 5 minutes

PURGE_INTERVAL_SECONDS = 15 * 60  # 15 minutes
MAX_OBSERVATION_AGE_HOURS = 12

SERVER_HOST = "0.0.0.0"
SERVER_PORT = 8080

HTTP_TIMEOUT_SECONDS = 30
