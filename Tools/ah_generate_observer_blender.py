"""
Anomaly Hunter | "The Observer" non-graphic humanoid anomaly
Blender 4.0+ generator, compatible with UE 5.8 FBX import.

Runs as a NEW BLENDER SCENE; any unsaved work in the open .blend will be lost.
It creates a procedural humanoid, individual skinned skeletal segments,
textured materials (shader-noise), independent rig controls, and seven animation
clips. Purposefully stylised, not a photogrammetry-grade character.

Run: Blender > Scripting > Open > ah_generate_observer_blender.py > Run Script.
Outputs Assets/ForestResearch/Enemy/Exports/ under this Git repository.

Animations: Idle, Walk, Run, Turn, Alert, Investigate, Reach, Retreat.
Each is authored in place and generated with a consistent exported skeleton.
This code is untested in Blender in the generation environment.
"""
import os, math, sys, random
import bpy
from mathutils import Vector
from math import sin, pi

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT,"Assets","ForestResearch","Enemy","Exports")
os.makedirs(OUT,exist_ok=True)
random.seed(89)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene
scene.render.engine="CYCLES"
scene.unit_settings.system='METRIC'
scene.unit_settings.scale_length=1.0
scene.frame_start=1
scene.frame_end=72
scene.render.resolution_x=1100
scene.render.resolution_y=1400
scene.render.resolution_percentage=100

def material(name,base,rough=0.7,metal=0.0,noise=0.0,emission=0):
    mat=bpy.data.materials.new(name)
    mat.diffuse_color=(*base,1)
    mat.use_nodes=True
    nt=mat.node_tree
    principled=nt.nodes.get("Principled BSDF")
    principled.inputs["Base Color"].default_value=(*base,1)
    principled.inputs["Roughness"].default_value=rough
    principled.inputs["Metallic"].default_value=metal
    if emission>0:
        principled.inputs["Emission Color"].default_value=(*base,1)
        principled.inputs["Emission Strength"].default_value=emission
    if noise>0:
        tex=nt.nodes.new("ShaderNodeTexNoise")
        tex.inputs["Scale"].default_value=55
        tex.inputs["Detail"].default_value=3.5
        bump=nt.nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value=noise
        bump.inputs["Distance"].default_value=0.009
        nt.links.new(tex.outputs["Fac"],bump.inputs["Height"])
        nt.links.new(bump.outputs["Normal"],principled.inputs["Normal"])
    return mat

fabric=material("M_Observer_DustyFabric",(0.11,0.13,0.13),.95,0,.15)
coat=material("M_Observer_WeatheredCoat",(0.24,0.25,0.22),.95,0,.2)
undersuit=material("M_Observer_DarkUndersuit",(0.055,0.062,0.069),.88,0,.11)
skin=material("M_Observer_PaleMask",(0.53,0.56,0.52),.82,0,.09)
ceramic=material("M_Observer_SensorCeramic",(0.12,0.15,0.17),.61,.22,.09)
black=material("M_Observer_Shadow",(0.013,.019,.021),.85,0,.06)
amber=material("M_Observer_AmberTrace",(.95,.34,.07),.42,.05,0,1.25)
cold=material("M_Observer_CyanTrace",(.04,.55,.62),.50,0,0,.55)

# Rig: Z-up, Y-forward towards -Y, X positive for observer's left arm.
bpy.ops.object.armature_add(enter_editmode=False,location=(0,0,0))
arm=bpy.context.object
arm.name="SKEL_AH_Observer"
bones={}
bpy.ops.object.mode_set(mode="EDIT")
for bone in list(arm.data.edit_bones):arm.data.edit_bones.remove(bone)
def bone(name,head,tail,parent=None):
    b=arm.data.edit_bones.new(name)
    b.head=Vector(head); b.tail=Vector(tail)
    if parent:b.parent=arm.data.edit_bones[parent]
    bones[name]=(Vector(head),Vector(tail))
    return b
