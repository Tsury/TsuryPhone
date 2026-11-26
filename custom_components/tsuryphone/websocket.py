from __future__ import annotations

import asyncio
import json
from dataclasses import dataclass
from typing import Any, AsyncIterator, Callable, Optional

from aiohttp import ClientSession, ClientWebSocketResponse, WSMsgType

from .const import LOGGER, EVENT_QUEUE_MAX


EventCallback = Callable[[dict[str, Any]], None]


@dataclass
class WebSocketConfig:
    host: str
    port: int = 8080
    path: str = "/ws"
    backoff_initial_s: float = 1.0
    backoff_max_s: float = 60.0


class TsuryPhoneWebSocket:
    def __init__(self, session: ClientSession, config: WebSocketConfig, on_event: EventCallback) -> None:
        self._session = session
        self._config = config
        self._on_event = on_event
        self._running = False
        self._queue: asyncio.Queue[dict[str, Any]] = asyncio.Queue(maxsize=EVENT_QUEUE_MAX)
        self._consumer_task: Optional[asyncio.Task] = None

    async def start(self) -> None:
        if self._running:
            return
        self._running = True
        self._consumer_task = asyncio.create_task(self._consume())
        asyncio.create_task(self._run_forever())

    async def stop(self) -> None:
        self._running = False
        if self._consumer_task:
            self._consumer_task.cancel()

    async def _run_forever(self) -> None:
        backoff = self._config.backoff_initial_s
        url = f"ws://{self._config.host}:{self._config.port}{self._config.path}"
        while self._running:
            try:
                async with self._session.ws_connect(url) as ws:
                    LOGGER.debug("WS connected: %s", url)
                    backoff = self._config.backoff_initial_s
                    await self._recv_loop(ws)
            except Exception as err:  # broad: network errors
                LOGGER.warning("WS error: %s", err)
            if not self._running:
                break
            await asyncio.sleep(backoff)
            backoff = min(backoff * 2, self._config.backoff_max_s)

    async def _recv_loop(self, ws: ClientWebSocketResponse) -> None:
        async for msg in ws:
            if msg.type == WSMsgType.TEXT:
                try:
                    data = json.loads(msg.data)
                    if not isinstance(data, dict):
                        continue
                    if self._queue.full():
                        # Drop oldest by getting once
                        _ = self._queue.get_nowait()
                    self._queue.put_nowait(data)
                except Exception as err:  # invalid JSON or queue
                    LOGGER.debug("WS parse error: %s", err)
            elif msg.type in (WSMsgType.CLOSE, WSMsgType.CLOSING, WSMsgType.CLOSED):
                break
            elif msg.type == WSMsgType.ERROR:
                break

    async def _consume(self) -> None:
        try:
            while self._running:
                event = await self._queue.get()
                try:
                    self._on_event(event)
                except Exception as err:
                    LOGGER.exception("Event callback error: %s", err)
        except asyncio.CancelledError:
            return

