"""Pose the Meshy Tactical Operator (rigged, Mixamo-style 24-bone skeleton) into three dead bodies
and bake each into a static mesh for MyProject5. No animation or ragdoll needed: they're props.

  SM_AH_DeadOperator_Slumped  sat against a wall, legs out, head lolled, arms limp   (front faces +X)
  SM_AH_DeadOperator_Prone    face down, one arm reaching ahead (dragged, then dropped) (head toward +X)
  SM_AH_DeadOperator_Supine   on his back, knee up, hand pressed to a wound           (head toward +X)

Input: the extracted Meshy ZIP "Meshy_AI_Tactical_Operator_TPose_Rigged_..._Running" (FBX + PNGs)
anywhere under the folder you pass (default Assets/Meshy, git-ignored).
Output: Assets/DeadOperators/ - three texture-free FBXs (real size, pivot = bottom centre, Unreal
frame) + T_AH_DeadOperator_D.jpg / _N.png (DirectX) / _MR.jpg shared by all three.
Then Tools/place_story_ue58.py imports and places them.

Run:  blender -b -P Tools/pose_dead_operators_blender.py -- [input folder]
"""
import glob
import math
import os
import sys

import bpy
import numpy as np
from mathutils import Matrix, Vector

TEX = 2048
X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))


def root():
    try:
        return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    except NameError:
        return os.getcwd()


ROOT = root()
OUT = os.path.join(ROOT, "Assets", "DeadOperators")


def find_fbx(src):
    for f in sorted(glob.glob(os.path.join(src, "**", "*.fbx"), recursive=True)):
        if "tactical_operator" in f.lower() and "rigged" in f.lower():
            return f
    return None


def load(fbx):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=fbx)
    arm = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE')
    mesh = next(o for o in bpy.context.scene.objects if o.type == 'MESH')
    if arm.animation_data:
        arm.animation_data_clear()                      # T-pose rest, not the run cycle
    for pb in arm.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)
    bpy.context.view_layer.update()
    return arm, mesh


def turn(arm, bone, axis, degrees):
    """Rotate a bone about a WORLD axis through its own joint (children follow)."""
    pb = arm.pose.bones[bone]
    m = pb.matrix.copy()                                 # armature space (= world axes here)
    head = m.to_translation()
    r = Matrix.Rotation(math.radians(degrees), 4, axis)
    pb.matrix = Matrix.Translation(head) @ r @ Matrix.Translation(-head) @ m
    bpy.context.view_layer.update()


# The rig faces -Y, up +Z, the character's left is +X. Rotations are applied in order, in world axes.
POSES = {
    "Slumped": [
        ("LeftUpLeg", X, -88), ("RightUpLeg", X, -88),          # sat down, legs out in front
        ("LeftUpLeg", Z, 22), ("RightUpLeg", Z, -10),           # splayed
        ("LeftLeg", Z, -55),                                    # left knee bent out flat
        ("RightLeg", X, 6),
        ("Spine02", X, -18),                                    # leaning back on the wall
        ("Spine", X, 14), ("Spine", Y, 6),                      # slumped
        ("neck", X, 30), ("Head", X, 12), ("Head", Y, 24),      # chin down, head lolled to one side
        ("LeftArm", Y, 78), ("RightArm", Y, -80),               # arms dropped
        ("LeftArm", X, 12), ("RightArm", X, 22),
        ("LeftForeArm", X, -35), ("RightForeArm", X, -70),      # one hand in the lap
    ],
    "Prone": [
        ("Hips", X, 90),                                        # face down, head towards -Y
        ("LeftArm", Z, -85),                                    # left arm reaching ahead
        ("LeftArm", X, 8),
        ("RightArm", Z, -100),                                  # right arm trailing by the side
        ("RightArm", X, 6),
        ("LeftUpLeg", Z, -10), ("RightUpLeg", Z, 14),
        ("RightLeg", X, 35),                                    # calf lifted a little
        ("Head", Y, 75),                                        # face turned to the side
    ],
    "Supine": [
        ("Hips", X, -90),                                       # on his back, head towards +Y
        ("LeftArm", Z, 55), ("RightArm", Z, -62),               # arms out, down towards the hips
        ("RightForeArm", Z, -110),                              # hand back across the stomach
        ("LeftUpLeg", X, -28), ("LeftLeg", X, 58),              # left knee up
        ("RightUpLeg", Z, -6),
        ("Head", Y, -55),                                       # head fallen to the side
    ],
}
# then turn the baked mesh so the front (sitting) or the head (lying) points at Unreal +X
FINAL_YAW = {"Slumped": 90, "Prone": 90, "Supine": -90}