bone("root",(0,0,0),(0,0,.18))
bone("pelvis",(0,0,1.10),(0,0,1.24),"root")
bone("spine_01",(0,0,1.24),(0,0,1.52),"pelvis")
bone("spine_02",(0,0,1.52),(0,0,1.82),"spine_01")
bone("neck",(0,0,1.82),(0,-.015,2.02),"spine_02")
bone("head",(0,-.015,2.02),(0,-.035,2.35),"neck")
for suffix,sgn in (("l",1),("r",-1)):
    bone("clavicle_"+suffix,(0,0,1.77),(.22*sgn,0,1.79),"spine_02")
    bone("upperarm_"+suffix,(.22*sgn,0,1.79),(.37*sgn,-.03,1.28),"clavicle_"+suffix)
    bone("lowerarm_"+suffix,(.37*sgn,-.03,1.28),(.38*sgn,-.07,.88),"upperarm_"+suffix)
    bone("hand_"+suffix,(.38*sgn,-.07,.88),(.40*sgn,-.11,.72),"lowerarm_"+suffix)
    bone("thigh_"+suffix,(.15*sgn,0,1.10),(.19*sgn,.04,.59),"pelvis")
    bone("shin_"+suffix,(.19*sgn,.04,.59),(.21*sgn,-.08,.13),"thigh_"+suffix)
    bone("foot_"+suffix,(.21*sgn,-.08,.13),(.21*sgn,-.27,.095),"shin_"+suffix)
bpy.ops.object.mode_set(mode="OBJECT")
for b in arm.data.bones: b.use_deform=True
arm.show_in_front=True

parts=[]
def attach(obj,name,bone_name,mat):
    obj.name=name
    obj.data.materials.clear()
    obj.data.materials.append(mat)
    # Each segment has its own 100% deform influence; independent bone motion
    # is correct but joint deformations remain rigid prototype quality.
    bpy.context.view_layer.objects.active=obj
    for o in bpy.context.selected_objects:o.select_set(False)
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    group=obj.vertex_groups.new(name=bone_name)
    group.add(list(range(len(obj.data.vertices))),1.0,'REPLACE')
    mod=obj.modifiers.new("Skeleton","ARMATURE")
    mod.object=arm
    parts.append(obj)
    return obj

def sphere(name,at,scale,bone_name,mat,segments=24,rings=14):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments,ring_count=rings,radius=1, location=at)
    obj=bpy.context.object;obj.scale=scale
    return attach(obj,name,bone_name,mat)

def capsule(name,start,end,rad1,rad2,bone_name,mat,vertices=20):
    # A smooth, tapered cylindrical shell; two end caps are rounded separately.
    a=Vector(start); b=Vector(end)
    delta=b-a; center=(a+b)*.5
    bpy.ops.mesh.primitive_cone_add(vertices=vertices,radius1=rad1,radius2=rad2,
         depth=delta.length, location=center)
    obj=bpy.context.object
    obj.rotation_euler=Vector((0,0,1)).rotation_difference(delta).to_euler()
    attach(obj,name,bone_name,mat)
    return obj

def limb(name,b,thick,mat,distal=.8):
    h,t=bones[b]
    capsule(name,h,t,thick,thick*distal,b,mat)
    sphere(name+"_JointBase",h,(thick,thick,thick*.8),b,mat,16,10)
    sphere(name+"_JointEnd",t,(thick*distal,thick*distal,thick*distal*.95),b,mat,16,10)

# Torso and head: asymmetrical silhouette to feel uncanny, but no graphic content.
sphere("Torso_PaddedUpper",(0,0,1.60),(.32,.18,.34),"spine_02",coat,40,24)
sphere("Torso_Undercoat",(0,0,1.37),(.25,.15,.23),"spine_01",fabric,32,18)
sphere("Pelvis_Plate",(0,0,1.13),(.25,.17,.16),"pelvis",undersuit)
sphere("HighCollar",(0,0,1.85),(.18,.17,.11),"neck",black)
limb("NeckFabric","neck",.082,fabric)
sphere("Head_Mask",(0,-.042,2.17),(.149,.135,.22),"head",skin,40,24)
sphere("Head_UpperHood",(0,.044,2.23),(.161,.13,.19),"head",coat,32,20)
sphere("Face_Visor",(0,-.158,2.17),(.126,.038,.09),"head",black,32,12)
for sgn in [-1,1]:
    sphere("EyeTrace_%d"%sgn,(sgn*.060,-.189,2.18),(.036,.012,.015),"head",amber)
    sphere("TemplePlate_%d"%sgn,(sgn*.14,-.010,2.22),(.035,.094,.13),"head",ceramic)
