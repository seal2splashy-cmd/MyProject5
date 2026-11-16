"""Anomaly Hunter first-person containment operator - FIXED rig/export pipeline.

Based on the asset pack's Blender/rig_and_export_fbx.py (v0.3.0), with these fixes:

  1. AXIS FIX. The GLB stores the authoring frame (Z-up, -Y forward) inside a Y-up glTF,
     so Blender's importer lays the arms along -Z. The original rig builds its bones in
     the authoring frame, so bones, weights and mesh never lined up. We rotate the
     imported mesh -90 deg about X back into the authoring frame before rigging.
  2. THUMB FIX. The thumb assembly (thumb, thumb pad/web/saddle/stitches) and the thenar
     mound sat beside the little finger, with the hypothenar mound beside the index.
     We mirror those parts across each hand's centre line so the thumb sits next to the
     index finger, and move the thumb bones to match.
  3. SIDE FIX. With -Y forward and Z up, +X is the operator's LEFT, but the pack called
     that arm "_R". Bones are named by the real side, so hand_r is the right hand.
  4. Textures are NOT embedded in the FBX (they're imported into Unreal separately).

Run headless:
  blender -b -P rig_and_export_fbx_fixed.py -- <pack_root> <export_dir>
or with the bpy module:
  python rig_and_export_fbx_fixed.py -- <pack_root> <export_dir>
Defaults: pack root = ../AnomalyHunter_Operator_AssetPack next to this script, exports in ./Exports
"""
import bpy
import bmesh
import os
import sys
import math
from mathutils import Vector, Matrix

VERSION = "0.3.0-fixed1"
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
PACK_ROOT = os.path.abspath(args[0]) if len(args) > 0 else os.path.join(SCRIPT_DIR, "AnomalyHunter_Operator_AssetPack")
EXPORT_DIR = os.path.abspath(args[1]) if len(args) > 1 else os.path.join(SCRIPT_DIR, "Exports")
SOURCE_GLB = os.path.join(PACK_ROOT, "Meshes", "SK_AH_ContainmentOperator_FP_StaticPreview.glb")
TEXTURE_DIR = os.path.join(PACK_ROOT, "Textures")
FBX_PATH = os.path.join(EXPORT_DIR, "SK_AH_ContainmentOperator_FP.fbx")
BLEND_PATH = os.path.join(EXPORT_DIR, "AnomalyHunter_ContainmentOperator_Rigged.blend")
os.makedirs(EXPORT_DIR, exist_ok=True)

if not os.path.isfile(SOURCE_GLB):
    raise FileNotFoundError("Missing source GLB: " + SOURCE_GLB)

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1.0

bpy.ops.import_scene.gltf(filepath=SOURCE_GLB)
mesh_objects = [o for o in bpy.context.scene.objects if o.type == 'MESH']
if not mesh_objects:
    raise RuntimeError("GLB imported without mesh objects")

# -----------------------------------------------------------------------------
# FIX 1: bake everything into the authoring frame (Z up, -Y forward, metres)
# -----------------------------------------------------------------------------
TO_AUTHORING = Matrix.Rotation(math.radians(-90.0), 4, 'X')
for obj in mesh_objects:
    world = obj.matrix_world.copy()
    obj.parent = None
    obj.matrix_world = TO_AUTHORING @ world
for obj in [o for o in bpy.context.scene.objects if o.type == 'EMPTY']:
    bpy.data.objects.remove(obj, do_unlink=True)
for obj in mesh_objects:
    obj.data.transform(obj.matrix_world)
    obj.matrix_world = Matrix.Identity(4)
bpy.context.view_layer.update()

# sanity: shoulder caps should sit at the rig's shoulder joints (+-0.205, 0.075, 0.055)
cap = bpy.data.objects.get("SM_ShoulderCap_R")
if cap:
    c = sum((v.co for v in cap.data.vertices), Vector()) / len(cap.data.vertices)
    if abs(c.y - 0.075) > 0.03 or abs(c.z - 0.055) > 0.03:
        raise RuntimeError(f"Axis fix failed, shoulder cap at {tuple(round(x, 3) for x in c)}")

