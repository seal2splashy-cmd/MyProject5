"""Build the service pistol static mesh for MyProject5 and export it as FBX.

A generic polymer-frame, striker-fired service pistol (no real make or markings):
slide with front/rear serrations and an ejection port, barrel hood and crown,
raked grip with stippled side panels, trigger guard, accessory rail notches,
three-dot sights (glowing dots), slide stop, magazine release and base plate.

Run in Blender 4.x / 5.x: Scripting workspace, Text > Open this file, Run Script.
Or from a terminal:  blender -b -P Tools/build_pistol_blender.py
Writes Assets/Weapons/SM_AH_ServicePistol.fbx (Tools/import_pistol_ue58.py imports it).
Starts from an empty scene: save anything open first.

Frame (as it lands in Unreal, centimetres): +X out of the muzzle, +Z up, +Y the gun's right
(ejection port side). Origin placed so the centre of the grip is at (-3, 0, -7), matching
AGun's hand setup. Blender's -Y becomes Unreal's +Y on import, so "right" is -Y here.
"""
import math
import os

import bpy      # first: when running as the pip 'bpy' module, bmesh only exists after this
import bmesh
from mathutils import Vector

SCRIPT_NAME = "build_pistol_blender.py"
CM = 0.01                      # model in cm, Blender scene in metres (exports as cm to Unreal)
RIGHT = -1.0                   # Blender Y sign of the gun's right side


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


OUT_DIR = os.path.join(os.path.dirname(script_folder()), "Assets", "Weapons")
OUT_FBX = os.path.join(OUT_DIR, "SM_AH_ServicePistol.fbx")

# Design-space points (cm) used to place the origin and tell AGun where things are.
GRIP_CENTRE = Vector((1.3, 0.0, -7.3))      # middle of the grip, where the palm wraps
GRIP_TARGET = Vector((-3.0, 0.0, -7.0))      # where AGun expects the grip relative to the pivot
MUZZLE = Vector((18.72, 0.0, 0.0))           # bore exit


# --------------------------------------------------------------------------- scene / materials

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1.0


def make_material(name, colour, metallic, roughness, emission=None, strength=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*colour, 1.0)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    if emission:
        bsdf.inputs["Emission Color"].default_value = (*emission, 1.0)
        bsdf.inputs["Emission Strength"].default_value = strength
    m.diffuse_color = (*colour, 1.0)
    return m


MAT = {
    "slide": make_material("M_Pistol_Slide", (0.035, 0.037, 0.040), 0.85, 0.38),
    "frame": make_material("M_Pistol_Frame", (0.022, 0.022, 0.024), 0.0, 0.62),
    "grip":  make_material("M_Pistol_Grip",  (0.016, 0.016, 0.018), 0.0, 0.92),
    "steel": make_material("M_Pistol_Steel", (0.20, 0.20, 0.21), 1.0, 0.30),
    "dots":  make_material("M_Pistol_Dots",  (0.75, 0.95, 0.80), 0.0, 0.40, (0.25, 1.0, 0.45), 2.0),
}

parts = []


def finish(obj, mat, bevel=0.0, segments=2, angle=40.0):
    obj.data.materials.append(MAT[mat])
    if bevel > 0:
        b = obj.modifiers.new("Bevel", 'BEVEL')
        b.width = bevel * CM
        b.segments = segments
        b.limit_method = 'ANGLE'
        b.angle_limit = math.radians(angle)
        b.harden_normals = False
    parts.append(obj)
    return obj


def mesh_object(name, bm):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(name, me)
    scene.collection.objects.link(obj)
    return obj


