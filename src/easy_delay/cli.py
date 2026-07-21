from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

from .config import create_example_config, load_config
from .orchestrator import measure


def main() -> None:
    parser = argparse.ArgumentParser(description="Measure clock offset to a configured Linux host")
    parser.add_argument(
        "--config",
        type=Path,
        default=Path("easy-delay.toml"),
        metavar="PATH",
        help="目标主机和测量参数的 TOML 配置文件（默认：./easy-delay.toml）",
    )
    arguments = parser.parse_args()

    # Stage 1: Bootstrap a missing configuration without initiating a connection.
    if not arguments.config.exists():
        try:
            create_example_config(arguments.config)
        except OSError as error:
            print(json.dumps({"result": "error", "error": str(error)}, ensure_ascii=False))
            raise SystemExit(2) from error
        print(json.dumps({
            "result": "config_created",
            "config": str(arguments.config),
            "message": "请编辑配置文件后重新运行",
        }, ensure_ascii=False, indent=2))
        raise SystemExit(0)

    # Stage 2: Load the explicit target and execute the measurement pipeline.
    try:
        report = measure(load_config(arguments.config))
    except (OSError, TypeError, ValueError, RuntimeError) as error:
        print(json.dumps({"result": "error", "error": str(error)}, ensure_ascii=False))
        raise SystemExit(2) from error
    print(json.dumps(report, ensure_ascii=False, indent=2))
    raise SystemExit(0 if report["result"] == "pass" else 1)
