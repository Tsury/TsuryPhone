from __future__ import annotations

import asyncio
from contextlib import asynccontextmanager
from typing import AsyncIterator


class AsyncBoundedSemaphore:
    def __init__(self, limit: int) -> None:
        self._sem = asyncio.BoundedSemaphore(limit)

    @asynccontextmanager
    async def acquire(self) -> AsyncIterator[None]:
        await self._sem.acquire()
        try:
            yield
        finally:
            self._sem.release()


def mask_number(number: str) -> str:
    if not number or len(number) <= 4:
        return number
    return f"{'*' * (len(number) - 4)}{number[-4:]}"

