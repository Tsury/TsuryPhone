"""Binary sensor platform for TsuryPhone."""
import logging
from typing import Dict, Any

from homeassistant.components.binary_sensor import (
    BinarySensorDeviceClass,
    BinarySensorEntity,
)
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
    """Set up TsuryPhone binary sensor based on a config entry."""
    coordinator = hass.data[DOMAIN][config_entry.entry_id]

    entities = [
        TsuryPhoneCallActiveBinarySensor(coordinator),
        TsuryPhoneDndBinarySensor(coordinator),
        TsuryPhoneWifiConnectedBinarySensor(coordinator),
        TsuryPhoneCallWaitingBinarySensor(coordinator),
    ]

    async_add_entities(entities)


class TsuryPhoneBaseBinarySensor(CoordinatorEntity, BinarySensorEntity):
    """Base class for TsuryPhone binary sensors."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator, sensor_type: str) -> None:
        """Initialize the binary sensor."""
        super().__init__(coordinator)
        self._sensor_type = sensor_type
        self._attr_unique_id = f"{coordinator.base_url}_{sensor_type}"
        self._attr_device_info = {
            "identifiers": {(DOMAIN, coordinator.base_url)},
            "name": "TsuryPhone",
            "manufacturer": MANUFACTURER,
            "model": MODEL,
            "configuration_url": coordinator.base_url,
        }


class TsuryPhoneCallActiveBinarySensor(TsuryPhoneBaseBinarySensor):
    """Binary sensor for active call status."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator) -> None:
        """Initialize the binary sensor."""
        super().__init__(coordinator, "call_active")
        self._attr_name = "TsuryPhone Call Active"
        self._attr_icon = "mdi:phone"

    @property
    def is_on(self) -> bool:
        """Return true if call is active."""
        if "status" in self.coordinator.data:
            call = self.coordinator.data["status"].get("call", {})
            return call.get("active", False)
        return False

    @property
    def extra_state_attributes(self) -> Dict[str, Any]:
        """Return additional state attributes."""
        if "status" not in self.coordinator.data:
            return {}
        
        call = self.coordinator.data["status"].get("call", {})
        if call.get("active"):
            return {
                "call_number": call.get("number"),
                "call_id": call.get("id"),
                "has_call_waiting": call.get("has_waiting", False),
            }
        return {}


class TsuryPhoneDndBinarySensor(TsuryPhoneBaseBinarySensor):
    """Binary sensor for Do Not Disturb status."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator) -> None:
        """Initialize the binary sensor."""
        super().__init__(coordinator, "dnd_active")
        self._attr_name = "TsuryPhone Do Not Disturb"
        self._attr_icon = "mdi:bell-off"

    @property
    def is_on(self) -> bool:
        """Return true if DnD is active."""
        if "status" in self.coordinator.data:
            return self.coordinator.data["status"].get("dnd_enabled", False)
        return False

    @property
    def extra_state_attributes(self) -> Dict[str, Any]:
        """Return additional DnD attributes."""
        if "dnd" not in self.coordinator.data:
            return {}
        
        dnd = self.coordinator.data["dnd"]
        return {
            "config_enabled": dnd.get("enabled", False),
            "currently_active": dnd.get("currently_active", False),
            "start_time": f"{dnd.get('start_hour', 0):02d}:{dnd.get('start_minute', 0):02d}",
            "end_time": f"{dnd.get('end_hour', 0):02d}:{dnd.get('end_minute', 0):02d}",
        }


class TsuryPhoneWifiConnectedBinarySensor(TsuryPhoneBaseBinarySensor):
    """Binary sensor for WiFi connection status."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator) -> None:
        """Initialize the binary sensor."""
        super().__init__(coordinator, "wifi_connected")
        self._attr_name = "TsuryPhone WiFi Connected"
        self._attr_device_class = BinarySensorDeviceClass.CONNECTIVITY

    @property
    def is_on(self) -> bool:
        """Return true if WiFi is connected."""
        if "status" in self.coordinator.data:
            wifi = self.coordinator.data["status"].get("wifi", {})
            return wifi.get("connected", False)
        return False

    @property
    def extra_state_attributes(self) -> Dict[str, Any]:
        """Return additional WiFi attributes."""
        if "status" not in self.coordinator.data:
            return {}
        
        wifi = self.coordinator.data["status"].get("wifi", {})
        return {
            "ip_address": wifi.get("ip"),
            "ssid": wifi.get("ssid"),
            "rssi": wifi.get("rssi"),
        }


class TsuryPhoneCallWaitingBinarySensor(TsuryPhoneBaseBinarySensor):
    """Binary sensor for call waiting status."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator) -> None:
        """Initialize the binary sensor."""
        super().__init__(coordinator, "call_waiting")
        self._attr_name = "TsuryPhone Call Waiting"
        self._attr_icon = "mdi:phone-plus"

    @property
    def is_on(self) -> bool:
        """Return true if there's a call waiting."""
        if "status" in self.coordinator.data:
            call = self.coordinator.data["status"].get("call", {})
            return call.get("has_waiting", False)
        return False

    @property
    def extra_state_attributes(self) -> Dict[str, Any]:
        """Return additional call waiting attributes."""
        if "status" not in self.coordinator.data:
            return {}
        
        call = self.coordinator.data["status"].get("call", {})
        if call.get("has_waiting"):
            return {
                "waiting_call_id": call.get("waiting_id"),
                "active_call_id": call.get("id"),
                "active_call_number": call.get("number"),
            }
        return {}
