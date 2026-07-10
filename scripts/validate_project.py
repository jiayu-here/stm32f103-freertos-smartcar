#!/usr/bin/env python3
"""Static consistency checks for the handoff package."""

from __future__ import annotations

import csv
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def require_files() -> None:
    required = [
        "README.md",
        "App/Src/app.c",
        "App/Src/app_config.c",
        "BSP/Src/bsp.c",
        "hardware/BOM.csv",
        "hardware/pinmap.csv",
        "docs/01_硬件接线.md",
        "docs/03_蓝牙串口协议.md",
        "docs/09_操作步骤_从零到运行.md",
    ]
    missing = [name for name in required if not (ROOT / name).is_file()]
    assert not missing, f"missing files: {missing}"


def validate_utf8() -> None:
    for path in [ROOT / "README.md", *(ROOT / "docs").glob("*.md")]:
        text = path.read_text(encoding="utf-8")
        assert "�" not in text, f"replacement character in {path.name}"


def validate_pinmap() -> None:
    used: dict[str, str] = {}
    with (ROOT / "hardware/pinmap.csv").open(encoding="utf-8-sig", newline="") as stream:
        for row in csv.DictReader(stream):
            pin = row["STM32引脚"]
            owner = f'{row["模块"]}/{row["信号"]}'
            assert pin not in used, f"pin conflict: {pin}: {used[pin]} and {owner}"
            used[pin] = owner
    assert len(used) == 29, f"unexpected pin count: {len(used)}"


def validate_commands() -> None:
    protocol = (ROOT / "App/Src/protocol.c").read_text(encoding="utf-8")
    docs = (ROOT / "docs/03_蓝牙串口协议.md").read_text(encoding="utf-8")
    commands = [
        "MODE", "DRIVE", "SPEED", "PID", "MOVE", "TURN", "QMOVE", "QTURN",
        "ROUTE", "RUN", "CLEARROUTE", "ODOM", "RESETODOM", "RAMP", "LINEGAIN",
        "WHEEL", "BATTERY", "STALL", "SAVE", "LOAD", "DEFAULTS", "CONFIG",
        "STATUS", "DIAG", "LOG", "VERSION", "STOP", "CLEAR", "CANCEL", "HELP",
    ]
    for command in commands:
        assert f'"{command}"' in protocol, f"parser missing {command}"
        assert command in docs, f"protocol doc missing {command}"


def validate_flash_partition() -> None:
    c8_linker = (ROOT / "linker/STM32F103C8T6.ld").read_text(encoding="utf-8")
    rc_linker = (ROOT / "linker/STM32F103RCT6.ld").read_text(encoding="utf-8")
    config = (ROOT / "App/Src/app_config.c").read_text(encoding="utf-8")
    assert "LENGTH = 63K" in c8_linker and "0x0800FC00" in config
    assert "LENGTH = 254K" in rc_linker and "0x0803F800" in config


def main() -> None:
    require_files()
    validate_utf8()
    validate_pinmap()
    validate_commands()
    validate_flash_partition()
    print("PASS: project structure, UTF-8, pin map, commands, and Flash partition")


if __name__ == "__main__":
    main()
