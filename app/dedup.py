from __future__ import annotations

from app.models import ObservationStation


def merge_station(existing: ObservationStation, incoming: ObservationStation) -> ObservationStation:
    """Merge two observations for the same platform_code.

    Keep the newer observation. If the incoming observation is newer (or same age),
    it wins entirely. We don't field-merge because each observation is a snapshot.
    """
    if incoming.time >= existing.time:
        return incoming
    return existing
