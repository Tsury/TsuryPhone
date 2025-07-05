"""Button platform for TsuryPhone."""
import logging
from typing import Any

from homeassistant.components.button import ButtonEntity
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
    """Set up TsuryPhone button based on a config entry."""
    coordinator = hass.data[DOMAIN][config_entry.entry_id]

    entities = [
        TsuryPhoneHangupButton(coordinator),
        TsuryPhoneResetButton(coordinator),
        TsuryPhoneRebootButton(coordinator),
        TsuryPhoneRingButton(coordinator),
    ]

    async_add_entities(entities)


class TsuryPhoneBaseButton(CoordinatorEntity, ButtonEntity):
    """Base class for TsuryPhone buttons."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator, button_type: str) -> None:
        """Initialize the button."""
        super().__init__(coordinator)
        self._button_type = button_type
        self._attr_unique_id = f"{coordinator.base_url}_{button_type}"
        self._attr_device_info = {
            "identifiers": {(DOMAIN, coordinator.base_url)},
            "name": "TsuryPhone",
            "manufacturer": MANUFACTURER,
            "model": MODEL,
            "configuration_url": coordinator.base_url,
        }


class TsuryPhoneHangupButton(TsuryPhoneBaseButton):
    """Button to hang up current call."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator) -> None:
        """Initialize the button."""
        super().__init__(coordinator, "hangup")
        self._attr_name = "TsuryPhone Hangup"
        self._attr_icon = "mdi:phone-hangup"

    async def async_press(self) -> None:
        """Handle the button press."""
        await self.coordinator.hangup()
        await self.coordinator.async_request_refresh()

    @property
    def available(self) -> bool:
        """Return if button is available."""
        if "status" in self.coordinator.data:
            call = self.coordinator.data["status"].get("call", {})
            return call.get("active", False)
        return False


class TsuryPhoneResetButton(TsuryPhoneBaseButton):
    """Button to reset the device."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator) -> None:
        """Initialize the button."""
        super().__init__(coordinator, "reset")
        self._attr_name = "TsuryPhone Reset"
        self._attr_icon = "mdi:restart"

    async def async_press(self) -> None:
        """Handle the button press."""
        await self.coordinator.reset_device()


class TsuryPhoneRebootButton(TsuryPhoneBaseButton):
    """Button to reboot the device."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator) -> None:
        """Initialize the button."""
        super().__init__(coordinator, "reboot")
        self._attr_name = "TsuryPhone Reboot"
        self._attr_icon = "mdi:power-cycle"

    async def async_press(self) -> None:
        """Handle the button press."""
        await self.coordinator.reboot_device()


class TsuryPhoneRingButton(TsuryPhoneBaseButton):
    """Button to ring the device for 5 seconds."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator) -> None:
        """Initialize the button."""
        super().__init__(coordinator, "ring")
        self._attr_name = "TsuryPhone Ring"
        self._attr_icon = "mdi:bell-ring"

    async def async_press(self) -> None:
        """Handle the button press."""
        # Ring for 5 seconds (5000ms) when button is pressed
        await self.coordinator.ring_device(5000)
        await self.coordinator.async_request_refresh()