sphere("MouthFilter",(0,-.182,2.085),(.067,.023,.038),"head",ceramic)
for i,z in enumerate((2.064,2.082,2.100)):
    sphere("MaskVent_%d"%i,(0,-.206,z),(.045,.002,.003),"head",black,12,8)
for suffix,sgn in (("l",1),("r",-1)):
    limb("UpperArm_"+suffix,"upperarm_"+suffix,.107,coat,.75)
    limb("Forearm_"+suffix,"lowerarm_"+suffix,.082,fabric,.72)
    sphere("Glove_"+suffix,(sgn*.39,-.10,.80),(.066,.084,.097),"hand_"+suffix,black)
    for i in range(4):
        dx=(i-1.5)*.022
        h=Vector((sgn*(.39+dx),-.112,.763))
        t=h+Vector((0,-.03,-.082))
        capsule("Digit_%s_%d"%(suffix,i),h,t,.010,.008,"hand_"+suffix,black,10)
    sphere("ShoulderPatch_"+suffix,(sgn*.25,-.112,1.76),(.075,.023,.09),"clavicle_"+suffix,ceramic)
    sphere("ElbowGuard_"+suffix,(sgn*.38,-.045,1.30),(.09,.094,.08),"lowerarm_"+suffix,ceramic)
    capsule("ForearmTrace_"+suffix,(sgn*.39,-.122,1.16),(sgn*.40,-.13,.96),.009,.010,
            "lowerarm_"+suffix,cold,10)
    limb("UpperLeg_"+suffix,"thigh_"+suffix,.139,coat,.77)
    limb("LowerLeg_"+suffix,"shin_"+suffix,.105,undersuit,.70)
    sphere("KneeGuard_"+suffix,(sgn*.194,-.045,.60),(.116,.10,.11),"shin_"+suffix,ceramic)
    sphere("Boot_"+suffix,(sgn*.21,-.17,.13),(.125,.217,.09),"foot_"+suffix,black,24,14)
    for z in (.75,.84,.96):
        sphere("LegStrap_%s_%.2f"%(suffix,z),(sgn*.185,-.13,z),(.12,.022,.021),
            "thigh_"+suffix,fabric,12,8)

# Do not join after parenting, as vertex groups already track each individual segment.
# FBX exports multiple skinned meshes bound to the same armature (import with Combine Meshes).
for o in bpy.context.selected_objects:o.select_set(False)
arm.select_set(True)
bpy.context.view_layer.objects.active=arm
for o in parts:o.select_set(True)

def pose_zero():
    for pb in arm.pose.bones:
        pb.rotation_mode="XYZ"
        pb.rotation_euler=(0,0,0)

def k(pb, frame, xyz):
    pb.rotation_euler=xyz
    pb.keyframe_insert(data_path="rotation_euler",frame=frame,group=pb.name)

