"""Create a stylised, non-graphic faceless research anomaly for Unreal 5.8.

Run in Blender 4.x / 5.x with the Scripting workspace (Text > Open this file,
then Run Script), or from a terminal:
    blender -b -P Tools/build_forest_anomaly_blender.py
Generates an armature-skinned character and seven simple in-place action clips.
Exports into <script folder>/Exports/ForestAnomaly (i.e. Tools/Exports/ForestAnomaly).
Only a procedural first-pass model; not a photorealistic final character.
"""
import os
import math
from mathutils import Vector, Quaternion
import bpy

SCRIPT_NAME = "build_forest_anomaly_blender.py"

def script_folder():
    """Folder this script lives in, however it was started.

    From a terminal (-P) __file__ is the real path. From Blender's Text Editor it
    is "<blend file path>/<text name>", which is not a real folder (and is "/"
    for an unsaved file), so fall back to the path of the opened text block.
    """
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
    raise RuntimeError("Can't tell where %s lives. In Blender use Text > Open on "
                       "Tools/%s and run it from there, or run: blender -b -P Tools/%s"
                       % (SCRIPT_NAME, SCRIPT_NAME, SCRIPT_NAME))

ROOT = script_folder()
OUT = os.path.join(ROOT, "Exports", "ForestAnomaly")
os.makedirs(OUT, exist_ok=True)

# WARNING: clears current Blender scene; save any open work separately.
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)

def material(name, color, rough=0.85, metallic=0):
    m=bpy.data.materials.new(name)
    m.diffuse_color=(*color,1.0)
    m.use_nodes=True
    node=m.node_tree.nodes.get("Principled BSDF")
    node.inputs["Base Color"].default_value=(*color,1.0)
    node.inputs["Roughness"].default_value=rough
    node.inputs["Metallic"].default_value=metallic
    return m

uniform=material("M_AH_FadedContainmentSuit",(.105,.14,.145))
fabric=material("M_AH_SuitSeams",(.045,.06,.065))
glove=material("M_AH_DarkPolymer",(.033,.046,.051))
mask=material("M_AH_BlackFaceplate",(.018,.023,.027),.30)
pale=material("M_AH_PaleCeramic",(.44,.50,.49),.70)
strap=material("M_AH_Strap",(.18,.22,.22))
light=material("M_AH_ColdIndicator",(.07,.67,.73),.28)

# Armature in Blender metres; +Z up and -Y forward.
bones=[
 ("root",(0,0,.035),(0,0,.14),None),
 ("pelvis",(0,0,1.05),(0,0,1.16),"root"),
 ("spine",(0,0,1.16),(0,0,1.39),"pelvis"),
 ("chest",(0,0,1.39),(0,0,1.65),"spine"),
 ("neck",(0,0,1.65),(0,0,1.79),"chest"),
 ("head",(0,0,1.79),(0,0,2.08),"neck"),
]
for side,sgn in (("l",1),("r",-1)):
    bones.extend([
        ("clavicle_"+side,(0,0,1.57),(.33*sgn,0,1.58),"chest"),
        ("upperarm_"+side,(.33*sgn,0,1.58),(.54*sgn,0,1.20),"clavicle_"+side),
        ("lowerarm_"+side,(.54*sgn,0,1.20),(.57*sgn,-.045,.77),"upperarm_"+side),
        ("hand_"+side,(.57*sgn,-.045,.77),(.61*sgn,-.09,.54),"lowerarm_"+side),
        ("thigh_"+side,(.19*sgn,0,1.05),(.22*sgn,-.025,.58),"pelvis"),
        ("calf_"+side,(.22*sgn,-.025,.58),(.22*sgn,-.005,.12),"thigh_"+side),
        ("foot_"+side,(.22*sgn,-.005,.12),(.22*sgn,-.20,.06),"calf_"+side),
    ])
