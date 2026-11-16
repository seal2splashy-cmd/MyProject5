"""Prototype of the C++ UOperatorArmsComponent IK, run on the rigged .blend, rendered from the FP camera.
Coordinates: Blender authoring frame (forward -Y, up +Z, right -X). Camera-space vectors use the UE
convention (x forward, y right, z up) in cm, converted here."""
import bpy, sys, math, os
from mathutils import Vector, Quaternion, Matrix

blend, outdir, scenario = sys.argv[sys.argv.index("--")+1:][:3]
bpy.ops.wm.open_mainfile(filepath=blend)
arm = bpy.data.objects["Armature"]; skin = bpy.data.objects["SK_AH_ContainmentOperator_FP"]
bones = arm.data.bones
SCALE = 0.75                          # ArmsScale
SHOULDER_OFFSET = Vector((-15, 0, -25))   # camera-space cm (fwd, right, up)

FWD, RIGHT, UP = Vector((0, -1, 0)), Vector((-1, 0, 0)), Vector((0, 0, 1))
def head(n): return bones[n].head_local.copy()
def restq(n): return bones[n].matrix_local.to_quaternion()

S_l, S_r = head("upperarm_l"), head("upperarm_r")
shoulder_mid = (S_l + S_r) * 0.5
# camera sits SHOULDER_OFFSET behind/below... i.e. shoulders are at SHOULDER_OFFSET in camera space
cam_pos = shoulder_mid - (FWD * SHOULDER_OFFSET.x + RIGHT * SHOULDER_OFFSET.y + UP * SHOULDER_OFFSET.z) * (0.01 / SCALE)
def cam_to_world(v_cm):  # camera-space cm -> mesh space
    return cam_pos + (FWD * v_cm.x + RIGHT * v_cm.y + UP * v_cm.z) * (0.01 / SCALE)
def cam_dir(v):
    return (FWD * v.x + RIGHT * v.y + UP * v.z).normalized()

# mesh-derived basis (C++ does the same from bone positions)
F_axis = sum(((head("wrist_" + s) - head("upperarm_" + s)) for s in "lr"), Vector()).normalized()
R_axis = (S_r - S_l).normalized()
DOWN = F_axis.cross(R_axis).normalized()      # Blender is right-handed: up = R x F, so down = F x R
assert DOWN.z < -0.9, DOWN

def basis_quat(x, y):
    x = x.normalized(); y = (y - x * x.dot(y)).normalized(); z = x.cross(y)
    return Matrix((x, y, z)).transposed().to_quaternion()

def swing_twist(q, axis):
    p = Vector((q.x, q.y, q.z)); proj = axis * p.dot(axis)
    tw = Quaternion((q.w, proj.x, proj.y, proj.z))
    if tw.magnitude < 1e-6: return Quaternion()
    tw.normalize(); return tw

