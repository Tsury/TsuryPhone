from __future__ import annotations

from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.typing import ConfigType

from .const import DOMAIN, LOGGER


async def async_setup(hass: HomeAssistant, config: ConfigType) -> bool:
    return True


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    LOGGER.debug("Setting up TsuryPhone entry: %s", entry.entry_id)
    hass.data.setdefault(DOMAIN, {})
    # Coordinator and platform setup will be deferred until implemented
    return True


async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    LOGGER.debug("Unloading TsuryPhone entry: %s", entry.entry_id)
    # Unload platforms when they are registered
    return True

