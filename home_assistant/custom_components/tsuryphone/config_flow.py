"""Config flow for TsuryPhone integration."""
import logging
from typing import Any, Dict, Optional
import voluptuous as vol
import aiohttp
import asyncio

from homeassistant import config_entries
from homeassistant.core import HomeAssistant
from homeassistant.data_entry_flow import FlowResult
from homeassistant.exceptions import HomeAssistantError
import homeassistant.helpers.config_validation as cv

from .const import DOMAIN, CONF_HOST, CONF_PORT, DEFAULT_PORT

_LOGGER = logging.getLogger(__name__)

STEP_USER_DATA_SCHEMA = vol.Schema(
    {
        vol.Required(CONF_HOST): cv.string,
        vol.Required(CONF_PORT, default=DEFAULT_PORT): cv.port,
    }
)


async def validate_input(hass: HomeAssistant, data: Dict[str, Any]) -> Dict[str, Any]:
    """Validate the user input allows us to connect."""
    host = data[CONF_HOST]
    port = data[CONF_PORT]
    
    try:
        async with aiohttp.ClientSession(timeout=aiohttp.ClientTimeout(total=10)) as session:
            url = f"http://{host}:{port}/"
            async with session.get(url) as response:
                response.raise_for_status()
                device_info = await response.json()
                
                # Validate this is actually a TsuryPhone device
                if "device" not in device_info or device_info.get("device", {}).get("model") != "TsuryPhone":
                    raise InvalidDevice
                
                return {
                    "title": device_info.get("device", {}).get("name", "TsuryPhone"),
                    "device_info": device_info["device"]
                }
                
    except asyncio.TimeoutError:
        raise CannotConnect
    except aiohttp.ClientError:
        raise CannotConnect


class ConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
    """Handle a config flow for TsuryPhone."""

    VERSION = 1

    async def async_step_user(
        self, user_input: Optional[Dict[str, Any]] = None
    ) -> FlowResult:
        """Handle the initial step."""
        if user_input is None:
            return self.async_show_form(
                step_id="user", data_schema=STEP_USER_DATA_SCHEMA
            )

        errors = {}

        try:
            info = await validate_input(self.hass, user_input)
        except CannotConnect:
            errors["base"] = "cannot_connect"
        except InvalidDevice:
            errors["base"] = "invalid_device"
        except Exception:  # pylint: disable=broad-except
            _LOGGER.exception("Unexpected exception")
            errors["base"] = "unknown"
        else:
            # Create unique ID based on device info
            device_info = info["device_info"]
            unique_id = device_info.get("mac", f"{user_input[CONF_HOST]}_{user_input[CONF_PORT]}")
            
            await self.async_set_unique_id(unique_id)
            self._abort_if_unique_id_configured()

            return self.async_create_entry(title=info["title"], data=user_input)

        return self.async_show_form(
            step_id="user", data_schema=STEP_USER_DATA_SCHEMA, errors=errors
        )


class CannotConnect(HomeAssistantError):
    """Error to indicate we cannot connect."""


class InvalidDevice(HomeAssistantError):
    """Error to indicate device is not a TsuryPhone."""