def bake(arm, mesh, name):
    """Apply the posed armature to a copy of the mesh: a static mesh, real size, pivot bottom centre."""
    dg = bpy.context.evaluated_depsgraph_get()
    ev = mesh.evaluated_get(dg)
    data = bpy.data.meshes.new_from_object(ev, preserve_all_data_layers=True, depsgraph=dg)
    obj = bpy.data.objects.new("SM_AH_DeadOperator_" + name, data)
    bpy.context.scene.collection.objects.link(obj)
    data.transform(mesh.matrix_world)                    # includes the 0.01 armature scale
    data.transform(Matrix.Rotation(math.radians(FINAL_YAW[name]), 4, 'Z'))
    co = np.empty(len(data.vertices) * 3)
    data.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    lo, hi = co.min(0), co.max(0)
    data.transform(Matrix.Translation(Vector((-(lo[0] + hi[0]) / 2, -(lo[1] + hi[1]) / 2, -lo[2]))))
    data.update()
    for v in obj.vertex_groups[:]:
        obj.vertex_groups.remove(v)
    return obj, (hi - lo)


def export(obj, path):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={'MESH'},
                             mesh_smooth_type='FACE', path_mode='STRIP', embed_textures=False,
                             add_leaf_bones=False, bake_anim=False, axis_forward='-Z', axis_up='Y')


def textures(fbx):
    folder = os.path.dirname(fbx)
    maps = {}
    for png in glob.glob(os.path.join(folder, "*.png")):
        n = png.lower()
        if n.endswith("_normal.png"):
            maps["N"] = png
        elif n.endswith("_roughness.png"):
            maps["R"] = png
        elif n.endswith("_metallic.png"):
            maps["M"] = png
        elif not n.endswith("_metallic_roughness.png"):
            maps["D"] = png

    def load(p):
        img = bpy.data.images.load(p)
        img.colorspace_settings.name = 'Non-Color'
        img.scale(TEX, TEX)
        px = np.empty(TEX * TEX * 4, dtype=np.float32)
        img.pixels.foreach_get(px)
        bpy.data.images.remove(img)
        return px.reshape(TEX, TEX, 4)

    def save(arr, path):
        img = bpy.data.images.new("o", TEX, TEX, alpha=False)
        img.colorspace_settings.name = 'Non-Color'
        img.pixels.foreach_set(np.ascontiguousarray(arr, dtype=np.float32).ravel())
        img.filepath_raw = path
        img.file_format = 'JPEG' if path.endswith(".jpg") else 'PNG'
        img.save()
        bpy.data.images.remove(img)

    if "D" in maps:
        save(load(maps["D"]), os.path.join(OUT, "T_AH_DeadOperator_D.jpg"))
    if "N" in maps:
        n = load(maps["N"])
        n[..., 1] = 1.0 - n[..., 1]                      # OpenGL -> DirectX
        save(n, os.path.join(OUT, "T_AH_DeadOperator_N.png"))
    mr = np.ones((TEX, TEX, 4), dtype=np.float32)
    mr[..., 1] = load(maps["R"])[..., 0] if "R" in maps else 0.8
    mr[..., 2] = load(maps["M"])[..., 0] if "M" in maps else 0.0
    save(mr, os.path.join(OUT, "T_AH_DeadOperator_MR.jpg"))


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    src = os.path.abspath(args[0]) if args else os.path.join(ROOT, "Assets", "Meshy")
    fbx = find_fbx(src)
    if not fbx:
        raise SystemExit("[Dead operators] no rigged Tactical Operator FBX under " + src)
    os.makedirs(OUT, exist_ok=True)
    for name, moves in POSES.items():
        arm, mesh = load(fbx)
        for bone, axis, deg in moves:
            turn(arm, bone, axis, deg)
        obj, size = bake(arm, mesh, name)
        export(obj, os.path.join(OUT, "SM_AH_DeadOperator_%s.fbx" % name))
        print("[Dead operators] %s: %.0f x %.0f x %.0f cm" % (name, size[0] * 100, size[1] * 100, size[2] * 100))
    textures(fbx)
    print("[Dead operators] done -> " + OUT)


main()
