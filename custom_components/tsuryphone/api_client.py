from __future__ import annotations

import asyncio
from dataclasses import dataclass
from typing import Any, Mapping, Optional

from aiohttp import ClientError, ClientSession

from .const import LOGGER, ERROR_CODE_TO_TRANSLATION_KEY
from .util import AsyncBoundedSemaphore


class TsuryPhoneError(Exception):
    def __init__(self, error_code: str | None, message: str | None) -> None:
        super().__init__(message or error_code or "Unknown error")
        self.error_code = error_code
        self.message = message or error_code or "Unknown error"


@dataclass
class ApiClientConfig:
    host: str
    port: int = 8080
    timeout_s: int = 10


class TsuryPhoneApiClient:
    def __init__(self, session: ClientSession, config: ApiClientConfig, concurrency_limit: int = 4) -> None:
        self._session = session
        self._config = config
        self._base = f"http://{config.host}:{config.port}"
        self._sem = AsyncBoundedSemaphore(concurrency_limit)
        self._timeout = aiohttp_client_timeout(config.timeout_s)

    async def _post_json(self, path: str, payload: Optional[Mapping[str, Any]] = None) -> Mapping[str, Any]:
        url = f"{self._base}{path}"
        async with self._sem.acquire():
            try:
                async with self._session.post(url, json=payload or {}, timeout=self._timeout) as resp:
                    data = await resp.json(content_type=None)
            except ClientError as err:
                LOGGER.error("HTTP error POST %s: %s", url, err)
                raise TsuryPhoneError(None, f"HTTP error: {err}") from err
            return self._normalize_response(data)

    async def _get_json(self, path: str) -> Mapping[str, Any]:
        url = f"{self._base}{path}"
        async with self._sem.acquire():
            try:
                async with self._session.get(url, timeout=self._timeout) as resp:
                    data = await resp.json(content_type=None)
            except ClientError as err:
                LOGGER.error("HTTP error GET %s: %s", url, err)
                raise TsuryPhoneError(None, f"HTTP error: {err}") from err
            return self._normalize_response(data)

    @staticmethod
    def _normalize_response(data: Mapping[str, Any]) -> Mapping[str, Any]:
        if not isinstance(data, dict):
            raise TsuryPhoneError(None, "Invalid response")
        if data.get("success", True) is False:
            error_code = data.get("errorCode")
            message = data.get("message") or ERROR_CODE_TO_TRANSLATION_KEY.get(error_code or "", message)
            raise TsuryPhoneError(error_code, message)
        return data.get("data") or {}

    # Public API methods
    async def get_config(self) -> Mapping[str, Any]:
        return await self._get_json("/api/config/tsuryphone")

    async def refetch_all(self) -> Mapping[str, Any]:
        return await self._get_json("/api/refetch_all")

    async def diagnostics(self) -> Mapping[str, Any]:
        return await self._get_json("/api/diagnostics")

    async def dial(self, number: str) -> None:
        await self._post_json("/api/call/dial", {"number": number})

    async def answer(self) -> None:
        await self._post_json("/api/call/answer")

    async def hangup(self) -> None:
        await self._post_json("/api/call/hangup")

    async def ring(self, pattern: Optional[str] = None) -> None:
        payload: dict[str, Any] = {}
        if pattern is not None:
            payload["pattern"] = pattern
        await self._post_json("/api/system/ring", payload)

    async def switch_call_waiting(self) -> None:
        await self._post_json("/api/call/switch_call_waiting")

    async def set_dnd(self, **fields: Any) -> Mapping[str, Any]:
        return await self._post_json("/api/config/dnd", fields)

    async def set_maintenance(self, enabled: bool) -> None:
        await self._post_json("/api/config/maintenance", {"enabled": enabled})

    async def set_audio(self, **fields: Any) -> Mapping[str, Any]:
        return await self._post_json("/api/config/audio", fields)

    async def set_ring_pattern(self, pattern: str) -> None:
        await self._post_json("/api/config/ring_pattern", {"pattern": pattern})

    async def quick_dial_add(self, code: str, number: str, name: Optional[str] = None) -> Mapping[str, Any]:
        payload: dict[str, Any] = {"code": code, "number": number}
        if name:
            payload["name"] = name
        return await self._post_json("/api/config/quick_dial_add", payload)

    async def quick_dial_remove(self, code: str) -> None:
        await self._post_json("/api/config/quick_dial_remove", {"code": code})

    async def blocked_add(self, number: str, reason: Optional[str] = None) -> Mapping[str, Any]:
        payload: dict[str, Any] = {"number": number}
        if reason:
            payload["reason"] = reason
        return await self._post_json("/api/config/blocked_number_add", payload)

    async def blocked_remove(self, number: str) -> None:
        await self._post_json("/api/config/blocked_number_remove", {"number": number})

    async def dial_quick_dial(self, code: str) -> None:
        await self._post_json("/api/call/dial_quick_dial", {"code": code})

    async def webhook_add(self, code: str, id: Optional[str] = None, action_name: Optional[str] = None) -> Mapping[str, Any]:
        payload: dict[str, Any] = {"code": code}
        if id:
            payload["id"] = id
        if action_name:
            payload["actionName"] = action_name
        return await self._post_json("/api/config/webhook_add", payload)

    async def webhook_remove(self, code: str) -> None:
        await self._post_json("/api/config/webhook_remove", {"code": code})

    async def set_ha_url(self, url: str) -> None:
        await self._post_json("/api/config/ha_url", {"url": url})


def aiohttp_client_timeout(timeout_s: int):
    # Lazy import to avoid hard dep at import time in HA
    from aiohttp import ClientTimeout

    return ClientTimeout(total=timeout_s)

