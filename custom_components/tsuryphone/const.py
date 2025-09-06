from __future__ import annotations

import logging

DOMAIN = "tsuryphone"
LOGGER = logging.getLogger(__package__)

# Ring pattern presets per design
RING_PRESETS: dict[str, str] = {
    "default": "",  # device default
    "pulse_short": "300,300x2",
    "classic": "500,500",
    "long_gap": "800,400",
    "triple": "300,300,300",
    "stagger": "500,250,500",
    "alarm": "200,200x5",
    "slow": "1000",
    "burst": "150,150x3",
    "custom": "__custom__",
}

# Error code mapping (subset; extend as needed)
ERROR_CODE_TO_TRANSLATION_KEY: dict[str, str] = {
    "WEB_INVALID_NUMBER": "error_invalid_number",
}

# Defaults
FALLBACK_POLL_INTERVAL_S: int = 30
REFETCH_INTERVAL_S: int = 300
SERVICE_CONCURRENCY_LIMIT: int = 4
EVENT_QUEUE_MAX: int = 300