# -----------------------------------------------------------------------------
# FIX 2: move the thumb assembly to the index-finger side of each hand
# -----------------------------------------------------------------------------
HAND_CENTRE_X = 0.125
THUMB_SIDE_KEYS = ("GloveThumb", "ThumbPad", "ThumbWeb", "ThumbSaddle", "ThumbFlexStitch",
                   "ThenarEminence", "HypothenarEminence")
mirrored = 0
for obj in mesh_objects:
    if not any(k in obj.name for k in THUMB_SIDE_KEYS):
        continue
    sign = 1.0 if obj.name.endswith("_R") or "_R_" in obj.name else -1.0
    cx = sign * HAND_CENTRE_X
    # x -> 2cx - x
    m = Matrix.Translation((cx, 0, 0)) @ Matrix.Scale(-1.0, 4, (1, 0, 0)) @ Matrix.Translation((-cx, 0, 0))
    obj.data.transform(m)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.reverse_faces(bm, faces=bm.faces[:])   # mirroring flips winding
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()
    mirrored += 1
print(f"Thumb fix: mirrored {mirrored} parts")

# -----------------------------------------------------------------------------
# SKELETON  (FIX 3: +X is the operator's left)
# -----------------------------------------------------------------------------
arm_data = bpy.data.armatures.new("SKEL_AH_ContainmentOperator_FP")
arm = bpy.data.objects.new("Armature", arm_data)   # "Armature" so Unreal doesn't add it as an extra root bone
bpy.context.collection.objects.link(arm)
bpy.context.view_layer.objects.active = arm
arm.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')

BONE_SEGMENTS = {}

def add_bone(name, head, tail, parent=None, connected=False):
    b = arm_data.edit_bones.new(name)
    b.head = Vector(head)
    b.tail = Vector(tail)
    if parent:
        b.parent = arm_data.edit_bones[parent]
        b.use_connect = connected
    return b

def side_of_sign(sign):
    return "l" if sign > 0 else "r"

def side_of_object(name):
    # pack "_R" objects are at +X = operator's left
    return "l" if (name.endswith("_R") or "_R_" in name) else "r"

add_bone("root", (0.0, 0.075, 0.0), (0.0, 0.075, 0.08))
for sign in (-1, 1):
    side = side_of_sign(sign)
    shoulder = (sign * 0.205, 0.075, 0.055)
    elbow = (sign * 0.165, -0.365, -0.005)
    forearm_mid = (sign * 0.145, -0.535, -0.055)
    wrist = (sign * 0.125, -0.735, -0.105)
    palm_start = (sign * 0.125, -0.785, -0.132)
    palm_end = (sign * 0.125, -0.910, -0.157)

    add_bone("clavicle_" + side, (0.0, 0.075, 0.055), shoulder, "root")
    add_bone("upperarm_" + side, shoulder, elbow, "clavicle_" + side, True)
    add_bone("lowerarm_" + side, elbow, forearm_mid, "upperarm_" + side, True)
    add_bone("forearm_twist_" + side, forearm_mid, wrist, "lowerarm_" + side, True)
    add_bone("wrist_" + side, wrist, palm_start, "forearm_twist_" + side, True)
    add_bone("hand_" + side, palm_start, palm_end, "wrist_" + side, True)

    finger_offsets = [-0.044, -0.014, 0.016, 0.044]
    lengths = [0.103, 0.119, 0.112, 0.091]
    digit_names = ["index", "middle", "ring", "pinky"]
    for off, length, digit in zip(finger_offsets, lengths, digit_names):
        x = sign * (0.125 + off)
        p0 = (x, -0.905, -0.158)
        p1 = (x + sign * off * 0.025, -0.951 - length * 0.27, -0.169)
        p2 = (x + sign * off * 0.045, -0.951 - length * 0.59, -0.184)
        p3 = (x + sign * off * 0.060, -0.951 - length * 0.83, -0.197)
        p4 = (x + sign * off * 0.065, -0.951 - length, -0.204)
        b1, b2, b3 = digit + "_01_" + side, digit + "_02_" + side, digit + "_03_" + side
        add_bone(b1, p0, p1, "hand_" + side)
        add_bone(b2, p1, p2, b1, True)
        add_bone(b3, p2, p4, b2, True)
        BONE_SEGMENTS[b1] = (Vector(p0), Vector(p1))
        BONE_SEGMENTS[b2] = (Vector(p1), Vector(p2))
        BONE_SEGMENTS[b3] = (Vector(p2), Vector(p4))

    # Thumb, mirrored across the hand centre to match FIX 2 (now beside the index finger)
    thumb_src = [(0.184, -0.806, -0.136), (0.176, -0.846, -0.154),
                 (0.155, -0.892, -0.177), (0.134, -0.925, -0.194)]
    thumb_points = [(sign * (2 * HAND_CENTRE_X - x), y, z) for (x, y, z) in thumb_src]
    for i in range(3):
        nm = f"thumb_0{i+1}_{side}"
        parent = "hand_" + side if i == 0 else f"thumb_0{i}_{side}"
        add_bone(nm, thumb_points[i], thumb_points[i + 1], parent, i > 0)
        BONE_SEGMENTS[nm] = (Vector(thumb_points[i]), Vector(thumb_points[i + 1]))

