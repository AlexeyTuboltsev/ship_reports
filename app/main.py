from __future__ import annotations

import asyncio
import logging
import re
import time
from contextlib import asynccontextmanager
from datetime import datetime, timezone

import httpx
from fastapi import FastAPI, Query
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse

from app.config import (
    MAX_OBSERVATION_AGE_HOURS,
    NDBC_FETCH_INTERVAL_SECONDS,
    OSMC_FETCH_INTERVAL_SECONDS,
    PURGE_INTERVAL_SECONDS,
)
from app.fetchers.ndbc import NDBCMetadata, fetch_ndbc_latest, fetch_ndbc_metadata
from app.fetchers.osmc import fetch_osmc
from app.store import StationStore

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(name)s: %(message)s",
)
logger = logging.getLogger(__name__)

store = StationStore()
_start_time: float = 0.0

# Source status tracking
_source_status: dict[str, dict] = {
    "osmc": {"last_fetch": None, "stations": 0, "status": "pending"},
    "ndbc": {"last_fetch": None, "stations": 0, "status": "pending"},
}
_ndbc_meta = NDBCMetadata()


def _parse_age(age_str: str) -> float:
    """Parse an age string like '6h' or '30m' into hours."""
    m = re.match(r"^(\d+(?:\.\d+)?)\s*(h|m)$", age_str.strip().lower())
    if not m:
        return 6.0  # default
    val = float(m.group(1))
    unit = m.group(2)
    if unit == "m":
        return val / 60.0
    return val


async def _fetch_osmc_task(client: httpx.AsyncClient) -> None:
    """Fetch OSMC data and update the store."""
    try:
        stations = await fetch_osmc(client)
        store.update_from_osmc(stations)
        _source_status["osmc"] = {
            "last_fetch": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "stations": len(stations),
            "status": "ok",
        }
    except Exception:
        logger.exception("OSMC fetch failed")
        _source_status["osmc"]["status"] = "error"


async def _fetch_ndbc_task(client: httpx.AsyncClient) -> None:
    """Fetch NDBC data and update the store."""
    try:
        stations = await fetch_ndbc_latest(client, _ndbc_meta)
        store.update_from_ndbc(stations)
        _source_status["ndbc"] = {
            "last_fetch": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "stations": len(stations),
            "status": "ok",
        }
    except Exception:
        logger.exception("NDBC fetch failed")
        _source_status["ndbc"]["status"] = "error"


async def _scheduler(client: httpx.AsyncClient) -> None:
    """Run periodic fetch tasks."""
    osmc_interval = OSMC_FETCH_INTERVAL_SECONDS
    ndbc_interval = NDBC_FETCH_INTERVAL_SECONDS
    purge_interval = PURGE_INTERVAL_SECONDS

    osmc_next = 0.0
    ndbc_next = 0.0
    purge_next = 0.0

    while True:
        now = time.monotonic()

        tasks = []
        if now >= osmc_next:
            tasks.append(_fetch_osmc_task(client))
            osmc_next = now + osmc_interval
        if now >= ndbc_next:
            tasks.append(_fetch_ndbc_task(client))
            ndbc_next = now + ndbc_interval
        if now >= purge_next:
            store.purge_old(MAX_OBSERVATION_AGE_HOURS)
            purge_next = now + purge_interval

        if tasks:
            await asyncio.gather(*tasks, return_exceptions=True)

        # Sleep until the next event
        next_event = min(osmc_next, ndbc_next, purge_next)
        sleep_for = max(1.0, next_event - time.monotonic())
        await asyncio.sleep(sleep_for)


@asynccontextmanager
async def lifespan(app: FastAPI):
    global _start_time, _ndbc_meta

    _start_time = time.monotonic()

    async with httpx.AsyncClient() as client:
        # Load NDBC metadata at startup
        try:
            _ndbc_meta = await fetch_ndbc_metadata(client)
        except Exception:
            logger.exception("Failed to load NDBC metadata at startup")

        # Start the background scheduler
        task = asyncio.create_task(_scheduler(client))
        try:
            yield
        finally:
            task.cancel()
            try:
                await task
            except asyncio.CancelledError:
                pass


app = FastAPI(title="Ship & Buoy Observation Server", lifespan=lifespan)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["GET"],
    allow_headers=["*"],
)


@app.get("/api/v1/observations")
async def get_observations(
    lat_min: float = Query(-90, ge=-90, le=90),
    lat_max: float = Query(90, ge=-90, le=90),
    lon_min: float = Query(-180, ge=-180, le=180),
    lon_max: float = Query(180, ge=-180, le=180),
    max_age: str = Query("6h"),
    types: str = Query("all"),
):
    age_hours = _parse_age(max_age)

    type_filter: set[str] | None = None
    if types != "all":
        type_filter = {t.strip() for t in types.split(",") if t.strip()}

    results = store.query(
        lat_min=lat_min,
        lat_max=lat_max,
        lon_min=lon_min,
        lon_max=lon_max,
        max_age_hours=age_hours,
        types=type_filter,
    )

    return JSONResponse(
        content={
            "generated": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "count": len(results),
            "stations": [s.to_api_dict() for s in results],
        }
    )


@app.get("/api/v1/status")
async def get_status():
    elapsed = time.monotonic() - _start_time
    days = int(elapsed // 86400)
    hours = int((elapsed % 86400) // 3600)
    if days > 0:
        uptime_str = f"{days}d {hours}h"
    else:
        minutes = int((elapsed % 3600) // 60)
        uptime_str = f"{hours}h {minutes}m"

    oldest = store.oldest_observation()

    return JSONResponse(
        content={
            "uptime": uptime_str,
            "sources": _source_status,
            "total_stations": store.count,
            "oldest_observation": oldest.strftime("%Y-%m-%dT%H:%M:%SZ") if oldest else None,
        }
    )
