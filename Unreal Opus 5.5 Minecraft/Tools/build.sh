#!/usr/bin/env bash
# Builds the Windows editor (default) or game target with UnrealBuildTool.
# Usage: Tools/build.sh [Editor|Game] [logname]

PROJ_DIR="$(cd "$(dirname "$0")/.." && pwd)"
UE="${UE_5_8_ROOT:-${UE_ROOT:-}}"

if [ -z "$UE" ] || [ ! -f "$UE/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" ]; then
  echo "Set UE_5_8_ROOT to your Unreal Engine 5.8 installation directory." >&2
  exit 2
fi

TARGET="Unreal_MinecraftEditor"
[ "${1:-Editor}" == "Game" ] && TARGET="Unreal_Minecraft"
LOG="${2:-build}"
mkdir -p "$PROJ_DIR/.opus55-run/logs"

VSLANG=1033 "$UE/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" "$TARGET" Win64 Development \
  "-Project=$PROJ_DIR/Unreal_Minecraft.uproject" -WaitMutex -NoHotReloadFromIDE -FromMsBuild \
  > "$PROJ_DIR/.opus55-run/logs/$LOG.log" 2>&1
RC=$?

echo "exit=$RC"
iconv -f CP866 -t UTF-8 "$PROJ_DIR/.opus55-run/logs/$LOG.log" > "$PROJ_DIR/.opus55-run/logs/$LOG.utf8.log" 2>/dev/null || \
  cp "$PROJ_DIR/.opus55-run/logs/$LOG.log" "$PROJ_DIR/.opus55-run/logs/$LOG.utf8.log"
grep -a -E "error [A-Z]+[0-9]+|fatal error|: error|: Error" "$PROJ_DIR/.opus55-run/logs/$LOG.utf8.log" | \
  sed 's/^.*Source[\/]Unreal_Minecraft/.../' | sort -u | head -${3:-60}
tail -3 "$PROJ_DIR/.opus55-run/logs/$LOG.log"
exit $RC