data=bpy.data.armatures.new("SKEL_AH_ForestAnomaly")
rig=bpy.data.objects.new("Armature",data)
bpy.context.collection.objects.link(rig)
bpy.context.view_layer.objects.active=rig
rig.select_set(True)
bpy.ops.object.mode_set(mode="EDIT")
for name,h,t,parent in bones:
    b=data.edit_bones.new(name)
    b.head=h
    b.tail=t
    if parent: b.parent=data.edit_bones[parent]
bpy.ops.object.mode_set(mode="OBJECT")
rig.select_set(False)
pieces=[]

def weight_mesh(obj,bone,mat):
    bpy.context.view_layer.objects.active=obj
    obj.data.materials.append(mat)
    vg=obj.vertex_groups.new(name=bone)
    vg.add(list(range(len(obj.data.vertices))),1.0,'REPLACE')
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    pieces.append(obj)
    return obj

def oval(name,loc,scale,bone,mat,segments=12,rings=8):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments,ring_count=rings,location=loc)
    o=bpy.context.object
    o.name=name
    o.scale=scale
    return weight_mesh(o,bone,mat)

def limb(name,a,b,r0,r1,bone,mat,vertices=12):
    # Tapered capsule limb from endpoint to endpoint.
    a,b=Vector(a),Vector(b)
    d=b-a
    center=(a+b)*.5
    bpy.ops.mesh.primitive_cone_add(vertices=vertices,radius1=r0,radius2=r1,depth=d.length,location=center)
    o=bpy.context.object
    o.name=name
    o.rotation_euler=d.to_track_quat("Z","Y").to_euler()
    return weight_mesh(o,bone,mat)

oval("Torso_Armoured",(0,0,1.48),(.35,.21,.33),"chest",uniform)
oval("UpperChestPlate",(0,-.17,1.55),(.25,.065,.18),"chest",pale)
oval("Abdomen",(0,0,1.20),(.28,.18,.19),"spine",uniform)
oval("PelvisShell",(0,0,1.04),(.31,.19,.19),"pelvis",uniform)
oval("NeckSeal",(0,0,1.72),(.13,.14,.105),"neck",strap)
oval("FacelessHead",(0,-.005,1.94),(.17,.165,.24),"head",mask,16,12)
oval("Faceplate",(0,-.142,1.94),(.125,.048,.15),"head",pale)
oval("VisorVoid",(0,-.184,1.97),(.106,.024,.08),"head",mask)
oval("SensorBand",(0,-.209,1.99),(.09,.010,.012),"head",light)
oval("ChestBadge",(0,-.247,1.53),(.049,.018,.029),"chest",light)
for side,sgn in (("l",1),("r",-1)):
    u="upperarm_"+side; l="lowerarm_"+side; hand="hand_"+side
    limb("UpperArmSleeve_"+side,(.34*sgn,0,1.55),(.54*sgn,0,1.21),.13,.11,u,uniform)
    oval("ElbowGuard_"+side,(.54*sgn,0,1.20),(.12,.12,.14),l,strap)
    limb("ForearmGuard_"+side,(.54*sgn,-.02,1.18),(.57*sgn,-.045,.78),.112,.082,l,pale)
    oval("GlovedHand_"+side,(.58*sgn,-.058,.68),(.086,.10,.16),hand,glove)
    limb("UpperLeg_"+side,(.19*sgn,0,1.04),(.22*sgn,-.025,.59),.17,.12,"thigh_"+side,uniform)
    oval("KneeShell_"+side,(.22*sgn,-.025,.57),(.126,.14,.12),"calf_"+side,pale)
    limb("Shin_"+side,(.22*sgn,-.025,.57),(.22*sgn,-.005,.13),.115,.095,"calf_"+side,uniform)
    oval("Boot_"+side,(.22*sgn,-.11,.075),(.145,.26,.083),"foot_"+side,glove)
    for i in range(3):
        z=1.38 + .065*i
        oval("SleeveBand_%s_%d"%(side,i),(.45*sgn,-.105,z),(.035,.014,.019),u,strap,8,6)