bpy.ops.object.mode_set(mode='OBJECT')
arm.show_in_front = True
arm.display_type = 'WIRE'

# -----------------------------------------------------------------------------
# WEIGHTING (unchanged logic from the pack, using the corrected side mapping)
# -----------------------------------------------------------------------------
def group_for(obj, name):
    g = obj.vertex_groups.get(name)
    return g if g else obj.vertex_groups.new(name=name)

def set_vertex_weights(obj, index, weights):
    total = sum(max(0.0, w) for _, w in weights)
    if total <= 1e-9:
        return
    for name, weight in weights:
        w = max(0.0, weight) / total
        if w > 0.001:
            group_for(obj, name).add([index], w, 'REPLACE')

def smoothstep(a, b, x):
    if b <= a:
        return 0.0
    t = max(0.0, min(1.0, (x - a) / (b - a)))
    return t * t * (3.0 - 2.0 * t)

def segment_distance(point, start, end):
    axis = end - start
    denom = axis.length_squared
    if denom < 1e-10:
        return (point - start).length
    t = max(0.0, min(1.0, (point - start).dot(axis) / denom))
    return (point - (start + axis * t)).length

def assign_to_digit(obj, side, digit, thumb=False):
    prefix = "thumb" if thumb else digit.lower()
    bone_names = [f"{prefix}_01_{side}", f"{prefix}_02_{side}", f"{prefix}_03_{side}"]
    segments = [BONE_SEGMENTS[n] for n in bone_names]
    sigma = 0.016 if not thumb else 0.020
    for v in obj.data.vertices:
        p = obj.matrix_world @ v.co
        ds = [segment_distance(p, a, b) for a, b in segments]
        vals = [math.exp(-(d * d) / (2 * sigma * sigma)) for d in ds]
        vals = [max(vv, 0.015) for vv in vals]
        set_vertex_weights(obj, v.index, list(zip(bone_names, vals)))

def assign_arm_surface(obj, side):
    for v in obj.data.vertices:
        p = obj.matrix_world @ v.co
        y = p.y
        depth = -y
        if y > -0.16:
            t = smoothstep(0.035, 0.16, depth)
            weights = [("clavicle_" + side, 1.0 - t), ("upperarm_" + side, t)]
        elif y > -0.285:
            weights = [("upperarm_" + side, 1.0)]
        elif y > -0.445:
            t = smoothstep(0.285, 0.445, depth)
            weights = [("upperarm_" + side, 1.0 - t), ("lowerarm_" + side, t)]
        elif y > -0.515:
            weights = [("lowerarm_" + side, 1.0)]
        elif y > -0.690:
            t = smoothstep(0.515, 0.690, depth)
            weights = [("lowerarm_" + side, 1.0 - t), ("forearm_twist_" + side, t)]
        elif y > -0.715:
            weights = [("forearm_twist_" + side, 1.0)]
        else:
            t = smoothstep(0.715, 0.785, depth)
            weights = [("forearm_twist_" + side, 1.0 - t), ("wrist_" + side, t)]
        set_vertex_weights(obj, v.index, weights)

