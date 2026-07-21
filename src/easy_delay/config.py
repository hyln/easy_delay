from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re
import tomllib


EXAMPLE_CONFIG = """[target]
host = "192.168.1.20"
user = "root"
password = "your-password"
port = 49220
ssh_port = 22
host_key_policy = "auto_add"

[measurement]
threshold_ms = 50.0
safety_margin_ms = 10.0
samples = 100
interval_ms = 10
"""


@dataclass(frozen=True)
class Target:
    host: str
    user: str
    password: str | None = None
    port: int = 49220
    ssh_port: int = 22
    host_key_policy: str = "reject"
    remote_directory: str = "/tmp/easy-delay"


@dataclass(frozen=True)
class Measurement:
    threshold_ms: float = 50.0
    safety_margin_ms: float = 10.0
    samples: int = 100
    interval_ms: int = 10


@dataclass(frozen=True)
class Config:
    target: Target
    measurement: Measurement


def load_config(path: Path) -> Config:
    with path.open("rb") as stream:
        raw = tomllib.load(stream)
    target = Target(**raw["target"])
    measurement = Measurement(**raw.get("measurement", {}))
    if not target.host or not target.user:
        raise ValueError("target.host and target.user are required")
    if not re.fullmatch(r"/[A-Za-z0-9_./-]+", target.remote_directory):
        raise ValueError("target.remote_directory must be a safe absolute path")
    if not 1024 <= target.port <= 65535:
        raise ValueError("target.port must be between 1024 and 65535")
    if measurement.samples < 10 or measurement.interval_ms < 1:
        raise ValueError("measurement samples/interval are invalid")
    if not target.password:
        raise ValueError("target.password is required")
    if target.host_key_policy not in {"reject", "auto_add"}:
        raise ValueError("target.host_key_policy must be reject or auto_add")
    return Config(target, measurement)


def create_example_config(path: Path) -> None:
    """Create, but never overwrite, a ready-to-edit configuration file."""
    with path.open("x", encoding="utf-8") as stream:
        stream.write(EXAMPLE_CONFIG)