new_rot, new_head = {}, {}
def solve(side, grip, A, curl, index_curl=None, pole_cam=Vector((-0.3, 0.6, -1.0))):
    s = side
    S, E0, M0, W0 = head("upperarm_"+s), head("lowerarm_"+s), head("forearm_twist_"+s), head("wrist_"+s)
    H0, Mid1 = head("hand_"+s), head("middle_01_"+s)
    Idx1, Pky1 = head("index_01_"+s), head("pinky_01_"+s)
    L1, L2 = (E0 - S).length, (W0 - E0).length
    palm_len = (Mid1 - H0).length
    grip_rest = H0.lerp(Mid1, 0.85) + DOWN * (0.35 * palm_len)

    hint = (grip - S).normalized()
    Fh1 = (hint - A * hint.dot(A))
    Fh1 = Fh1.normalized() if Fh1.length > 1e-4 else F_axis
    Q0 = basis_quat(Mid1 - H0, Pky1 - Idx1)
    Q1 = basis_quat(Fh1, -A)
    QH = Q1 @ Q0.inverted()

    W = grip - QH @ (grip_rest - W0)
    D = W - S; d = max(abs(L1 - L2) + 1e-3, min(D.length, (L1 + L2) * 0.999)); dirv = D.normalized(); W = S + dirv * d
    a = (L1*L1 - L2*L2 + d*d) / (2*d); h = math.sqrt(max(L1*L1 - a*a, 0))
    pole = cam_dir(Vector((pole_cam.x, pole_cam.y if s == "r" else -pole_cam.y, pole_cam.z)))
    pd = (pole - dirv * pole.dot(dirv)).normalized()
    E = S + dirv * a + pd * h

    Qu = (E0 - S).rotation_difference(E - S)
    Qf = (W0 - E0).rotation_difference(W - E)
    tw_axis = (W0 - M0).normalized()
    twist = swing_twist(Qf.inverted() @ QH, tw_axis)
    Qt = Qf @ Quaternion().slerp(twist, 0.6)

    new_rot["upperarm_"+s] = Qu @ restq("upperarm_"+s)
    new_rot["lowerarm_"+s] = Qf @ restq("lowerarm_"+s)
    new_rot["forearm_twist_"+s] = Qt @ restq("forearm_twist_"+s)
    new_rot["wrist_"+s] = QH @ restq("wrist_"+s)
    new_rot["hand_"+s] = QH @ restq("hand_"+s)
    J = (math.radians(55), math.radians(70), math.radians(45))
    for digit in ("index", "middle", "ring", "pinky", "thumb"):
        c = curl
        if digit == "index" and index_curl is not None: c = index_curl
        b = [f"{digit}_0{i}_{s}" for i in (1, 2, 3)]
        d1 = (head(b[1]) - head(b[0])).normalized()
        if digit == "thumb":
            to_centre = H0.lerp(Mid1, 0.6) - head(b[0]); to_centre -= d1 * to_centre.dot(d1)
            ax_op = d1.cross(to_centre).normalized()
            ax = d1.cross(DOWN).normalized()
            cum = 0.0
            for i, n in enumerate(b):
                cum += J[i] * 0.45 * c
                q = Quaternion(ax, cum)
                if i == 0: q = Quaternion(ax_op, math.radians(35) * c) @ q
                else: q = Quaternion(ax_op, math.radians(35) * c) @ q
                new_rot[n] = QH @ q @ restq(n)
        else:
            ax = d1.cross(DOWN).normalized()
            cum = 0.0
            for i, n in enumerate(b):
                cum += J[i] * c
                new_rot[n] = QH @ Quaternion(ax, cum) @ restq(n)
    return E, W

def apply_pose():
    # FK positions the way UPoseableMeshComponent does it (local translations preserved)
    order = []
    def walk(b):
        order.append(b)
        for c in b.children: walk(c)
    for b in bones:
        if b.parent is None: walk(b)
    delta = {}
    for b in order:
        rq = restq(b.name)
        nr = new_rot.get(b.name, None)
        if b.parent is None:
            hpos = b.head_local.copy(); nr = nr or rq
        else:
            p = b.parent
            hpos = new_head[p.name] + delta[p.name] @ (b.head_local - p.head_local)
            if nr is None: nr = delta[p.name] @ rq
        new_head[b.name] = hpos
        delta[b.name] = nr @ rq.inverted()
        pb = arm.pose.bones[b.name]
        pb.matrix = Matrix.LocRotScale(hpos, nr, Vector((1, 1, 1)))
        bpy.context.view_layer.update()

def add_cyl(name, base, axis, length, radius, color):
    bpy.ops.mesh.primitive_cylinder_add(radius=radius, depth=length, location=base + axis * (length / 2))
    o = bpy.context.object; o.name = name
    o.rotation_mode = 'QUATERNION'; o.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(axis)
    m = bpy.data.materials.new(name + "_m"); m.diffuse_color = color
    m.use_nodes = True; m.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = color
    o.data.materials.append(m); return o

def add_box(name, centre, axis, size, color):
    bpy.ops.mesh.primitive_cube_add(size=1, location=centre)
    o = bpy.context.object; o.name = name; o.scale = size
    o.rotation_mode = 'QUATERNION'; o.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(axis)
    m = bpy.data.materials.new(name + "_m")
    m.use_nodes = True; m.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = color
    o.data.materials.append(m); return o

if scenario.startswith("pipe"):
    grip_cam = Vector((45, 22, -28))
    A = cam_dir(Vector((0.45, 0.12, 0.88)))
    g = cam_to_world(grip_cam)
    solve("r", g + A * (12 * 0.01 / SCALE), A, 1.0)
    solve("l", g + A * (2 * 0.01 / SCALE), A, 1.0)
    add_cyl("Pipe", g - A * (6 * 0.01 / SCALE), A, 90 * 0.01 / SCALE, 2.0 * 0.01 / SCALE, (0.25, 0.12, 0.05, 1))
