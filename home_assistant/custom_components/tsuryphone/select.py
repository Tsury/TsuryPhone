"""Select platform for TsuryPhone."""
import logging
from typing import Any, List, Optional

from homeassistant.components.select import SelectEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN, MANUFACTURER, MODEL
from .coordinator import TsuryPhoneDataUpdateCoordinator

_LOGGER = logging.getLogger(__name__)


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up TsuryPhone select based on a config entry."""
    coordinator = hass.data[DOMAIN][config_entry.entry_id]

    entities = [
        TsuryPhoneCallSelect(coordinator),
    ]

    async_add_entities(entities)


class TsuryPhoneBaseSelect(CoordinatorEntity, SelectEntity):
    """Base class for TsuryPhone selects."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator, select_type: str) -> None:
        """Initialize the select."""
        super().__init__(coordinator)
        self._select_type = select_type
        self._attr_unique_id = f"{coordinator.base_url}_{select_type}"
        self._attr_device_info = {
            "identifiers": {(DOMAIN, coordinator.base_url)},
            "name": "TsuryPhone",
            "manufacturer": MANUFACTURER,
            "model": MODEL,
            "configuration_url": coordinator.base_url,
        }


class TsuryPhoneCallSelect(TsuryPhoneBaseSelect):
    """Select entity for calling phonebook entries."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator) -> None:
        """Initialize the select."""
        super().__init__(coordinator, "call_phonebook")
        self._attr_name = "TsuryPhone Call Phonebook"
        self._attr_icon = "mdi:phone-dial"
        
    @property
    def options(self) -> List[str]:
        """Return the list of available options."""
        options = ["Select contact..."]
        if "phonebook" in self.coordinator.data:
            entries = self.coordinator.data["phonebook"].get("entries", [])
            options.extend([f"{entry['name']} ({entry['number']})" for entry in entries])
        return options

    @property
    def current_option(self) -> Optional[str]:
        """Return the current option."""
        return "Select contact..."

    async def async_select_option(self, option: str) -> None:
        """Change the selected option."""
        if option == "Select contact...":
            return
            
        # Extract number from the option string "Name (Number)"
        if "(" in option and ")" in option:
            start = option.rfind("(") + 1
            end = option.rfind(")")
            number = option[start:end]
            
            # Make the call
            await self.coordinator.call_number(number)
            _LOGGER.info("Called %s via phonebook selection", number)

    @property
    def available(self) -> bool:
        """Return if entity is available."""
        # Only available when not in a call
        if "status" in self.coordinator.data:
            call = self.coordinator.data["status"].get("call", {})
            return not call.get("active", False)
        return True