# Mesh joined but retains named vertex groups.
bpy.ops.object.select_all(action="DESELECT")
for p in pieces: p.select_set(True)
bpy.context.view_layer.objects.active=pieces[0]
bpy.ops.object.join()
skin=bpy.context.object
skin.name="SK_AH_ForestAnomaly"
skin.data.name="SK_AH_ForestAnomaly_Mesh"
skin.parent=rig
mod=skin.modifiers.new("ArmatureSkin","ARMATURE")
mod.object=rig
for p in skin.data.polygons:p.use_smooth=True
# Set transparent? No, opaque faceplate intentionally.
# ---------------------------------------------------------------------------
# Posing. Every rotation below is written about the CHARACTER's axes
# (forward = -Y, up = +Z, the anomaly's left = +X) and then converted into each
# bone's own frame. Bone-local euler angles depend on how Blender happened to
# roll each bone: on this rig a local-Z "turn" tilted the head and chest
# sideways, and the arm swing flapped outwards.
# ---------------------------------------------------------------------------
AX_SIDE = Vector((1, 0, 0))     # pitch axis: limbs swing forward/back about it
AX_FWD  = Vector((0, 1, 0))     # roll axis: tilts sideways about it
AX_UP   = Vector((0, 0, 1))     # yaw axis: turns left/right about it
REST = {b.name: b.matrix_local.to_quaternion() for b in data.bones}

def rot(axis, degrees):
    return Quaternion(axis, math.radians(degrees))

def forward(degrees):
    """A hanging limb swings forward (towards -Y) by this much; negative swings it back."""
    return rot(AX_SIDE, -degrees)

def outward(side, degrees):
    """A hanging limb swings away from the body (sideways) by this much."""
    return rot(AX_FWD, -degrees if side == "l" else degrees)

def turn(degrees):
    """Turn to the anomaly's left (positive) or right (negative) about the vertical."""
    return rot(AX_UP, degrees)

def tilt(degrees):
    """Tilt the top of the bone towards the anomaly's left (positive) or right."""
    return rot(AX_FWD, -degrees)

def to_bone_space(bone, q):
    # Pose basis that applies the character-axis rotation q to this bone,
    # relative to its (possibly already posed) parent.
    r = REST[bone]
    return r.inverted() @ q @ r

def reset_pose():
    for pb in rig.pose.bones:
        pb.rotation_mode = 'QUATERNION'
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.location = (0, 0, 0)

def action(name, frames, duration):
    reset_pose()
    rig.animation_data_create()
    act=bpy.data.actions.new("AH_FA_"+name)
    rig.animation_data.action=act
    for frame, rotations in frames:
        bpy.context.scene.frame_set(frame)
        reset_pose()
        for bone, q in rotations.items():
            pb=rig.pose.bones.get(bone)
            if pb:
                pb.rotation_quaternion=to_bone_space(bone, q)
        for pb in rig.pose.bones:
            pb.keyframe_insert(data_path="rotation_quaternion",frame=frame,group=pb.name)
    act.use_fake_user=True
    return act

def pose(leg=0, arm=0, head=0, torso=0, reach=0, raise_arms=0, head_tilt=0):
    """One key pose.

    leg        + = left leg forward, right leg back (degrees at the hip)
    arm        + = left arm back, right arm forward (arms swing against the legs)
    head/torso + = turn to the anomaly's left (yaw); the head turns on top of the chest
    reach      elbow bend, forearm swings forward (degrees)
    raise_arms both upper arms swing forward/up (degrees)
    head_tilt  + = head tilts towards its left shoulder
    """
    p = {}
    for side, sgn in (("l", 1), ("r", -1)):
        hip = leg * sgn                                   # + = this leg forward
        knee = 4 + max(hip, 0) * .55 + max(-hip, 0) * .25  # forward leg bends more
        p["thigh_"+side] = forward(hip)
        p["calf_"+side] = forward(-knee)                  # lower leg folds back
        p["foot_"+side] = forward(knee - hip)             # keep the sole level
        p["upperarm_"+side] = forward(-arm * sgn + raise_arms) @ outward(side, 4)
        p["lowerarm_"+side] = forward(reach)
    p["chest"] = turn(torso)
    p["head"] = turn(head) @ tilt(head_tilt)
    return p