def assign_forearm_part(obj, side):
    for v in obj.data.vertices:
        p = obj.matrix_world @ v.co
        t = smoothstep(0.47, 0.69, -p.y)
        set_vertex_weights(obj, v.index, [("lowerarm_" + side, 1.0 - t), ("forearm_twist_" + side, t)])

def assign_all(obj, bone_name):
    if len(obj.data.vertices):
        group_for(obj, bone_name).add(list(range(len(obj.data.vertices))), 1.0, 'REPLACE')

digit_map = {1: "index", 2: "middle", 3: "ring", 4: "pinky"}
for obj in mesh_objects:
    name = obj.name
    side = side_of_object(name)
    digit_idx = next((i for i in range(1, 5) if f"Finger{i}_" in name), None)

    if any(k in name for k in ("ContainmentSleeve", "SleeveSeam", "StitchDash", "Elbow_Fold")):
        assign_arm_surface(obj, side)
    elif "ShoulderCap" in name:
        assign_arm_surface(obj, side)
    elif "Insignia" in name:
        assign_all(obj, "upperarm_" + side)
    elif digit_idx is not None:
        assign_to_digit(obj, side, digit_map[digit_idx])
    elif "FingerFlexSeam" in name or "FingerStitch" in name or "KnucklePad" in name or "FingertipPad" in name:
        idx = next((i for i in range(1, 5) if any(f"{k}{i}_" in name for k in ("FingerFlexSeam", "FingerStitch", "KnucklePad", "FingertipPad"))), 1)
        assign_to_digit(obj, side, digit_map[idx])
    elif any(k in name for k in ("GloveThumb", "ThumbPad", "ThumbWeb", "ThumbSaddle", "ThumbFlexStitch")):
        assign_to_digit(obj, side, "thumb", thumb=True)
    elif any(k in name for k in ("GlovePalm", "Backhand", "PalmHeel", "GripDot", "Thenar", "Hypothenar", "DorsalTendon", "Backhand_Piping")):
        assign_all(obj, "hand_" + side)
    elif "Cuff" in name:
        assign_all(obj, "wrist_" + side)
    elif any(k in name for k in ("Forearm", "Guard", "Rib", "Strap", "Buckle")):
        assign_forearm_part(obj, side)
    else:
        assign_all(obj, "lowerarm_" + side)

for obj in mesh_objects:
    if not obj.data.uv_layers:
        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.uv.smart_project(angle_limit=math.radians(62), island_margin=0.012)
        bpy.ops.object.mode_set(mode='OBJECT')

bpy.ops.object.select_all(action='DESELECT')
for obj in mesh_objects:
    obj.select_set(True)
bpy.context.view_layer.objects.active = mesh_objects[0]
bpy.ops.object.join()
skin = bpy.context.object
skin.name = "SK_AH_ContainmentOperator_FP"
skin.data.name = "SK_AH_ContainmentOperator_FP_Mesh"

valid_bones = {b.name for b in arm_data.bones if b.use_deform}
missing_groups = sorted({g.name for g in skin.vertex_groups} - valid_bones)
if missing_groups:
    raise RuntimeError("Vertex groups without deform bones: " + ", ".join(missing_groups))
unweighted = 0
bad_sums = 0
for vertex in skin.data.vertices:
    if not vertex.groups:
        unweighted += 1
        continue
    total = sum(element.weight for element in vertex.groups)
    if abs(total - 1.0) > 0.025:
        bad_sums += 1
if unweighted or bad_sums:
    raise RuntimeError(f"Weight QA failed: {unweighted} unweighted vertices, {bad_sums} vertices with non-normalized weights")
