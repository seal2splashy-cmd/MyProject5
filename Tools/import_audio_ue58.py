"""
import_audio_ue58.py - run INSIDE the Unreal Editor (Tools > Execute Python Script...), any map open.

Imports every Assets/Audio/*.wav that isn't in /Game/Generated/Audio yet (new sounds after a git pull)
and makes every *_Loop wave loop. Doesn't touch maps, materials or sounds you already have, unlike
Tools/ue_setup_project.py (which also rebuilds TestArena). Safe to run any time.
"""

import os
import unreal

AUDIO_DST = "/Game/Generated/Audio"
eal = unreal.EditorAssetLibrary


def main():
    try:
        root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    except NameError:
        root = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    src = os.path.join(root, "Assets", "Audio")
    if not os.path.isdir(src):
        unreal.log_error("[Audio import] %s not found" % src)
        return
    if not eal.does_directory_exist(AUDIO_DST):
        eal.make_directory(AUDIO_DST)
    tasks = []
    for name in sorted(os.listdir(src)):
        stem, ext = os.path.splitext(name)
        if ext.lower() != ".wav" or eal.does_asset_exist(AUDIO_DST + "/" + stem):
            continue
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", os.path.join(src, name))
        t.set_editor_property("destination_path", AUDIO_DST)
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", True)
        tasks.append(t)
    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    loops = 0
    for path in eal.list_assets(AUDIO_DST, recursive=False, include_folder=False):
        stem = path.split(".")[0].split("/")[-1]
        if stem.endswith("_Loop"):
            wave = eal.load_asset(path.split(".")[0])
            if wave and not wave.get_editor_property("looping"):
                wave.set_editor_property("looping", True)
                eal.save_loaded_asset(wave)
                loops += 1
    unreal.log("[Audio import] imported %d new sounds into %s, set %d loops" % (len(tasks), AUDIO_DST, loops))


main()
