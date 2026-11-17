"""Anomaly Hunter Operator FP v0.4: independently animated arms for Blender 4.2+/5.x.

NON-DESTRUCTIVE to the original v0.3 pack and project files.

Usage:
  1. Open the previously generated AnomalyHunter_ContainmentOperator_Rigged.blend.
  2. In Blender > Scripting, open this file and click Run Script.
  3. A NEW Blender file and five animation FBXs are written under Exports/Operator_v04.

Each arm gets a separate hand IK effector and elbow pole. Elbow, forearm,
wrist and finger poses are baked into regular bones so Unreal Engine can import
animation sequences without depending on Blender IK constraints.

NOTE: This is a procedural first pass. Blender is needed to evaluate/bake the
rig and confirm joint orientations; this generator has not been run in Blender
in the author's environment.
"""
import bpy
import math
import os
from mathutils import Vector

ARM_NAME = 'SK_AH_ContainmentOperator_FP_Armature'  # legacy v0.3 exporter name
MESH_NAME = 'SK_AH_ContainmentOperator_FP'
BLEND_BASE = bpy.path.abspath('//')
if not bpy.data.filepath:
    raise RuntimeError('Open your existing v0.3 RIGGED .blend file before running this script.')
OUT_DIR = os.path.join(BLEND_BASE, 'Operator_v04')
os.makedirs(OUT_DIR, exist_ok=True)

skin = bpy.data.objects.get(MESH_NAME)
arm = bpy.data.objects.get(ARM_NAME) or bpy.data.objects.get('Armature')
if skin and not arm:
    arm = skin.parent if skin.parent and skin.parent.type == 'ARMATURE' else None
if skin and not arm:
    arm = next((m.object for m in skin.modifiers if m.type == 'ARMATURE' and m.object), None)
if not arm or arm.type != 'ARMATURE' or not skin or skin.type != 'MESH':
    raise RuntimeError('Expected the v0.3 rigged scene with armature and skeletal mesh; see README.')

required = ['root']
for s in ('l','r'):
    required += ['clavicle_'+s,'upperarm_'+s,'lowerarm_'+s,'forearm_twist_'+s,'wrist_'+s,'hand_'+s]
    for d in ('thumb','index','middle','ring','pinky'):
        required += [d+'_0'+str(i)+'_'+s for i in (1,2,3)]
missing=[b for b in required if b not in arm.pose.bones]
if missing:
    raise RuntimeError('Missing bones in v0.3 rig: '+', '.join(missing))

ctrl_col = bpy.data.collections.get('AH_FP_IK_Controls')
if not ctrl_col:
    ctrl_col = bpy.data.collections.new('AH_FP_IK_Controls')
    bpy.context.scene.collection.children.link(ctrl_col)

def empty(name, position, size=0.045):
    o=bpy.data.objects.get(name)
    if not o:
        o=bpy.data.objects.new(name,None)
        ctrl_col.objects.link(o)
    o.empty_display_type='SPHERE'
    o.empty_display_size=size
    o.location=position
    return o

rest_targets={}
controls=[]
for side,sign in (('l',-1),('r',1)):
    wrist_local=Vector((sign*.125,-.735,-.105))
    pole_local=Vector((sign*.38,-.41,-.24))
    target=empty('CTRL_AH_Hand_'+side,arm.matrix_world@wrist_local)
    pole=empty('CTRL_AH_Elbow_'+side,arm.matrix_world@pole_local)
    c=arm.pose.bones['forearm_twist_'+side].constraints.get('AH_Arm_IK')
    if not c:
        c=arm.pose.bones['forearm_twist_'+side].constraints.new('IK')
        c.name='AH_Arm_IK'
    c.target=target
    c.pole_target=pole
    c.chain_count=3  # forearm_twist <- lowerarm <- upperarm
    c.use_stretch=False
    c.pole_angle=0.0  # may require adjustment for a custom bone roll
    rest_targets[side]=wrist_local
    controls.extend([target,pole])
    # Give wrist/hand their own independent motion after the IK chain.
    for nm in ('wrist_'+side,'hand_'+side):
        arm.pose.bones[nm].rotation_mode='XYZ'

scene=bpy.context.scene
scene.render.fps=30

# Each clip uses separate left/right target motion, with asymmetrical natural variation.
# Template clip lengths loop by returning to the starting pose.
clips={
    'AH_FP_Idle': (72,'idle'),
    'AH_FP_Walk': (36,'walk'),
    'AH_FP_Run': (28,'run'),
    'AH_FP_Crouch': (72,'crouch'),
    'AH_FP_Inspect': (75,'inspect'),
}