clips={
 "Idle":(49,[(1,pose(reach=6)),(13,pose(head=4,torso=1,reach=7)),(25,pose(head=-3,torso=-1,reach=6)),(37,pose(head=4,torso=1,reach=7)),(49,pose(reach=6))]),
 "Walk":(33,[(1,pose(leg=24,arm=18,reach=12)),(9,pose(reach=14)),(17,pose(leg=-24,arm=-18,reach=12)),(25,pose(reach=14)),(33,pose(leg=24,arm=18,reach=12))]),
 "Run":(25,[(1,pose(leg=38,arm=35,reach=70,torso=-6)),(7,pose(reach=65)),(13,pose(leg=-38,arm=-35,reach=70,torso=6)),(19,pose(reach=65)),(25,pose(leg=38,arm=35,reach=70,torso=-6))]),
 "Turn":(33,[(1,pose(reach=6)),(13,pose(head=35,torso=18,reach=6)),(23,pose(head=-32,torso=-18,reach=6)),(33,pose(reach=6))]),
 "Alert":(41,[(1,pose(reach=6)),(8,pose(head=28,torso=10,raise_arms=12,reach=35)),(22,pose(head=24,torso=6,raise_arms=10,reach=40)),(41,pose(reach=6))]),
 "Investigate":(61,[(1,pose(reach=6)),(15,pose(head=18,torso=7,reach=12,head_tilt=14)),(30,pose(head=-20,torso=-6,reach=14,head_tilt=-16)),(45,pose(head=10,torso=3,reach=10,head_tilt=9)),(61,pose(reach=6))]),
 "Reach":(49,[(1,pose(reach=6)),(12,pose(raise_arms=40,reach=35,head=5)),(27,pose(raise_arms=62,reach=25,head=9)),(38,pose(raise_arms=35,reach=30)),(49,pose(reach=6))]),
}
actions={}
for name,(length,frames) in clips.items():
    actions[name]=action(name,frames,length)

def export(path,with_anim):
    bpy.ops.object.select_all(action="DESELECT")
    rig.select_set(True);skin.select_set(True)
    bpy.context.view_layer.objects.active=rig
    bpy.ops.export_scene.fbx(
        filepath=path,use_selection=True,object_types={'ARMATURE','MESH'},
        add_leaf_bones=False,apply_unit_scale=True,
        axis_forward='-Y',axis_up='Z',
        bake_anim=with_anim,bake_anim_use_all_bones=True,
        bake_anim_use_nla_strips=False,bake_anim_use_all_actions=False,
        bake_anim_simplify_factor=0.0,
        use_mesh_modifiers=False,
        path_mode='AUTO'
    )

rig.animation_data.action=None
bpy.context.scene.frame_set(1)
reset_pose()
export(os.path.join(OUT,"SK_AH_ForestAnomaly.fbx"),False)
for name,(length,frames) in clips.items():
    rig.animation_data.action=actions[name]
    bpy.context.scene.frame_start=1
    bpy.context.scene.frame_end=length
    bpy.context.scene.frame_set(1)
    export(os.path.join(OUT,"AN_AH_FA_"+name+".fbx"),True)
rig.animation_data.action=None
reset_pose()
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT,"AH_ForestAnomaly_Rigged.blend"))
print("Anomaly exported:",OUT, "actions:",list(clips))
