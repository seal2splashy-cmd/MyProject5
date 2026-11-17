"""Create a stylised, non-graphic faceless research anomaly for Unreal 5.8.

Run in Blender 4.x / 5.x with the Scripting workspace.
Generates an armature-skinned character and seven simple in-place action clips.
Exports into <script folder>/Exports/ForestAnomaly.
Only a procedural first-pass model; not a photorealistic final character.
"""
import os
import math
from mathutils import Vector
import bpy

ROOT = os.path.dirname(os.path.abspath(__file__))
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
def reset_pose():
    for pb in rig.pose.bones:
        pb.rotation_mode='XYZ'
        pb.rotation_euler=(0,0,0)

def action(name, frames, duration):
    reset_pose()
    rig.animation_data_create()
    act=bpy.data.actions.new("AH_FA_"+name)
    rig.animation_data.action=act
    for frame, rotations in frames:
        bpy.context.scene.frame_set(frame)
        reset_pose()
        for bone, angles in rotations.items():
            pb=rig.pose.bones.get(bone)
            if pb:
                pb.rotation_euler=tuple(math.radians(v) for v in angles)
        for pb in rig.pose.bones:
            pb.keyframe_insert(data_path="rotation_euler",frame=frame,group=pb.name)
    act.use_fake_user=True
    return act

def pose(leg=0,arm=0,head=0,torso=0,reach=0):
    return {
        "thigh_l":(leg,0,0),"thigh_r":(-leg,0,0),
        "calf_l":(-max(leg,0)*.35,0,0),"calf_r":(-max(-leg,0)*.35,0,0),
        "upperarm_l":(-arm,0,-3),"upperarm_r":(arm,0,3),
        "lowerarm_l":(reach,0,-3),"lowerarm_r":(reach,0,3),
        "head":(0,0,head),"chest":(0,0,torso)
    }
clips={
 "Idle":(49,[(1,pose()),(13,pose(head=3,torso=1)),(25,pose(head=-2,torso=-1)),(37,pose(head=3,torso=1)),(49,pose())]),
 "Walk":(33,[(1,pose(leg=24,arm=18)),(9,pose(leg=0,arm=0)),(17,pose(leg=-24,arm=-18)),(25,pose(leg=0,arm=0)),(33,pose(leg=24,arm=18))]),
 "Run":(25,[(1,pose(leg=38,arm=33,reach=20)),(7,pose(leg=0,arm=0,reach=18)),(13,pose(leg=-38,arm=-33,reach=20)),(19,pose(leg=0,arm=0,reach=18)),(25,pose(leg=38,arm=33,reach=20))]),
 "Turn":(33,[(1,pose()),(13,pose(head=15,torso=12)),(23,pose(head=-14,torso=-12)),(33,pose())]),
 "Alert":(41,[(1,pose()),(8,pose(head=19,torso=8,reach=20)),(22,pose(head=16,torso=4,reach=25)),(41,pose())]),
 "Investigate":(61,[(1,pose()),(15,pose(head=12,torso=7,reach=8)),(30,pose(head=-15,torso=-6,reach=10)),(45,pose(head=8,torso=3,reach=6)),(61,pose())]),
 "Reach":(49,[(1,pose()),(12,pose(reach=45,head=5)),(27,pose(reach=60,head=9)),(38,pose(reach=35)),(49,pose())]),
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