elif scenario.startswith("pistol"):
    grip_cam = Vector((35, 9, -18)) if scenario.startswith("pistol2") else Vector((22, 4, -15))
    A = cam_dir(Vector((0.25, 0.0, 1.0)))
    g = cam_to_world(grip_cam)
    solve("r", g, A, 1.0, index_curl=0.45)
    solve("l", cam_to_world(grip_cam + Vector((1, -3, -1))), A, 1.0)
    add_box("Grip", g, A, Vector((2.8, 1.2, 11)) * (0.01 / SCALE), (0.1, 0.1, 0.1, 1))
    add_box("Slide", cam_to_world(grip_cam + Vector((5, 0, 6.5))), cam_dir(Vector((1, 0, 0))) , Vector((2.6, 3, 18)) * (0.01 / SCALE), (0.2, 0.2, 0.22, 1))
elif scenario.startswith("rest"):
    pass
apply_pose()
arm.hide_render = True

# camera + lights
cam_data = bpy.data.cameras.new("Cam"); cam_data.lens_unit = 'FOV'; cam_data.angle = math.radians(90)
cam_data.clip_start = 0.005
cam = bpy.data.objects.new("Cam", cam_data); bpy.context.scene.collection.objects.link(cam)
cam.location = cam_pos
cam.rotation_mode = 'QUATERNION'
cam.rotation_quaternion = basis_quat(-RIGHT * -1, UP).copy()  # placeholder, replaced below
# Blender camera looks down its local -Z with +Y up
look = FWD; up = UP; right = look.cross(up)
cam.rotation_quaternion = Matrix((right, up, -look)).transposed().to_quaternion()
if len(sys.argv) > 0 and scenario.endswith("side"):
    pass
bpy.context.scene.camera = cam
sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", 'SUN')); bpy.context.scene.collection.objects.link(sun)
sun.data.energy = 3.5; sun.rotation_euler = (math.radians(50), math.radians(15), math.radians(30))
fill = bpy.data.objects.new("Fill", bpy.data.lights.new("Fill", 'POINT')); bpy.context.scene.collection.objects.link(fill)
fill.data.energy = 30; fill.location = cam_pos + Vector((0.2, 0.0, 0.4))
world = bpy.data.worlds.new("W"); bpy.context.scene.world = world
world.use_nodes = True; world.node_tree.nodes["Background"].inputs[0].default_value = (0.03, 0.035, 0.04, 1)
world.node_tree.nodes["Background"].inputs[1].default_value = 1.0

sc = bpy.context.scene
sc.render.engine = 'CYCLES'; sc.cycles.device = 'CPU'; sc.cycles.samples = 24
sc.cycles.use_denoising = True
sc.render.resolution_x, sc.render.resolution_y = 960, 540
sc.render.filepath = os.path.join(outdir, f"ik_{scenario}.png")
bpy.ops.render.render(write_still=True)

# second view from the side to judge the arm shape
cam.location = cam_pos + RIGHT * 0.9 + FWD * 0.35 + UP * 0.1
look = (cam_pos + FWD * 0.45 - UP * 0.35 - cam.location).normalized(); up = UP; right = look.cross(up).normalized(); up = right.cross(look)
cam.rotation_quaternion = Matrix((right, up, -look)).transposed().to_quaternion()
cam_data.angle = math.radians(60)
sc.render.filepath = os.path.join(outdir, f"ik_{scenario}_side.png")
bpy.ops.render.render(write_still=True)
if scenario.endswith("close"):
    def shot(tag, target, offset, fov=35):
        cam.location = target + offset
        look = (target - cam.location).normalized(); right = look.cross(UP).normalized(); up = right.cross(look)
        cam.rotation_quaternion = Matrix((right, up, -look)).transposed().to_quaternion()
        cam_data.angle = math.radians(fov)
        sc.render.filepath = os.path.join(outdir, f"ik_{scenario}_{tag}.png"); bpy.ops.render.render(write_still=True)
    for s_ in "rl":
        fing = new_head.get("middle_02_"+s_, head("middle_02_"+s_))
        out = RIGHT if s_ == "r" else -RIGHT
        # from the outside-front, slightly above, and from below
        shot("top_"+s_, fing, out * 0.18 + FWD * 0.30 + UP * 0.30)
        shot("under_"+s_, fing, out * 0.10 + FWD * 0.35 - UP * 0.25)
print("rendered", scenario)
