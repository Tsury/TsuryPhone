from __future__ import annotations

import asyncio
import time
from typing import Any, Mapping, Optional

from aiohttp import ClientSession
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.update_coordinator import DataUpdateCoordinator

from .api_client import ApiClientConfig, TsuryPhoneApiClient
from .const import (
    DOMAIN,
    FALLBACK_POLL_INTERVAL_S,
    LOGGER,
    REFETCH_INTERVAL_S,
)
from .model import TsuryPhoneState
from .storage_cache import StorageCache
from .websocket import TsuryPhoneWebSocket, WebSocketConfig


class TsuryPhoneCoordinator(DataUpdateCoordinator[TsuryPhoneState]):
    def __init__(self, hass: HomeAssistant, entry_id: str, host: str) -> None:
        super().__init__(hass, LOGGER, name=f"{DOMAIN} coordinator", update_interval=None)
        self._entry_id = entry_id
        self._host = host
        self._session: ClientSession = hass.helpers.aiohttp_client.async_get_clientsession()
        self.api = TsuryPhoneApiClient(self._session, ApiClientConfig(host))
        self.state = TsuryPhoneState(host=host)
        self.cache = StorageCache(hass, entry_id)

        self._ws = TsuryPhoneWebSocket(self._session, WebSocketConfig(host), self._on_event)
        self._poll_task: Optional[asyncio.Task] = None
        self._call_timer_task: Optional[asyncio.Task] = None
        self._last_refetch_monotonic: float = 0

    async def async_start(self) -> None:
        cached = await self.cache.load()
        if cached:
            self.state = cached
            self.async_set_updated_data(self.state)
        await self._ws.start()
        self._poll_task = asyncio.create_task(self._fallback_poll_loop())

    async def async_stop(self) -> None:
        await self._ws.stop()
        if self._poll_task:
            self._poll_task.cancel()
        if self._call_timer_task:
            self._call_timer_task.cancel()
        await self.cache.save(self.state)

    async def _fallback_poll_loop(self) -> None:
        try:
            while True:
                await asyncio.sleep(FALLBACK_POLL_INTERVAL_S)
                # Optional: implement polling snapshot/status if WS down
        except asyncio.CancelledError:
            return

    @callback
    def _on_event(self, event: dict[str, Any]) -> None:
        if not isinstance(event, dict):
            return
        if event.get("schemaVersion") != 2:
            LOGGER.warning("Schema version mismatch: %s", event.get("schemaVersion"))
        seq = int(event.get("seq", -1))
        if self.state.seq_last != -1 and seq < self.state.seq_last:
            self.state.reboot_detected = True
            # schedule refetch
            self._maybe_schedule_refetch()
        self.state.seq_last = max(self.state.seq_last, seq)

        category = event.get("category")
        subevent = event.get("event")
        if category == "phone_state":
            self._handle_phone_state_event(subevent, event)
        elif category == "call":
            self._handle_call_event(subevent, event)
        elif category == "system":
            self._handle_system_event(subevent, event)
        elif category == "config":
            self._handle_config_event(subevent, event)

        self.async_set_updated_data(self.state)

    def _handle_phone_state_event(self, sub: str, ev: Mapping[str, Any]) -> None:
        if sub == "state":
            state_name = ev.get("stateName") or ev.get("state")
            self.state.app_state = str(state_name)
        elif sub == "dialing":
            self.state.call.currentDialingNumber = ev.get("currentDialingNumber")
        elif sub == "ring":
            self.state.ringing = bool(ev.get("isRinging"))
        elif sub == "dnd":
            self.state.dnd_active = bool(ev.get("dndActive"))
        elif sub == "call_info":
            self.state.call.currentCallNumber = ev.get("currentCallNumber")

    def _handle_call_event(self, sub: str, ev: Mapping[str, Any]) -> None:
        if sub == "start":
            self.state.call.currentCallNumber = ev.get("number")
            self.state.call.isIncoming = bool(ev.get("isIncoming"))
            self.state.call.callStartTs = int(ev.get("callStartTs", 0))
            self._start_call_timer()
        elif sub == "end":
            # Commit final; for now just stop timer
            self._stop_call_timer()
            self.state.call.currentCallNumber = None
            self.state.call.callStartTs = None
            self.state.call.isIncoming = None
            self.state.call.durationSecondsLocal = 0
        elif sub == "blocked":
            # stats increment handled by system stats usually; no-op here
            pass

    def _handle_system_event(self, sub: str, ev: Mapping[str, Any]) -> None:
        if sub == "stats":
            stats = ev.get("stats", {})
            totals = stats.get("calls", {}).get("totals", {})
            self.state.stats.total = int(totals.get("total", self.state.stats.total))
            self.state.stats.incoming = int(totals.get("incoming", self.state.stats.incoming))
            self.state.stats.outgoing = int(totals.get("outgoing", self.state.stats.outgoing))
            self.state.stats.blocked = int(totals.get("blocked", self.state.stats.blocked))
            self.state.stats.talkTimeSeconds = int(totals.get("talkTimeSeconds", self.state.stats.talkTimeSeconds))
        elif sub == "status":
            self.state.system.freeHeap = int(ev.get("freeHeap", self.state.system.freeHeap))
            self.state.system.rssi = int(ev.get("rssi", self.state.system.rssi))
            self.state.system.uptime = int(ev.get("uptime", self.state.system.uptime))

    def _handle_config_event(self, sub: str, ev: Mapping[str, Any]) -> None:
        if sub != "config_delta":
            return
        if "key" in ev:
            self._apply_config_delta(ev.get("key"), ev.get("newValue"))
        elif "changes" in ev:
            for change in ev.get("changes", []):
                self._apply_config_delta(change.get("key"), change.get("newValue"))

    def _apply_config_delta(self, key: Optional[str], new_value: Any) -> None:
        if not key:
            return
        if key.startswith("audio."):
            subkey = key.split(".", 1)[1]
            setattr(self.state.audio, subkey, new_value)
        elif key == "dnd.active":
            self.state.dnd_active = bool(new_value)
        elif key == "maintenance_mode":
            self.state.maintenance_mode = bool(new_value)
        elif key == "ring.pattern":
            self.state.ring_pattern = str(new_value)
        elif key.startswith("quick_dial") or key.startswith("blocked") or key.startswith("webhook"):
            # For simplicity, trigger a refetch to reconcile lists
            self._maybe_schedule_refetch()

    def _start_call_timer(self) -> None:
        if self._call_timer_task and not self._call_timer_task.done():
            return
        self.state.call.durationSecondsLocal = 0
        self._call_timer_task = asyncio.create_task(self._call_timer())

    async def _call_timer(self) -> None:
        try:
            while self.state.call.callStartTs is not None:
                await asyncio.sleep(1)
                self.state.call.durationSecondsLocal += 1
                self.async_set_updated_data(self.state)
        except asyncio.CancelledError:
            return

    def _stop_call_timer(self) -> None:
        if self._call_timer_task:
            self._call_timer_task.cancel()
        self.state.call.durationSecondsLocal = 0

    def _maybe_schedule_refetch(self) -> None:
        now = time.monotonic()
        if now - self._last_refetch_monotonic < 10:
            return
        self._last_refetch_monotonic = now
        asyncio.create_task(self._do_refetch())

    async def _do_refetch(self) -> None:
        try:
            data = await self.api.refetch_all()
            # Best-effort merge: update audio, lists, pattern, stats
            audio = data.get("audio") or {}
            for k, v in audio.items():
                setattr(self.state.audio, k, v)
            self.state.quick_dials = data.get("quickDials") or self.state.quick_dials
            self.state.blocked_numbers = data.get("blockedNumbers") or self.state.blocked_numbers
            self.state.webhooks = data.get("webhooks") or self.state.webhooks
            if "ringPattern" in data:
                self.state.ring_pattern = data.get("ringPattern")
            stats = data.get("stats") or {}
            totals = stats.get("calls", {}).get("totals", {})
            if totals:
                self.state.stats.total = int(totals.get("total", self.state.stats.total))
                self.state.stats.incoming = int(totals.get("incoming", self.state.stats.incoming))
                self.state.stats.outgoing = int(totals.get("outgoing", self.state.stats.outgoing))
                self.state.stats.blocked = int(totals.get("blocked", self.state.stats.blocked))
                self.state.stats.talkTimeSeconds = int(totals.get("talkTimeSeconds", self.state.stats.talkTimeSeconds))
            self.async_set_updated_data(self.state)
        except Exception as err:
            LOGGER.warning("Refetch failed: %s", err)

