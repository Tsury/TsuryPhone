from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Optional


@dataclass
class AudioConfig:
    earpieceVolume: int | None = None
    earpieceGain: int | None = None
    speakerVolume: int | None = None
    speakerGain: int | None = None


@dataclass
class StatsTotals:
    total: int = 0
    incoming: int = 0
    outgoing: int = 0
    blocked: int = 0
    talkTimeSeconds: int = 0


@dataclass
class SystemMetrics:
    uptime: int = 0
    rssi: int = 0
    freeHeap: int = 0


@dataclass
class CallState:
    currentCallNumber: Optional[str] = None
    currentDialingNumber: Optional[str] = None
    callStartTs: Optional[int] = None
    isIncoming: Optional[bool] = None
    durationSecondsLocal: int = 0


@dataclass
class TsuryPhoneState:
    device_id: Optional[str] = None
    host: Optional[str] = None
    schema_version: int = 2
    seq_last: int = -1
    reboot_detected: bool = False

    app_state: str = "Unknown"
    ringing: bool = False
    dnd_active: bool = False
    maintenance_mode: bool = False

    audio: AudioConfig = field(default_factory=AudioConfig)
    stats: StatsTotals = field(default_factory=StatsTotals)
    system: SystemMetrics = field(default_factory=SystemMetrics)
    call: CallState = field(default_factory=CallState)

    quick_dials: list[dict[str, Any]] = field(default_factory=list)
    blocked_numbers: list[dict[str, Any]] = field(default_factory=list)
    webhooks: list[dict[str, Any]] = field(default_factory=list)
    ring_pattern: Optional[str] = None

