"""Data update coordinator for TsuryPhone."""
import asyncio
import logging
from datetime import timedelta
from typing import Any, Dict

import aiohttp
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.update_coordinator import DataUpdateCoordinator, UpdateFailed

from .const import (
    DOMAIN,
    DEFAULT_SCAN_INTERVAL,
    ENDPOINT_STATUS,
    ENDPOINT_STATS,
    ENDPOINT_DND,
    ENDPOINT_PHONEBOOK,
    ENDPOINT_SCREENED,
    ENDPOINT_ACTION_CALL,
    ENDPOINT_ACTION_HANGUP,
    ENDPOINT_ACTION_RESET,
    ENDPOINT_ACTION_REBOOT,
    ENDPOINT_ACTION_RING,
    CONF_HOST,
    CONF_PORT,
)

_LOGGER = logging.getLogger(__name__)


class TsuryPhoneDataUpdateCoordinator(DataUpdateCoordinator):
    """Class to manage fetching TsuryPhone data."""

    def __init__(self, hass: HomeAssistant, entry: ConfigEntry) -> None:
        """Initialize the coordinator."""
        self.host = entry.data[CONF_HOST]
        self.port = entry.data[CONF_PORT]
        self.base_url = f"http://{self.host}:{self.port}"
        
        super().__init__(
            hass,
            _LOGGER,
            name=DOMAIN,
            update_interval=timedelta(seconds=DEFAULT_SCAN_INTERVAL),
        )

    async def _async_update_data(self) -> Dict[str, Any]:
        """Fetch data from API endpoint."""
        try:
            async with aiohttp.ClientSession(timeout=aiohttp.ClientTimeout(total=10)) as session:
                # Gather all data in parallel
                status_task = self._fetch_endpoint(session, ENDPOINT_STATUS)
                stats_task = self._fetch_endpoint(session, ENDPOINT_STATS)
                dnd_task = self._fetch_endpoint(session, ENDPOINT_DND)
                phonebook_task = self._fetch_endpoint(session, ENDPOINT_PHONEBOOK)
                screened_task = self._fetch_endpoint(session, ENDPOINT_SCREENED)
                
                status, stats, dnd, phonebook, screened = await asyncio.gather(
                    status_task, stats_task, dnd_task, phonebook_task, screened_task,
                    return_exceptions=True
                )
                
                # Handle any exceptions
                data = {}
                if not isinstance(status, Exception):
                    data["status"] = status
                if not isinstance(stats, Exception):
                    data["stats"] = stats
                if not isinstance(dnd, Exception):
                    data["dnd"] = dnd
                if not isinstance(phonebook, Exception):
                    data["phonebook"] = phonebook
                if not isinstance(screened, Exception):
                    data["screened"] = screened
                
                if not data:
                    raise UpdateFailed("No data received from any endpoint")
                
                return data
                
        except asyncio.TimeoutError as err:
            raise UpdateFailed(f"Timeout connecting to TsuryPhone at {self.base_url}") from err
        except aiohttp.ClientError as err:
            raise UpdateFailed(f"Error connecting to TsuryPhone at {self.base_url}: {err}") from err

    async def _fetch_endpoint(self, session: aiohttp.ClientSession, endpoint: str) -> Dict[str, Any]:
        """Fetch data from a specific endpoint."""
        url = f"{self.base_url}{endpoint}"
        _LOGGER.debug("Fetching data from %s", url)
        
        async with session.get(url) as response:
            response.raise_for_status()
            return await response.json()

    async def call_number(self, number: str) -> None:
        """Make a call to the specified number."""
        await self._make_request("POST", ENDPOINT_ACTION_CALL, {"number": number})

    async def hangup(self) -> None:
        """Hang up the current call."""
        await self._make_request("POST", ENDPOINT_ACTION_HANGUP)

    async def reset_device(self) -> None:
        """Reset the device."""
        await self._make_request("POST", ENDPOINT_ACTION_RESET)

    async def reboot_device(self) -> None:
        """Reboot the device."""
        await self._make_request("POST", ENDPOINT_ACTION_REBOOT)

    async def ring_device(self, duration_ms: int) -> None:
        """Ring the device for specified duration."""
        await self._make_request("POST", ENDPOINT_ACTION_RING, {"duration": str(duration_ms)})

    async def set_dnd_enabled(self, enabled: bool) -> None:
        """Enable or disable Do Not Disturb."""
        await self._make_request("POST", ENDPOINT_DND, {"enabled": str(enabled).lower()})

    async def set_dnd_hours(self, start_hour: int, start_minute: int, end_hour: int, end_minute: int) -> None:
        """Set Do Not Disturb hours."""
        data = {
            "start_hour": str(start_hour),
            "start_minute": str(start_minute),
            "end_hour": str(end_hour),
            "end_minute": str(end_minute),
        }
        await self._make_request("POST", ENDPOINT_DND, data)

    async def add_phonebook_entry(self, name: str, number: str) -> None:
        """Add a phonebook entry."""
        data = {"action": "add", "name": name, "number": number}
        await self._make_request("POST", ENDPOINT_PHONEBOOK, data)

    async def remove_phonebook_entry(self, name: str) -> None:
        """Remove a phonebook entry."""
        data = {"action": "remove", "name": name}
        await self._make_request("POST", ENDPOINT_PHONEBOOK, data)

    async def add_screened_number(self, number: str) -> None:
        """Add a screened (blocked) number."""
        data = {"action": "add", "number": number}
        await self._make_request("POST", ENDPOINT_SCREENED, data)

    async def remove_screened_number(self, number: str) -> None:
        """Remove a screened (blocked) number."""
        data = {"action": "remove", "number": number}
        await self._make_request("POST", ENDPOINT_SCREENED, data)

    async def _make_request(self, method: str, endpoint: str, data: Dict[str, str] = None) -> None:
        """Make a request to the device."""
        url = f"{self.base_url}{endpoint}"
        
        try:
            async with aiohttp.ClientSession(timeout=aiohttp.ClientTimeout(total=10)) as session:
                if method == "POST" and data:
                    async with session.post(url, data=data) as response:
                        response.raise_for_status()
                else:
                    async with session.request(method, url) as response:
                        response.raise_for_status()
                        
        except asyncio.TimeoutError as err:
            raise UpdateFailed(f"Timeout during {method} request to {url}") from err
        except aiohttp.ClientError as err:
            raise UpdateFailed(f"Error during {method} request to {url}: {err}") from err
