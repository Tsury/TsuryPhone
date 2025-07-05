"""Services for TsuryPhone integration."""
import logging
import voluptuous as vol

from homeassistant.core import HomeAssistant, ServiceCall
from homeassistant.helpers import config_validation as cv

from .const import DOMAIN

_LOGGER = logging.getLogger(__name__)

SERVICE_CALL_NUMBER = "call_number"
SERVICE_RING_DEVICE = "ring_device"
SERVICE_ADD_PHONEBOOK_ENTRY = "add_phonebook_entry"
SERVICE_REMOVE_PHONEBOOK_ENTRY = "remove_phonebook_entry"
SERVICE_ADD_SCREENED_NUMBER = "add_screened_number"
SERVICE_REMOVE_SCREENED_NUMBER = "remove_screened_number"

CALL_NUMBER_SCHEMA = vol.Schema({
    vol.Required("device_id"): cv.string,
    vol.Required("number"): cv.string,
})

RING_DEVICE_SCHEMA = vol.Schema({
    vol.Required("device_id"): cv.string,
    vol.Optional("duration", default=5000): vol.All(vol.Coerce(int), vol.Range(min=500, max=30000)),
})

PHONEBOOK_ENTRY_SCHEMA = vol.Schema({
    vol.Required("device_id"): cv.string,
    vol.Required("name"): cv.string,
    vol.Required("number"): cv.string,
})

REMOVE_PHONEBOOK_ENTRY_SCHEMA = vol.Schema({
    vol.Required("device_id"): cv.string,
    vol.Required("name"): cv.string,
})

SCREENED_NUMBER_SCHEMA = vol.Schema({
    vol.Required("device_id"): cv.string,
    vol.Required("number"): cv.string,
})


async def async_setup_services(hass: HomeAssistant) -> None:
    """Set up services for TsuryPhone integration."""

    async def handle_call_number(call: ServiceCall) -> None:
        """Handle call number service."""
        device_id = call.data.get("device_id")
        number = call.data.get("number")
        
        # Find coordinator for this device
        coordinator = None
        for entry_id, coord in hass.data[DOMAIN].items():
            if hasattr(coord, "base_url") and device_id in coord.base_url:
                coordinator = coord
                break
                
        if coordinator:
            await coordinator.call_number(number)
            _LOGGER.info("Called %s on device %s", number, device_id)
        else:
            _LOGGER.error("Device %s not found", device_id)

    async def handle_ring_device(call: ServiceCall) -> None:
        """Handle ring device service."""
        device_id = call.data.get("device_id")
        duration = call.data.get("duration", 5000)
        
        # Find coordinator for this device
        coordinator = None
        for entry_id, coord in hass.data[DOMAIN].items():
            if hasattr(coord, "base_url") and device_id in coord.base_url:
                coordinator = coord
                break
                
        if coordinator:
            await coordinator.ring_device(duration)
            _LOGGER.info("Rang device %s for %d ms", device_id, duration)
        else:
            _LOGGER.error("Device %s not found", device_id)

    async def handle_add_phonebook_entry(call: ServiceCall) -> None:
        """Handle add phonebook entry service."""
        device_id = call.data.get("device_id")
        name = call.data.get("name")
        number = call.data.get("number")
        
        # Find coordinator for this device
        coordinator = None
        for entry_id, coord in hass.data[DOMAIN].items():
            if hasattr(coord, "base_url") and device_id in coord.base_url:
                coordinator = coord
                break
                
        if coordinator:
            await coordinator.add_phonebook_entry(name, number)
            _LOGGER.info("Added phonebook entry %s: %s on device %s", name, number, device_id)
        else:
            _LOGGER.error("Device %s not found", device_id)

    async def handle_remove_phonebook_entry(call: ServiceCall) -> None:
        """Handle remove phonebook entry service."""
        device_id = call.data.get("device_id")
        name = call.data.get("name")
        
        # Find coordinator for this device
        coordinator = None
        for entry_id, coord in hass.data[DOMAIN].items():
            if hasattr(coord, "base_url") and device_id in coord.base_url:
                coordinator = coord
                break
                
        if coordinator:
            await coordinator.remove_phonebook_entry(name)
            _LOGGER.info("Removed phonebook entry %s on device %s", name, device_id)
        else:
            _LOGGER.error("Device %s not found", device_id)

    async def handle_add_screened_number(call: ServiceCall) -> None:
        """Handle add screened number service."""
        device_id = call.data.get("device_id")
        number = call.data.get("number")
        
        # Find coordinator for this device
        coordinator = None
        for entry_id, coord in hass.data[DOMAIN].items():
            if hasattr(coord, "base_url") and device_id in coord.base_url:
                coordinator = coord
                break
                
        if coordinator:
            await coordinator.add_screened_number(number)
            _LOGGER.info("Added screened number %s on device %s", number, device_id)
        else:
            _LOGGER.error("Device %s not found", device_id)

    async def handle_remove_screened_number(call: ServiceCall) -> None:
        """Handle remove screened number service."""
        device_id = call.data.get("device_id")
        number = call.data.get("number")
        
        # Find coordinator for this device
        coordinator = None
        for entry_id, coord in hass.data[DOMAIN].items():
            if hasattr(coord, "base_url") and device_id in coord.base_url:
                coordinator = coord
                break
                
        if coordinator:
            await coordinator.remove_screened_number(number)
            _LOGGER.info("Removed screened number %s on device %s", number, device_id)
        else:
            _LOGGER.error("Device %s not found", device_id)

    # Register services
    hass.services.async_register(
        DOMAIN, SERVICE_CALL_NUMBER, handle_call_number, schema=CALL_NUMBER_SCHEMA
    )
    
    hass.services.async_register(
        DOMAIN, SERVICE_RING_DEVICE, handle_ring_device, schema=RING_DEVICE_SCHEMA
    )
    
    hass.services.async_register(
        DOMAIN, SERVICE_ADD_PHONEBOOK_ENTRY, handle_add_phonebook_entry, schema=PHONEBOOK_ENTRY_SCHEMA
    )
    
    hass.services.async_register(
        DOMAIN, SERVICE_REMOVE_PHONEBOOK_ENTRY, handle_remove_phonebook_entry, schema=REMOVE_PHONEBOOK_ENTRY_SCHEMA
    )
    
    hass.services.async_register(
        DOMAIN, SERVICE_ADD_SCREENED_NUMBER, handle_add_screened_number, schema=SCREENED_NUMBER_SCHEMA
    )
    
    hass.services.async_register(
        DOMAIN, SERVICE_REMOVE_SCREENED_NUMBER, handle_remove_screened_number, schema=SCREENED_NUMBER_SCHEMA
    )


async def async_unload_services(hass: HomeAssistant) -> None:
    """Unload services for TsuryPhone integration."""
    hass.services.async_remove(DOMAIN, SERVICE_CALL_NUMBER)
    hass.services.async_remove(DOMAIN, SERVICE_RING_DEVICE)
    hass.services.async_remove(DOMAIN, SERVICE_ADD_PHONEBOOK_ENTRY)
    hass.services.async_remove(DOMAIN, SERVICE_REMOVE_PHONEBOOK_ENTRY)
    hass.services.async_remove(DOMAIN, SERVICE_ADD_SCREENED_NUMBER)
    hass.services.async_remove(DOMAIN, SERVICE_REMOVE_SCREENED_NUMBER)