if len(arm_data.bones) != 43:
    raise RuntimeError(f"Expected 43 skeleton bones including root, found {len(arm_data.bones)}")
print(f"Rig QA passed: {len(arm_data.bones)} bones; {len(skin.data.vertices)} weighted vertices")

# Drop unused material slots ("Dots Stroke", "Material") so Unreal only gets real slots
used = {p.material_index for p in skin.data.polygons}
for i in reversed(range(len(skin.material_slots))):
    if i not in used:
        skin.active_material_index = i
        bpy.context.view_layer.objects.active = skin
        bpy.ops.object.material_slot_remove()
print("Material slots:", [s.material.name for s in skin.material_slots if s.material])

# -----------------------------------------------------------------------------
# PBR MATERIALS (Blender preview only - Unreal materials are built by ue_setup_project.py)
# -----------------------------------------------------------------------------
def load_image(filename):
    path = os.path.join(TEXTURE_DIR, filename)
    if not os.path.isfile(path):
        return None
    return bpy.data.images.load(path, check_existing=True)

def rebuild_material(mat, stem, metallic, roughness):
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    bc = load_image(f"T_{stem}_BC.png")
    rough = load_image(f"T_{stem}_R.png")
    normal = load_image(f"T_{stem}_N.png")
    if bc:
        tex = nt.nodes.new("ShaderNodeTexImage"); tex.image = bc
        nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    if rough:
        rough.colorspace_settings.name = 'Non-Color'
        tex = nt.nodes.new("ShaderNodeTexImage"); tex.image = rough
        nt.links.new(tex.outputs["Color"], bsdf.inputs["Roughness"])
    if normal:
        normal.colorspace_settings.name = 'Non-Color'
        tex = nt.nodes.new("ShaderNodeTexImage"); tex.image = normal
        nmap = nt.nodes.new("ShaderNodeNormalMap")
        nt.links.new(tex.outputs["Color"], nmap.inputs["Color"])
        nt.links.new(nmap.outputs["Normal"], bsdf.inputs["Normal"])

for mat in bpy.data.materials:
    n = mat.name.lower()
    if "containment_fabric" in n:
        rebuild_material(mat, "ContainmentFabric", 0.0, 0.88)
    elif "glove_polymer" in n:
        rebuild_material(mat, "GlovePolymer", 0.02, 0.64)
    elif "protective_composite" in n:
        rebuild_material(mat, "ForearmGuard", 0.12, 0.49)
    elif "insignia" in n:
        rebuild_material(mat, "Containment_Insignia", 0.0, 0.91)

mod = skin.modifiers.new("Armature_Deform", 'ARMATURE')
mod.object = arm
mod.use_deform_preserve_volume = True
skin.parent = arm
skin.matrix_parent_inverse = arm.matrix_world.inverted()

for poly in skin.data.polygons:
    poly.use_smooth = True
wn = skin.modifiers.new("Weighted_Surface_Normals", 'WEIGHTED_NORMAL')
wn.keep_sharp = True

bpy.ops.object.select_all(action='DESELECT')
arm.select_set(True); skin.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.wm.save_as_mainfile(filepath=BLEND_PATH, relative_remap=False)
bpy.ops.export_scene.fbx(
    filepath=FBX_PATH,
    use_selection=True,
    object_types={'ARMATURE', 'MESH'},
    use_mesh_modifiers=True,
    apply_unit_scale=True,
    apply_scale_options='FBX_SCALE_UNITS',
    axis_forward='-Y', axis_up='Z',
    add_leaf_bones=False,
    use_armature_deform_only=True,
    bake_anim=False,
    mesh_smooth_type='FACE',
    use_tspace=True,
    path_mode='STRIP',
    embed_textures=False,
)
print("Anomaly Hunter containment operator v" + VERSION + " exported")
print("Skeletal FBX:", FBX_PATH, round(os.path.getsize(FBX_PATH) / 1e6, 1), "MB")
print("Editable Blender project:", BLEND_PATH)
