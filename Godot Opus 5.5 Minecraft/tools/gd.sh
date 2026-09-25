#!/bin/bash
# Helper wrappers around the installed Godot 4.7.2 console executable.
GODOT="/w/Godot_v4.7.2-stable_win64.exe/Godot_v4.7.2-stable_win64_console.exe"
cd "$(dirname "$0")/.."
case "$1" in
  import) timeout 600 "$GODOT" --headless --path . --import 2>&1 | grep -v -E "^\[|^Godot Engine|^$|\[0m$" ;;
  script) shift; timeout 300 "$GODOT" --headless --path . --script "$@" 2>&1 | grep -v "^Godot Engine" ;;
  run) shift; timeout 600 "$GODOT" --path . "$@" 2>&1 | grep -v "^Godot Engine" ;;
  *) echo "usage: gd.sh import|script <file>|run [args]" ;;
esac