def prism(name, profile_xz, y0, y1, mat, bevel=0.0, segments=2):
    """Side-view outline (x, z in cm) extruded across the gun from y0 to y1 (cm)."""
    bm = bmesh.new()
    a = [bm.verts.new((x * CM, y0 * CM, z * CM)) for x, z in profile_xz]
    b = [bm.verts.new((x * CM, y1 * CM, z * CM)) for x, z in profile_xz]
    bm.faces.new(a)
    bm.faces.new(list(reversed(b)))
    n = len(profile_xz)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((a[i], a[j], b[j], b[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return finish(mesh_object(name, bm), mat, bevel, segments)


def box(name, x0, x1, y0, y1, z0, z1, mat=None, bevel=0.0, segments=2):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co.x = (x0 if v.co.x < 0 else x1) * CM
        v.co.y = (y0 if v.co.y < 0 else y1) * CM
        v.co.z = (z0 if v.co.z < 0 else z1) * CM
    obj = mesh_object(name, bm)
    if mat:
        finish(obj, mat, bevel, segments)
    return obj


def cylinder_x(name, x0, x1, y, z, r, mat=None, verts=32, bevel=0.0):
    """Cylinder along X."""
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=verts, radius1=r * CM, radius2=r * CM,
                          depth=(x1 - x0) * CM)
    for v in bm.verts:
        v.co = Vector((v.co.z + (x0 + x1) * 0.5 * CM, v.co.y + y * CM, v.co.x + z * CM))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    obj = mesh_object(name, bm)
    if mat:
        finish(obj, mat, bevel, 1)
    return obj


def cut(target, cutters):
    """Boolean-subtract cutter objects (joined) from target, then delete them."""
    if not cutters:
        return
    if len(cutters) > 1:
        ctx = {"active_object": cutters[0], "selected_editable_objects": cutters}
        with bpy.context.temp_override(**ctx):
            bpy.ops.object.join()
    c = cutters[0]
    mod = target.modifiers.new("Cut_" + c.name, 'BOOLEAN')
    mod.operation = 'DIFFERENCE'
    mod.solver = 'EXACT'
    mod.use_self = True            # joined cutters overlap each other (bore + spring hole)
    mod.use_hole_tolerant = True
    mod.object = c
    # Booleans before bevels so the cut edges get bevelled too.
    while target.modifiers[0] != mod:
        bpy.context.view_layer.objects.active = target
        with bpy.context.temp_override(object=target):
            bpy.ops.object.modifier_move_up(modifier=mod.name)
    c.hide_set(True)
    c.hide_render = True
    target["_cutters"] = list(target.get("_cutters", [])) + [c.name]


# --------------------------------------------------------------------------- slide

SLIDE_W = 1.25
slide = prism("Slide", [(0.0, -1.3), (18.6, -1.3), (18.6, 0.95), (18.15, 1.5), (0.35, 1.5), (0.0, 1.15)],
              -SLIDE_W, SLIDE_W, "slide", bevel=0.12)

grooves = []
for i in range(8):                                   # rear cocking serrations
    x = 0.75 + i * 0.36
    for side in (-1, 1):
        y_out = side * (SLIDE_W + 0.2)
        y_in = side * (SLIDE_W - 0.09)
        grooves.append(box("g", x, x + 0.16, min(y_in, y_out), max(y_in, y_out), -0.9, 1.25))
for i in range(5):                                   # front serrations
    x = 14.9 + i * 0.36
    for side in (-1, 1):
        y_out = side * (SLIDE_W + 0.2)
        y_in = side * (SLIDE_W - 0.09)
        grooves.append(box("g", x, x + 0.16, min(y_in, y_out), max(y_in, y_out), -0.6, 1.2))
# ejection port, right side, open over the top
grooves.append(box("port", 6.1, 10.4, min(RIGHT * 1.5, 0.15 * -RIGHT), max(RIGHT * 1.5, 0.15 * -RIGHT), 0.05, 1.9))
# bore + recoil spring opening in the slide front
grooves.append(cylinder_x("bore_slide", 17.5, 19.0, 0.0, 0.0, 0.66))
grooves.append(cylinder_x("rod_hole", 17.8, 19.0, 0.0, -0.85, 0.3))
cut(slide, grooves)

# barrel: hood visible in the port, crown at the muzzle
hood = box("BarrelHood", 5.9, 10.6, -0.72, 0.72, -0.7, 1.34, "steel", bevel=0.06)
crown = cylinder_x("BarrelCrown", 17.6, 18.72, 0.0, 0.0, 0.62, "steel", bevel=0.04)
cut(crown, [cylinder_x("bore", 17.0, 19.5, 0.0, 0.0, 0.42)])
guide = cylinder_x("GuideRod", 17.7, 18.55, 0.0, -0.85, 0.24, "steel")

# --------------------------------------------------------------------------- frame + grip

FRAME_W = 1.15
frame_profile = [
    (-0.9, -1.45), (0.4, -1.3), (17.3, -1.3), (17.55, -1.55), (17.55, -2.6), (17.2, -2.95),
    (12.6, -2.95), (12.15, -3.35), (11.85, -5.0), (11.1, -5.75), (6.7, -5.75), (5.75, -5.25),
    (5.0, -6.4), (4.4, -7.5), (4.1, -8.6), (3.3, -10.9), (2.9, -11.8), (-2.75, -11.8),
    (-1.75, -7.6), (-0.2, -3.3), (-0.6, -2.4), (-1.15, -1.95),
]
frame = prism("Frame", frame_profile, -FRAME_W, FRAME_W, "frame", bevel=0.14)
frame_cuts = [
    prism("guard_hole", [(11.2, -3.25), (10.95, -4.85), (10.35, -5.25), (7.25, -5.25), (6.45, -4.65), (6.2, -3.25)],
          -2.0, 2.0, "frame"),
    box("rail1", 13.2, 13.8, -2.0, 2.0, -3.1, -2.7),
    box("rail2", 14.6, 15.2, -2.0, 2.0, -3.1, -2.7),
    box("rail3", 16.0, 16.6, -2.0, 2.0, -3.1, -2.7),
]
for c in frame_cuts[:1]:
    c.data.materials.clear()
    parts.remove(c)
cut(frame, frame_cuts)

# wider grip body (the frame above stays slim)
grip = prism("GripBody", [(-0.3, -3.0), (5.85, -3.0), (5.55, -5.3), (4.8, -6.4), (4.2, -7.5), (3.9, -8.6),
                          (3.15, -10.9), (2.85, -11.8), (-2.75, -11.8), (-1.75, -7.6)],
             -1.45, 1.45, "frame", bevel=0.35, segments=3)

# stippled side panels, slightly proud of the grip
for side in (-1, 1):
    y0, y1 = sorted((side * 1.38, side * 1.5))
    prism("Stipple_%s" % ("R" if side == RIGHT else "L"),
          [(-1.0, -4.2), (4.6, -4.2), (4.0, -6.5), (3.5, -8.4), (2.8, -10.5), (-2.05, -10.5), (-1.45, -7.6)],
          y0, y1, "grip", bevel=0.06, segments=1)

# magazine base plate, slightly flared
box("BasePlate", -3.15, 3.15, -1.6, 1.6, -12.55, -11.75, "frame", bevel=0.2, segments=2)

# trigger
prism("Trigger", [(8.35, -3.1), (8.75, -3.1), (8.62, -4.15), (8.35, -4.85), (7.95, -5.0), (8.08, -4.35), (8.18, -3.7)],
      -0.42, 0.42, "frame", bevel=0.08)
box("TriggerSafety", 8.28, 8.52, -0.12, 0.12, -4.4, -3.6, "steel", bevel=0.03, segments=1)

# --------------------------------------------------------------------------- controls & sights

LEFT = -RIGHT
box("SlideStop", 6.3, 10.0, min(LEFT * 1.12, LEFT * 1.38), max(LEFT * 1.12, LEFT * 1.38), -1.55, -1.1, "steel", bevel=0.05)
box("SlideStopTab", 6.3, 7.3, min(LEFT * 1.12, LEFT * 1.45), max(LEFT * 1.12, LEFT * 1.45), -1.75, -1.1, "steel", bevel=0.05)
box("MagRelease", 4.85, 5.6, min(LEFT * 1.3, LEFT * 1.62), max(LEFT * 1.3, LEFT * 1.62), -3.85, -3.15, "frame", bevel=0.08)
box("Takedown", 11.6, 12.4, -1.3, 1.3, -1.6, -1.35, "steel", bevel=0.04, segments=1)

rear = box("RearSight", 0.5, 1.6, -1.0, 1.0, 1.45, 2.2, "slide", bevel=0.08)
cut(rear, [box("notch", 0.0, 2.0, -0.22, 0.22, 1.75, 2.5)])
box("FrontSight", 16.7, 17.55, -0.3, 0.3, 1.45, 2.2, "slide", bevel=0.06)
for y in (-0.55, 0.55):
    cylinder_x("RearDot", 0.42, 0.52, y, 1.95, 0.13, "dots", verts=16)
cylinder_x("FrontDot", 16.62, 16.72, 0.0, 1.95, 0.13, "dots", verts=16)


# --------------------------------------------------------------------------- bake, join, export

def apply_all(obj):
    bpy.context.view_layer.objects.active = obj
    for m in list(obj.modifiers):
        with bpy.context.temp_override(object=obj, active_object=obj):
            bpy.ops.object.modifier_apply(modifier=m.name)


for obj in parts:
    apply_all(obj)
for name in {n for o in parts for n in o.get("_cutters", [])}:
    o = bpy.data.objects.get(name)
    if o:
        bpy.data.objects.remove(o, do_unlink=True)

for obj in parts:
    me = obj.data
    if hasattr(me, "set_sharp_from_angle"):
        me.set_sharp_from_angle(angle=math.radians(38))
    for p in me.polygons:
        p.use_smooth = True

with bpy.context.temp_override(active_object=parts[0], selected_editable_objects=parts):
    bpy.ops.object.join()
gun = parts[0]
gun.name = gun.data.name = "SM_AH_ServicePistol"
with bpy.context.temp_override(active_object=gun, object=gun):
    bpy.ops.object.material_slot_remove_unused()

# Pivot: grip centre lands on GRIP_TARGET.
offset = (GRIP_TARGET - GRIP_CENTRE) * CM
for v in gun.data.vertices:
    v.co += offset
muzzle_ue = MUZZLE + (GRIP_TARGET - GRIP_CENTRE)

# One UV set (lightmaps + any future textures).
bpy.context.view_layer.objects.active = gun
gun.select_set(True)
with bpy.context.temp_override(active_object=gun, object=gun):
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=0.02)
    bpy.ops.object.mode_set(mode='OBJECT')

os.makedirs(OUT_DIR, exist_ok=True)
bpy.ops.object.select_all(action='DESELECT')
gun.select_set(True)
bpy.ops.export_scene.fbx(
    filepath=OUT_FBX, use_selection=True, object_types={'MESH'}, use_mesh_modifiers=True,
    mesh_smooth_type='OFF', add_leaf_bones=False, bake_anim=False, path_mode='STRIP',
    axis_forward='-Z', axis_up='Y', apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE')

dims = gun.dimensions / CM
print("Pistol: %d tris, %.1f x %.1f x %.1f cm, materials: %s" % (
    sum(len(p.vertices) - 2 for p in gun.data.polygons), dims.x, dims.y, dims.z,
    ", ".join(m.name for m in gun.data.materials if m)))
print("Pistol: muzzle at (%.2f, %.2f, %.2f) cm from the pivot (Unreal frame)" % (muzzle_ue.x, -muzzle_ue.y, muzzle_ue.z))
print("Pistol: exported " + OUT_FBX)