def hand_offset(typ,t,side):
    phase = 0 if side=='l' else math.pi
    if typ=='idle':
        return Vector((.008*math.sin(2*math.pi*t+phase), .008*math.sin(2*math.pi*t+phase+.7), .007*math.cos(2*math.pi*t+.8+phase)))
    if typ=='walk':
        return Vector((.016*math.sin(2*math.pi*t+phase), .050*math.sin(2*math.pi*t+phase), .020*math.cos(4*math.pi*t+phase)))
    if typ=='run':
        return Vector((.035*math.sin(2*math.pi*t+phase), .105*math.sin(2*math.pi*t+phase), .035*math.cos(2*math.pi*t+phase)))
    if typ=='crouch':
        return Vector((-.015 if side=='r' else .015,.02,-.048+.006*math.sin(2*math.pi*t+phase)))
    if typ=='inspect':
        # Bring one hand forwards for a non-weapon investigative gesture.
        easing=(1-math.cos(2*math.pi*t))/2
        return Vector(((.028 if side=='l' else -.018)*easing, -.11*easing if side=='l' else .02*easing,
                       .07*easing if side=='l' else -.008*easing))
    return Vector((0,0,0))

def angle(typ,t,side,digit):
    ph=0 if side=='l' else 0.4
    base={'thumb':.18,'index':.16,'middle':.20,'ring':.27,'pinky':.33}[digit]
    if typ=='run': base+=.14
    if typ=='crouch':base+=.10
    if typ=='inspect': base -= .09*(1-math.cos(2*math.pi*t))/2
    return base + .07*math.sin(2*math.pi*t+ph)

def reset_pose():
    if arm.animation_data:
        arm.animation_data_clear()
    for p in arm.pose.bones:
        p.rotation_mode='XYZ'
        p.location=(0,0,0)
        p.rotation_euler=(0,0,0)
        p.scale=(1,1,1)
    for o in controls:
        if o.animation_data:
            o.animation_data_clear()

# Save an IK editable file before exporting. It also contains the targets and
# independent elbow controls for further animator edits.
for side,sign in (('l',-1),('r',1)):
    bpy.data.objects['CTRL_AH_Hand_'+side].location=arm.matrix_world@rest_targets[side]
editable=os.path.join(OUT_DIR,'AH_Operator_FP_v04_IK_Controls.blend')
bpy.ops.wm.save_as_mainfile(filepath=editable)

for name,(frames,kind) in clips.items():
    reset_pose()
    scene.frame_start=1
    scene.frame_end=frames
    # Cache transforms for independent left/right control motion.
    # Bake every frame to avoid losing movement detail when exporting.
    for f in range(1,frames+1):
        t=(f-1)/max(1,frames-1)
        scene.frame_set(f)
        for side in ('l','r'):
            target=bpy.data.objects['CTRL_AH_Hand_'+side]
            target.location=arm.matrix_world@(rest_targets[side]+hand_offset(kind,t,side))
            target.keyframe_insert(data_path='location',frame=f)
            wrist=arm.pose.bones['wrist_'+side]
            wrist.rotation_euler=(.055*math.sin(2*math.pi*t), .065*math.sin(2*math.pi*t+(0 if side=='l' else math.pi)),0)
            wrist.keyframe_insert(data_path='rotation_euler',frame=f)
            for digit in ('thumb','index','middle','ring','pinky'):
                curl=angle(kind,t,side,digit)
                for idx,weight in ((1,.65),(2,.9),(3,.65)):
                    b=arm.pose.bones[f'{digit}_0{idx}_{side}']
                    b.rotation_euler=(curl*weight,0,0)
                    b.keyframe_insert(data_path='rotation_euler',frame=f)
    # Bake the evaluated IK motion to all deform bones, then disable IK for FBX
    # so the exported action is standalone and uses normal skeletal keys.
    bpy.ops.object.select_all(action='DESELECT')
    arm.select_set(True)
    bpy.context.view_layer.objects.active=arm
    bpy.ops.object.mode_set(mode='POSE')
    bpy.ops.pose.select_all(action='SELECT')
    bpy.ops.nla.bake(frame_start=1,frame_end=frames,step=1,only_selected=False,
                     visual_keying=True,clear_constraints=False,clear_parents=False,
                     use_current_action=False,bake_types={'POSE'})
    bpy.ops.object.mode_set(mode='OBJECT')
    if not arm.animation_data or not arm.animation_data.action:
        raise RuntimeError('Could not bake animation '+name)
    arm.animation_data.action.name=name
    for s in ('l','r'):
        arm.pose.bones['forearm_twist_'+s].constraints['AH_Arm_IK'].mute=True
    scene.frame_set(1)
    bpy.ops.object.select_all(action='DESELECT')
    arm.select_set(True)
    skin.select_set(True)
    bpy.context.view_layer.objects.active=arm
    outfile=os.path.join(OUT_DIR,name+'.fbx')
    bpy.ops.export_scene.fbx(filepath=outfile,use_selection=True,
        object_types={'ARMATURE','MESH'},axis_forward='-Y',axis_up='Z',
        add_leaf_bones=False,bake_anim=True,bake_anim_use_all_actions=False,
        bake_anim_use_nla_strips=False,bake_anim_step=1.0,
        apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS')
    for s in ('l','r'):
        arm.pose.bones['forearm_twist_'+s].constraints['AH_Arm_IK'].mute=False
    print(name,'=>',outfile)

# Revert to editable rig controls, not the last baked clip.
reset_pose()
scene.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=editable)
print('Operator v0.4 generated:',OUT_DIR)
print('Five animation FBXs + editable IK controls .blend. Test deformations before importing to Unreal.')
