"""Unreal Editor Python entry point for the Opus 5.5 content build.

    UnrealEditor-Cmd.exe Unreal_Minecraft.uproject -run=pythonscript -script=Tools/content/build_content.py

The heavy lifting lives in C++ (UMCEditorLibrary, editor module) so the material graphs
and asset settings are type-checked; this script sequences the steps, imports nothing
itself, and prints the verification report.
"""
import unreal

lib = unreal.MCEditorLibrary
report = lib.build_all_content()
for line in report.splitlines():
    unreal.log_warning("OPUS55_CONTENT " + line)
for probe in ("/Game/Opus55Minecraft/Mobs/player/SM_player__head.SM_player__head",
              "/Game/Opus55Minecraft/Mobs/creeper/SM_creeper__body.SM_creeper__body"):
    unreal.log_warning("OPUS55_PROBE " + lib.probe_mesh_colors(probe))
