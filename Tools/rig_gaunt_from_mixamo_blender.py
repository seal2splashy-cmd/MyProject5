"""Merge the Gaunt's Mixamo downloads into ONE FBX that Unreal imports in one go.

Mixamo gives you one FBX per animation. Imported into Unreal one by one, each would bring its
own copy of the skeleton. This script puts the rigged character and all the clips on a single
armature, names each clip, and exports Assets/Anomaly/Gaunt/Rigged/SK_AH_Gaunt.fbx.
Tools/import_gaunt_ue58.py then imports it as SK_AH_Gaunt + A_Gaunt_<Clip>.

Put the Mixamo FBXs in Assets/Anomaly/Gaunt/Mixamo/ (see Docs/GAUNT_ANOMALY.md):
  - the character, downloaded "With Skin"         (any name, e.g. Gaunt.fbx)
  - each animation, downloaded "Without Skin"     (Mixamo's own names are fine: "Zombie Walk",
                                                    "Mutant Run", "Zombie Dying"... are recognised;
                                                    anything unrecognised is kept under its own name)

Run in Blender 4.x / 5.x: Scripting workspace, Text > Open this file, Run Script.
Or from a terminal:  blender -b -P Tools/rig_gaunt_from_mixamo_blender.py
Starts from an empty scene: save anything open first.
"""
import glob
import os

import bpy

SCRIPT_NAME = "rig_gaunt_from_mixamo_blender.py"
CLIPS = ("Idle", "Walk", "Run", "Attack", "Death")
# Words in a Mixamo file name that mean each clip ("Zombie Dying.fbx" -> Death).
CLIP_WORDS = {
    "Death":  ("death", "dying", "die"),
    "Attack": ("attack", "swip", "punch", "bite", "slash", "hit"),
    "Run":    ("run", "sprint"),
    "Walk":   ("walk", "stalk", "crawl"),
    "Idle":   ("idle", "breath", "stand"),
}


def script_folder():
    try:
        here = os.path.abspath(__file__)
        if os.path.isfile(here):
            return os.path.dirname(here)
    except NameError:
        pass
    for text in bpy.data.texts:
        path = bpy.path.abspath(text.filepath) if text.filepath else ""
        if path and os.path.basename(path) == SCRIPT_NAME and os.path.isfile(path):
            return os.path.dirname(path)
    raise RuntimeError("Open Tools/%s with Text > Open and run it from there, "
                       "or run: blender -b -P Tools/%s" % (SCRIPT_NAME, SCRIPT_NAME))


GAUNT = os.path.join(os.path.dirname(script_folder()), "Assets", "Anomaly", "Gaunt")
SRC_DIR = os.path.join(GAUNT, "Mixamo")
OUT_DIR = os.path.join(GAUNT, "Rigged")
OUT_FBX = os.path.join(OUT_DIR, "SK_AH_Gaunt.fbx")


def clip_name(path):
    stem = os.path.splitext(os.path.basename(path))[0]
    low = stem.lower()
    for clip, words in CLIP_WORDS.items():
        if any(w in low for w in words):
            return clip
    return stem.replace(" ", "_")


def import_fbx(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path, automatic_bone_orientation=False, ignore_leaf_bones=True)
    return [o for o in bpy.data.objects if o not in before]


def action_of(arm):
    ad = arm.animation_data
    return ad.action if ad and ad.action else None


def main():
    files = sorted(glob.glob(os.path.join(SRC_DIR, "*.fbx")) + glob.glob(os.path.join(SRC_DIR, "*.FBX")))
    if not files:
        raise RuntimeError("No FBX files in %s - download them from Mixamo first (Docs/GAUNT_ANOMALY.md)" % SRC_DIR)

    bpy.ops.wm.read_factory_settings(use_empty=True)

    # 1. The character: the file that has a skinned mesh in it.
    character = None
    for path in files:
        objs = import_fbx(path)
        meshes = [o for o in objs if o.type == 'MESH']
        arms = [o for o in objs if o.type == 'ARMATURE']
        if meshes and arms:
            character = (path, arms[0], meshes, objs)
            break
        for o in objs:
            bpy.data.objects.remove(o, do_unlink=True)
    if character is None:
        raise RuntimeError("None of the FBXs has a skinned mesh: download the character once 'With Skin'")
    char_path, rig, meshes, _ = character
    rig.name = "Armature"

    # The character download carries a clip too (usually just the T-pose). Keep it only if
    # the file is named after a clip (e.g. you downloaded Idle "With Skin").
    kept = {}
    first = action_of(rig)
    if first and clip_name(char_path) in CLIPS:
        name = clip_name(char_path)
        first.name = name
        first.use_fake_user = True
        kept[name] = first

    # 2. Each animation file: steal its action, throw its armature away.
    for path in files:
        if path == char_path:
            continue
        objs = import_fbx(path)
        arms = [o for o in objs if o.type == 'ARMATURE']
        act = action_of(arms[0]) if arms else None
        if act:
            name = clip_name(path)
            if name in kept:                      # same clip twice: newest file wins
                bpy.data.actions.remove(kept[name])
            act.name = name
            act.use_fake_user = True
            kept[name] = act
        else:
            print("Gaunt: %s has no animation, skipped" % os.path.basename(path))
        for o in objs:
            bpy.data.objects.remove(o, do_unlink=True)

    # Mixamo bone names match between downloads, so every clip drives the one rig.
    # Drop anything left over that isn't one of our clips (e.g. leftovers from the imports).
    for act in list(bpy.data.actions):
        if act not in kept.values():
            bpy.data.actions.remove(act)
    if rig.animation_data:
        rig.animation_data.action = None

    # Textures are set up in Unreal by Tools/import_gaunt_ue58.py; don't drag Mixamo's along.
    for img in list(bpy.data.images):
        bpy.data.images.remove(img)

    os.makedirs(OUT_DIR, exist_ok=True)
    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True)
    for m in meshes:
        m.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.export_scene.fbx(
        filepath=OUT_FBX, use_selection=True, object_types={'ARMATURE', 'MESH'},
        add_leaf_bones=False, armature_nodetype='NULL',
        bake_anim=True, bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False,
        bake_anim_force_startend_keying=True, path_mode='STRIP', embed_textures=False,
        mesh_smooth_type='OFF', axis_forward='-Z', axis_up='Y')

    missing = [c for c in CLIPS if c not in kept]
    print("Gaunt: exported %s" % OUT_FBX)
    print("Gaunt: rig %s, %d bones, clips: %s" % (rig.name, len(rig.data.bones), ", ".join(sorted(kept))))
    if missing:
        print("Gaunt: no %s clip - it'll fall back (Run->Walk, others: stand still / ragdoll)" % ", ".join(missing))


main()