def anim_pose(frame, style, p):
    pose_zero()
    ang=2*pi*p
    def set_b(n, xyz):
        if n in arm.pose.bones:
            k(arm.pose.bones[n],frame,xyz)
    breath=.018*sin(ang)
    set_b("spine_01",(breath,0,.006*sin(ang)))
    set_b("spine_02",(breath*1.2,.009*sin(ang),0))
    set_b("head",(.018*sin(ang+.6),.026*sin(ang*.4),.02*sin(ang+.1)))
    if style in ("Walk","Run"):
        power=.27 if style=="Walk" else .48
        set_b("pelvis",(.015*sin(ang),0,.035*sin(ang)))
        for side,phase in (("l",0),("r",pi)):
            t=ang+phase
            set_b("thigh_"+side,(power*sin(t),0,0))
            set_b("shin_"+side,(max(0,-sin(t))*.40*power/.27,0,0))
            set_b("upperarm_"+side,(-power*.78*sin(t),0,0))
            set_b("lowerarm_"+side,(-.12-abs(sin(t))*.26,0,0))
            set_b("foot_"+side,(-.11*sin(t),0,0))
    elif style=="Turn":
        set_b("pelvis",(0,0,.12*sin(ang)))
        set_b("spine_01",(0,0,.16*sin(ang)))
        set_b("spine_02",(0,0,.22*sin(ang)))
        set_b("head",(0,0,.3*sin(ang)))
    elif style=="Alert":
        strength=(1-math.cos(ang))*.5
        set_b("head",(-.13*strength,.08*strength,-.19*strength))
        for side in ("l","r"):
            set_b("upperarm_"+side,(-.25*strength,0,0))
            set_b("lowerarm_"+side,(-.26*strength,0,0))
    elif style=="Investigate":
        strength=(1-math.cos(ang))*.5
        set_b("spine_02",(-.13*strength,0,.16*strength))
        set_b("head",(-.09*strength,0,-.19*strength))
        set_b("upperarm_r",(-.20*strength,0,0))
    elif style=="Reach":
        strength=(1-math.cos(ang))*.5
        set_b("clavicle_l",(-.11*strength,0,0))
        set_b("upperarm_l",(-.92*strength,0,-.16*strength))
        set_b("lowerarm_l",(-.30*strength,0,0))
        set_b("hand_l",(-.12*strength,0,0))
    elif style=="Retreat":
        strength=(1-math.cos(ang))*.5
        set_b("pelvis",(.12*strength,0,0))
        set_b("spine_01",(.12*strength,0,0))
        set_b("head",(.04*strength,0,0))
        set_b("upperarm_l",(-.18*strength,0,0))
        set_b("upperarm_r",(-.18*strength,0,0))
    else:
        for side in ("l","r"):
            set_b("upperarm_"+side,(-.03*sin(ang),0,0))
            set_b("lowerarm_"+side,(-.08-.04*sin(ang),0,0))

def select_for_export():
    bpy.ops.object.mode_set(mode="OBJECT")
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    for o in parts:o.select_set(True)
    bpy.context.view_layer.objects.active=arm

def export_fbx(path, animated=False):
    select_for_export()
    bpy.ops.export_scene.fbx(
        filepath=path, use_selection=True, object_types={'ARMATURE','MESH'},
        apply_unit_scale=True, add_leaf_bones=False, use_armature_deform_only=True,
        axis_forward='-Y', axis_up='Z', bake_anim=animated,
        bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False,
        bake_anim_step=1.0, bake_anim_simplify_factor=0.0,
        mesh_smooth_type='FACE', path_mode='AUTO')
    print("Exported:",path)

pose_zero()
export_fbx(os.path.join(OUT,"SK_AH_Observer.fbx"),False)

for name,frames,cycles in (
    ("Idle",72,1),("Walk",32,1),("Run",22,1),
    ("Turn",54,1),("Alert",60,1),("Investigate",72,1),
    ("Reach",60,1),("Retreat",52,1)
):
    action=bpy.data.actions.new("AH_Observer_"+name)
    arm.animation_data_create()
    arm.animation_data.action=action
    for frame in range(1,frames+1,2):
        anim_pose(frame,name,(frame-1)/(frames-1))
    scene.frame_start=1
    scene.frame_end=frames
    scene.frame_set(1)
    export_fbx(os.path.join(OUT,"AN_AH_Observer_"+name+".fbx"),True)

scene.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT,"AH_Observer_Rigged.blend"))
print("Observer rig and eight FBXs written to:",OUT)
print("NOTE: visual prototype requires local Blender and UE deformation/animation QA.")
