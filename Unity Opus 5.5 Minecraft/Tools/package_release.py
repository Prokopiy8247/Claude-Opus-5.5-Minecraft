"""Create viewer-friendly Windows and macOS release archives.

The macOS ZIP stores Unix executable bits explicitly. This is required when a
Unity macOS player is produced and packaged on Windows.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import zipfile


WINDOWS_README = """КАК ЗАПУСТИТЬ ИГРУ НА WINDOWS

1. Полностью распакуйте ZIP-архив.
2. Дважды щёлкните PLAY-WINDOWS.bat или MinecraftRecreation.exe.
3. Если SmartScreen покажет предупреждение, проверьте, что архив скачан со страницы
   github.com/Prokopiy8247/Claude-Opus-5.5-Minecraft, затем выберите
   «Подробнее» -> «Выполнить в любом случае».

Не выносите EXE из этой папки: рядом с ним нужны папка MinecraftRecreation_Data
и остальные файлы Unity.
"""

MACOS_README = """КАК ЗАПУСТИТЬ ИГРУ НА macOS

1. Полностью распакуйте ZIP-архив.
2. Откройте Minecraft Recreation.app.
3. Сборка не подписана Apple. При первом запуске нажмите приложение правой кнопкой,
   выберите «Открыть», затем ещё раз «Открыть». Если кнопки нет: Системные настройки
   -> Конфиденциальность и безопасность -> Всё равно открыть.

Сборка Universal и подходит для Intel Mac и Apple Silicon (M1/M2/M3/M4 и новее).
Минимальная версия системы: macOS 12.
"""


def zip_info(name: str, mode: int) -> zipfile.ZipInfo:
    info = zipfile.ZipInfo(name.replace(os.sep, "/"))
    info.create_system = 3
    info.external_attr = (mode & 0xFFFF) << 16
    info.compress_type = zipfile.ZIP_DEFLATED
    return info


def add_tree(archive: zipfile.ZipFile, source: Path, archive_root: str, macos: bool) -> None:
    for path in sorted(source.rglob("*")):
        relative = path.relative_to(source).as_posix()
        lowered = relative.lower()
        if "dontship" in lowered or "donotship" in lowered:
            continue
        name = f"{archive_root}/{relative}"
        if path.is_dir():
            archive.writestr(zip_info(name.rstrip("/") + "/", 0o755), b"")
            continue
        executable = macos and (
            "/Contents/MacOS/" in f"/{name}"
            or "/Contents/Frameworks/" in f"/{name}" and path.suffix == ".dylib"
            or "/Contents/PlugIns/" in f"/{name}" and path.suffix in {"", ".bundle"}
        )
        archive.writestr(zip_info(name, 0o755 if executable else 0o644), path.read_bytes())


def package(project: Path, output: Path) -> list[Path]:
    windows = project / "Builds" / "Windows"
    macos = project / "Builds" / "macOS" / "Minecraft Recreation.app"
    if not (windows / "MinecraftRecreation.exe").is_file():
        raise FileNotFoundError("Builds/Windows/MinecraftRecreation.exe is missing")
    if not (macos / "Contents" / "MacOS" / "Minecraft Recreation").is_file():
        raise FileNotFoundError("The macOS .app build is missing")

    output.mkdir(parents=True, exist_ok=True)
    windows_zip = output / "Minecraft-Recreation-Windows-x64.zip"
    macos_zip = output / "Minecraft-Recreation-macOS-Universal.zip"

    with zipfile.ZipFile(windows_zip, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        add_tree(archive, windows, "Minecraft Recreation", macos=False)
        archive.writestr(zip_info("Minecraft Recreation/PLAY-WINDOWS.bat", 0o755),
                         b'@echo off\r\nstart "" "%~dp0MinecraftRecreation.exe"\r\n')
        archive.writestr(zip_info("Minecraft Recreation/START HERE - WINDOWS.txt", 0o644),
                         WINDOWS_README.encode("utf-8-sig"))

    with zipfile.ZipFile(macos_zip, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        add_tree(archive, macos, "Minecraft Recreation.app", macos=True)
        launcher = '#!/bin/zsh\ncd -- "$(dirname -- "$0")"\nopen "Minecraft Recreation.app"\n'
        archive.writestr(zip_info("PLAY ON MAC.command", 0o755), launcher.encode())
        archive.writestr(zip_info("START HERE - macOS.txt", 0o644), MACOS_README.encode("utf-8"))

    return [windows_zip, macos_zip]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--project", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parent / "_out" / "release-assets")
    args = parser.parse_args()
    for path in package(args.project.resolve(), args.output.resolve()):
        print(f"{path.name}: {path.stat().st_size / 1024 / 1024:.1f} MB")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
