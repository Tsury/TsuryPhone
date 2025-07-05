"""Sensor platform for TsuryPhone."""
import logging
from typing import Any, Dict, Optional

from homeassistant.components.sensor import (
    SensorDeviceClass,
    SensorEntity,
    SensorStateClass,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity
from homeassistant.helpers.typing import StateType

from .const import DOMAIN, MANUFACTURER, MODEL
from .coordinator import TsuryPhoneDataUpdateCoordinator

_LOGGER = logging.getLogger(__name__)


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up TsuryPhone sensor based on a config entry."""
    coordinator = hass.data[DOMAIN][config_entry.entry_id]

    entities = [
        TsuryPhoneStateSensor(coordinator),
        TsuryPhoneUptimeSensor(coordinator),
        TsuryPhoneFreeHeapSensor(coordinator),
        TsuryPhoneWifiRSSISensor(coordinator),
        TsuryPhoneTotalCallsSensor(coordinator),
        TsuryPhoneIncomingCallsSensor(coordinator),
        TsuryPhoneOutgoingCallsSensor(coordinator),
        TsuryPhoneResetsSensor(coordinator),
        TsuryPhoneCallNumberSensor(coordinator),
        TsuryPhoneCallIdSensor(coordinator),
        TsuryPhoneCpuFreqSensor(coordinator),
        TsuryPhoneFlashSizeSensor(coordinator),
        TsuryPhoneSketchSizeSensor(coordinator),
    ]

    async_add_entities(entities)


class TsuryPhoneBaseSensor(CoordinatorEntity, SensorEntity):
    """Base class for TsuryPhone sensors."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator, sensor_type: str) -> None:
        """Initialize the sensor."""
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


class TsuryPhoneStateSensor(TsuryPhoneBaseSensor):
    """Sensor for phone state."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator) -> None:
        """Initialize the sensor."""
        super().__init__(coordinator, "state")
        self._attr_name = "TsuryPhone State"
        self._attr_icon = "mdi:phone"

    @property
    def native_value(self) -> StateType:
        """Return the state of the sensor."""
        if "status" in self.coordinator.data:
            return self.coordinator.data["status"].get("state")
        return None

    @property
    def extra_state_attributes(self) -> Dict[str, Any]:
        """Return additional state attributes."""
        if "status" not in self.coordinator.data:
            return {}
        
        status = self.coordinator.data["status"]
        attrs = {
            "previous_state": status.get("previous_state"),
        }
        
        if status.get("call", {}).get("active"):
            call = status["call"]
            attrs.update({
                "call_active": True,
                "call_number": call.get("number"),
                "call_id": call.get("id"),
                "has_call_waiting": call.get("has_waiting", False),
            })
            if call.get("has_waiting"):
                attrs["waiting_call_id"] = call.get("waiting_id")
        else:
            attrs["call_active"] = False
            
        return attrs


class TsuryPhoneUptimeSensor(TsuryPhoneBaseSensor):
    """Sensor for uptime."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator) -> None:
        """Initialize the sensor."""
        super().__init__(coordinator, "uptime")
        self._attr_name = "TsuryPhone Uptime"
        self._attr_icon = "mdi:clock-outline"
        self._attr_device_class = SensorDeviceClass.DURATION
        self._attr_native_unit_of_measurement = "ms"

    @property
    def native_value(self) -> StateType:
        """Return the uptime in milliseconds."""
        if "status" in self.coordinator.data:
            return self.coordinator.data["status"].get("uptime")
        return None


class TsuryPhoneFreeHeapSensor(TsuryPhoneBaseSensor):
    """Sensor for free heap memory."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator) -> None:
        """Initialize the sensor."""
        super().__init__(coordinator, "free_heap")
        self._attr_name = "TsuryPhone Free Heap"
        self._attr_icon = "mdi:memory"
        self._attr_device_class = SensorDeviceClass.DATA_SIZE
        self._attr_native_unit_of_measurement = "B"
        self._attr_state_class = SensorStateClass.MEASUREMENT

    @property
    def native_value(self) -> StateType:
        """Return the free heap in bytes."""
        if "status" in self.coordinator.data:
            return self.coordinator.data["status"].get("free_heap")
        return None


class TsuryPhoneWifiRSSISensor(TsuryPhoneBaseSensor):
    """Sensor for WiFi RSSI."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator) -> None:
        """Initialize the sensor."""
        super().__init__(coordinator, "wifi_rssi")
        self._attr_name = "TsuryPhone WiFi RSSI"
        self._attr_icon = "mdi:wifi"
        self._attr_device_class = SensorDeviceClass.SIGNAL_STRENGTH
        self._attr_native_unit_of_measurement = "dBm"
        self._attr_state_class = SensorStateClass.MEASUREMENT

    @property
    def native_value(self) -> StateType:
        """Return the WiFi RSSI."""
        if "status" in self.coordinator.data:
            wifi = self.coordinator.data["status"].get("wifi", {})
            return wifi.get("rssi")
        return None

    @property
    def extra_state_attributes(self) -> Dict[str, Any]:
        """Return additional WiFi attributes."""
        if "status" not in self.coordinator.data:
            return {}
        
        wifi = self.coordinator.data["status"].get("wifi", {})
        return {
            "connected": wifi.get("connected"),
            "ip_address": wifi.get("ip"),
            "ssid": wifi.get("ssid"),
        }


class TsuryPhoneTotalCallsSensor(TsuryPhoneBaseSensor):
    """Sensor for total calls."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator) -> None:
        """Initialize the sensor."""
        super().__init__(coordinator, "total_calls")
        self._attr_name = "TsuryPhone Total Calls"
        self._attr_icon = "mdi:phone-log"
        self._attr_state_class = SensorStateClass.TOTAL_INCREASING

    @property
    def native_value(self) -> StateType:
        """Return the total calls."""
        if "stats" in self.coordinator.data:
            return self.coordinator.data["stats"].get("total_calls")
        return None


class TsuryPhoneIncomingCallsSensor(TsuryPhoneBaseSensor):
    """Sensor for incoming calls."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator) -> None:
        """Initialize the sensor."""
        super().__init__(coordinator, "incoming_calls")
        self._attr_name = "TsuryPhone Incoming Calls"
        self._attr_icon = "mdi:phone-incoming"
        self._attr_state_class = SensorStateClass.TOTAL_INCREASING

    @property
    def native_value(self) -> StateType:
        """Return the incoming calls."""
        if "stats" in self.coordinator.data:
            return self.coordinator.data["stats"].get("total_incoming_calls")
        return None


class TsuryPhoneOutgoingCallsSensor(TsuryPhoneBaseSensor):
    """Sensor for outgoing calls."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator) -> None:
        """Initialize the sensor."""
        super().__init__(coordinator, "outgoing_calls")
        self._attr_name = "TsuryPhone Outgoing Calls"
        self._attr_icon = "mdi:phone-outgoing"
        self._attr_state_class = SensorStateClass.TOTAL_INCREASING

    @property
    def native_value(self) -> StateType:
        """Return the outgoing calls."""
        if "stats" in self.coordinator.data:
            return self.coordinator.data["stats"].get("total_outgoing_calls")
        return None


class TsuryPhoneResetsSensor(TsuryPhoneBaseSensor):
    """Sensor for device resets."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator) -> None:
        """Initialize the sensor."""
        super().__init__(coordinator, "resets")
        self._attr_name = "TsuryPhone Resets"
        self._attr_icon = "mdi:restart"
        self._attr_state_class = SensorStateClass.TOTAL_INCREASING

    @property
    def native_value(self) -> StateType:
        """Return the total resets."""
        if "stats" in self.coordinator.data:
            return self.coordinator.data["stats"].get("total_resets")
        return None


class TsuryPhoneCallNumberSensor(TsuryPhoneBaseSensor):
    """Sensor for current call number."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator) -> None:
        """Initialize the sensor."""
        super().__init__(coordinator, "call_number")
        self._attr_name = "TsuryPhone Call Number"
        self._attr_icon = "mdi:phone-dial"

    @property
    def native_value(self) -> StateType:
        """Return the current call number."""
        if "status" in self.coordinator.data:
            call = self.coordinator.data["status"].get("call", {})
            if call.get("active"):
                return call.get("number")
        return None


class TsuryPhoneCallIdSensor(TsuryPhoneBaseSensor):
    """Sensor for current call ID."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator) -> None:
        """Initialize the sensor."""
        super().__init__(coordinator, "call_id")
        self._attr_name = "TsuryPhone Call ID"
        self._attr_icon = "mdi:identifier"

    @property
    def native_value(self) -> StateType:
        """Return the current call ID."""
        if "status" in self.coordinator.data:
            call = self.coordinator.data["status"].get("call", {})
            if call.get("active"):
                return call.get("id")
        return None


class TsuryPhoneCpuFreqSensor(TsuryPhoneBaseSensor):
    """Sensor for CPU frequency."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator) -> None:
        """Initialize the sensor."""
        super().__init__(coordinator, "cpu_freq")
        self._attr_name = "TsuryPhone CPU Frequency"
        self._attr_icon = "mdi:chip"
        self._attr_native_unit_of_measurement = "MHz"

    @property
    def native_value(self) -> StateType:
        """Return the CPU frequency."""
        if "stats" in self.coordinator.data:
            return self.coordinator.data["stats"].get("cpu_freq")
        return None


class TsuryPhoneFlashSizeSensor(TsuryPhoneBaseSensor):
    """Sensor for flash size."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator) -> None:
        """Initialize the sensor."""
        super().__init__(coordinator, "flash_size")
        self._attr_name = "TsuryPhone Flash Size"
        self._attr_icon = "mdi:sd"
        self._attr_device_class = SensorDeviceClass.DATA_SIZE
        self._attr_native_unit_of_measurement = "B"

    @property
    def native_value(self) -> StateType:
        """Return the flash size."""
        if "stats" in self.coordinator.data:
            return self.coordinator.data["stats"].get("flash_size")
        return None


class TsuryPhoneSketchSizeSensor(TsuryPhoneBaseSensor):
    """Sensor for sketch size."""

    def __init__(self, coordinator: TsuryPhoneDataUpdateCoordinator) -> None:
        """Initialize the sensor."""
        super().__init__(coordinator, "sketch_size")
        self._attr_name = "TsuryPhone Sketch Size"
        self._attr_icon = "mdi:file-code"
        self._attr_device_class = SensorDeviceClass.DATA_SIZE
        self._attr_native_unit_of_measurement = "B"

    @property
    def native_value(self) -> StateType:
        """Return the sketch size."""
        if "stats" in self.coordinator.data:
            return self.coordinator.data["stats"].get("sketch_size")
        return None
