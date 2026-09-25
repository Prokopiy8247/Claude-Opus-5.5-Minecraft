#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
PROJECT_FILE="$PROJECT_ROOT/Unreal_Minecraft.uproject"

die() {
  printf '\nError: %s\n' "$1" >&2
  printf 'Press Return to close this window.\n' >&2
  read -r _ || true
  exit 1
}

is_ue_58() {
  local root="$1"
  local editor="$root/Engine/Binaries/Mac/UnrealEditor.app"
  local version="$root/Engine/Build/Build.version"
  [[ -d "$editor" && -f "$version" ]] || return 1
  grep -Eq '"MajorVersion"[[:space:]]*:[[:space:]]*5' "$version" &&
    grep -Eq '"MinorVersion"[[:space:]]*:[[:space:]]*8' "$version"
}

find_unreal_root() {
  local candidate
  for candidate in \
    "${UE_5_8_ROOT:-}" \
    "${UE_ROOT:-}" \
    "/Users/Shared/Epic Games/UE_5.8" \
    "/Applications/Epic Games/UE_5.8"; do
    if [[ -n "$candidate" ]] && is_ue_58 "$candidate"; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  for candidate in "/Users/Shared/Epic Games"/UE_5.8* "/Applications/Epic Games"/UE_5.8*; do
    if [[ -e "$candidate" ]] && is_ue_58 "$candidate"; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  return 1
}

[[ -f "$PROJECT_FILE" ]] || die "Project file not found: $PROJECT_FILE"
command -v xcode-select >/dev/null 2>&1 || die "xcode-select is unavailable. Install Xcode from the App Store."
xcode-select -p >/dev/null 2>&1 || die "Xcode tools are not selected. Run: xcode-select --install"

UNREAL_ROOT="$(find_unreal_root)" || die "Unreal Engine 5.8 was not found. Install it with Epic Games Launcher, or set UE_5_8_ROOT to the engine folder."
BUILD_SCRIPT="$UNREAL_ROOT/Engine/Build/BatchFiles/Mac/Build.sh"
EDITOR_APP="$UNREAL_ROOT/Engine/Binaries/Mac/UnrealEditor.app"

[[ -x "$BUILD_SCRIPT" ]] || die "Unreal build script not found: $BUILD_SCRIPT"

printf 'Project: %s\n' "$PROJECT_FILE"
printf 'Unreal Engine: %s\n\n' "$UNREAL_ROOT"
printf 'Building the editor target. The first build can take a while...\n'

if ! "$BUILD_SCRIPT" Unreal_MinecraftEditor Mac Development "-Project=$PROJECT_FILE" -WaitMutex -NoHotReloadFromIDE; then
  die "The C++ build failed. Install a version of Xcode supported by Unreal Engine 5.8, open Xcode once to accept its license, then try again."
fi

printf '\nStarting Unreal Editor...\n'
open "$EDITOR_APP" --args "$PROJECT_FILE"
