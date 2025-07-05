"""Number platform for TsuryPhone."""
import logging
from typing import Any

from homeassistant.components.number import NumberEntity
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
    """Set up TsuryPhone number based on a config entry."""
    coordinator = hass.data[DOMAIN][config_entry.entry_id]

    entities = [
        TsuryPhoneDndStartHourNumber(coordinator),
        TsuryPhoneDndStartMinuteNumber(coordinator),
        TsuryPhoneDndEndHourNumber(coordinator),
        TsuryPhoneDndEndMinuteNumber(coordinator),
    ]

    async_add_entities(entities)


class TsuryPhoneBaseNumber(CoordinatorEntity, NumberEntity):
    """Base class for TsuryPhone numbers."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator, number_type: str) -> None:
        """Initialize the number."""
        super().__init__(coordinator)
        self._number_type = number_type
        self._attr_unique_id = f"{coordinator.base_url}_{number_type}"
        self._attr_device_info = {
            "identifiers": {(DOMAIN, coordinator.base_url)},
            "name": "TsuryPhone",
            "manufacturer": MANUFACTURER,
            "model": MODEL,
            "configuration_url": coordinator.base_url,
        }


class TsuryPhoneDndStartHourNumber(TsuryPhoneBaseNumber):
    """Number entity for DnD start hour."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator) -> None:
        """Initialize the number."""
        super().__init__(coordinator, "dnd_start_hour")
        self._attr_name = "TsuryPhone DnD Start Hour"
        self._attr_icon = "mdi:clock-start"
        self._attr_native_min_value = 0
        self._attr_native_max_value = 23
        self._attr_native_step = 1

    @property
    def native_value(self) -> float:
        """Return the current value."""
        if "dnd" in self.coordinator.data:
            return self.coordinator.data["dnd"].get("start_hour", 0)
        return 0

    async def async_set_native_value(self, value: float) -> None:
        """Set the value."""
        if "dnd" in self.coordinator.data:
            dnd = self.coordinator.data["dnd"]
            await self.coordinator.set_dnd_hours(
                int(value),
                dnd.get("start_minute", 0),
                dnd.get("end_hour", 0),
                dnd.get("end_minute", 0)
            )
            await self.coordinator.async_request_refresh()


class TsuryPhoneDndStartMinuteNumber(TsuryPhoneBaseNumber):
    """Number entity for DnD start minute."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator) -> None:
        """Initialize the number."""
        super().__init__(coordinator, "dnd_start_minute")
        self._attr_name = "TsuryPhone DnD Start Minute"
        self._attr_icon = "mdi:clock-start"
        self._attr_native_min_value = 0
        self._attr_native_max_value = 59
        self._attr_native_step = 1

    @property
    def native_value(self) -> float:
        """Return the current value."""
        if "dnd" in self.coordinator.data:
            return self.coordinator.data["dnd"].get("start_minute", 0)
        return 0

    async def async_set_native_value(self, value: float) -> None:
        """Set the value."""
        if "dnd" in self.coordinator.data:
            dnd = self.coordinator.data["dnd"]
            await self.coordinator.set_dnd_hours(
                dnd.get("start_hour", 0),
                int(value),
                dnd.get("end_hour", 0),
                dnd.get("end_minute", 0)
            )
            await self.coordinator.async_request_refresh()


class TsuryPhoneDndEndHourNumber(TsuryPhoneBaseNumber):
    """Number entity for DnD end hour."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator) -> None:
        """Initialize the number."""
        super().__init__(coordinator, "dnd_end_hour")
        self._attr_name = "TsuryPhone DnD End Hour"
        self._attr_icon = "mdi:clock-end"
        self._attr_native_min_value = 0
        self._attr_native_max_value = 23
        self._attr_native_step = 1

    @property
    def native_value(self) -> float:
        """Return the current value."""
        if "dnd" in self.coordinator.data:
            return self.coordinator.data["dnd"].get("end_hour", 0)
        return 0

    async def async_set_native_value(self, value: float) -> None:
        """Set the value."""
        if "dnd" in self.coordinator.data:
            dnd = self.coordinator.data["dnd"]
            await self.coordinator.set_dnd_hours(
                dnd.get("start_hour", 0),
                dnd.get("start_minute", 0),
                int(value),
                dnd.get("end_minute", 0)
            )
            await self.coordinator.async_request_refresh()


class TsuryPhoneDndEndMinuteNumber(TsuryPhoneBaseNumber):
    """Number entity for DnD end minute."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator) -> None:
        """Initialize the number."""
        super().__init__(coordinator, "dnd_end_minute")
        self._attr_name = "TsuryPhone DnD End Minute"
        self._attr_icon = "mdi:clock-end"
        self._attr_native_min_value = 0
        self._attr_native_max_value = 59
        self._attr_native_step = 1

    @property
    def native_value(self) -> float:
        """Return the current value."""
        if "dnd" in self.coordinator.data:
            return self.coordinator.data["dnd"].get("end_minute", 0)
        return 0

    async def async_set_native_value(self, value: float) -> None:
        """Set the value."""
        if "dnd" in self.coordinator.data:
            dnd = self.coordinator.data["dnd"]
            await self.coordinator.set_dnd_hours(
                dnd.get("start_hour", 0),
                dnd.get("start_minute", 0),
                dnd.get("end_hour", 0),
                int(value)
            )
            await self.coordinator.async_request_refresh()
