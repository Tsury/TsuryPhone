from __future__ import annotations

from homeassistant.core import HomeAssistant
from homeassistant.helpers.storage import Store

from .const import DOMAIN, LOGGER
from .model import TsuryPhoneState


class StorageCache:
    def __init__(self, hass: HomeAssistant, entry_id: str) -> None:
        self._store = Store(hass, 1, f"{DOMAIN}/{entry_id}.json")

    async def load(self) -> TsuryPhoneState | None:
        data = await self._store.async_load()
        if not data:
            return None
        try:
            state = TsuryPhoneState(**data)
            return state
        except Exception as err:
            LOGGER.warning("Failed to load state cache: %s", err)
            return None

    async def save(self, state: TsuryPhoneState) -> None:
        await self._store.async_save(state.__dict__)

