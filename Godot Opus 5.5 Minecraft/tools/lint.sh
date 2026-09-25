#!/bin/bash
# Compile check of every script with the project's autoloads (Game, Sfx) loaded.
# Prints "file:line: message" for each script error, then the summary line.
cd "$(dirname "$0")/.."
GODOT="/w/Godot_v4.7.2-stable_win64.exe/Godot_v4.7.2-stable_win64_console.exe"
timeout 180 "$GODOT" --headless --path . res://game/tests/lint_scene.tscn 2>&1 | awk '
/SCRIPT ERROR:|^ERROR:/ { msg=$0; sub(/^.*(SCRIPT ERROR|ERROR): /, "", msg); next }
/^   at: / { loc=$0; sub(/^   at: [^(]*\(/, "", loc); sub(/\)$/, "", loc); if (msg != "") { key=loc": "msg; if (!(key in seen)) { seen[key]=1; print key } } msg=""; next }
/^lint2:|^FAILED:|^registries ok/ { print }
' | grep -v "Leaked\|PagedAllocator\|resources still in use\|RID allocations"
