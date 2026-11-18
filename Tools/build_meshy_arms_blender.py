"""Put the Meshy tactical forearms/gloves onto the operator arm rig.

STATUS (Nov 2026): WORK IN PROGRESS, not used by anything yet. The first Meshy arms had six
digits per hand in a cupped claw pose, so the fingertip finding (arm_landmarks) stops with
"expected 5". It's waiting for a regenerated five-finger, flat-hand model; the fingertip step
will be finished against that. Nothing else in the project depends on this script.

The game's arms (UOperatorArmsComponent) are driven entirely by IK on the operator rig's
43 named bones, so a new arm model has to live on that exact skeleton. The Meshy model is
forearms + hands only (cut at the elbow), posed reaching forward with spread fingers. This script:

  1. loads the current rigged arms (Assets/OperatorArms/SK_AH_ContainmentOperator_FP.fbx)
     and the Meshy model (Assets/Meshy/arms.glb, git-ignored raw download),
  2. finds, on each Meshy arm, the elbow end, the wrist (narrowest point), and the five
     fingers (the bare fingertips of the fingerless gloves, found from the texture),
  3. scales/turns each Meshy arm so its elbow and wrist land exactly on the rig's elbow and
     wrist, rolled so the hand matches the rig's palm-down hand,
  4. refits the wrist, hand and finger bones into the Meshy hand (same names, same hierarchy),
  5. keeps the operator's upper arms (clavicle/upperarm) and drops its forearms and hands,
  6. skins the Meshy part to the rig and joins everything into one mesh,
  7. writes Assets/OperatorArmsMeshy/SK_AH_OperatorArms_Meshy.fbx + 2K textures
     (Tools/import_meshy_arms_ue58.py imports them; the component uses them automatically).

Run in Blender 4.x / 5.x (Text > Open, Run Script) or:  blender -b -P Tools/build_meshy_arms_blender.py
Starts from an empty scene: save anything open first.
"""
import math
import os

import bpy          # first: with the pip 'bpy' module, bmesh/mathutils need it imported
import bmesh
import numpy as np
from mathutils import Matrix, Quaternion, Vector

SCRIPT_NAME = "build_meshy_arms_blender.py"


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


ROOT = os.path.dirname(script_folder())
OP_FBX = os.path.join(ROOT, "Assets", "OperatorArms", "SK_AH_ContainmentOperator_FP.fbx")
MESHY_GLB = os.path.join(ROOT, "Assets", "Meshy", "arms.glb")
OUT_DIR = os.path.join(ROOT, "Assets", "OperatorArmsMeshy")
OUT_NAME = "SK_AH_OperatorArms_Meshy"
TEX_SIZE = 2048

DIGITS = ("index", "middle", "ring", "pinky", "thumb")
FOREARM_AND_HAND = ("lowerarm", "forearm_twist", "wrist", "hand") + DIGITS


# --------------------------------------------------------------------------- small helpers

def v3(a):
    return Vector((float(a[0]), float(a[1]), float(a[2])))


def arr(v):
    return np.array([v[0], v[1], v[2]], dtype=float)


def unit(a):
    n = np.linalg.norm(a)
    return a / n if n > 1e-12 else a


def image_array(img):
    w, h = img.size
    px = np.empty(w * h * 4, dtype=np.float32)
    img.pixels.foreach_get(px)
    return px.reshape(h, w, 4)


def save_png(a, path, colour):
    h, w = a.shape[:2]
    img = bpy.data.images.new("tmp_out", w, h, alpha=True, float_buffer=False)
    img.colorspace_settings.name = 'sRGB' if colour else 'Non-Color'
    img.pixels.foreach_set(np.ascontiguousarray(a, dtype=np.float32).ravel())
    if w != TEX_SIZE:
        img.scale(TEX_SIZE, TEX_SIZE)
    img.filepath_raw = path
    img.file_format = 'PNG'
    img.save()
    bpy.data.images.remove(img)


def bsdf_image(mat, socket):
    bsdf = next(n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    stack = [bsdf.inputs[socket].links[0].from_node] if bsdf.inputs[socket].is_linked else []
    while stack:
        n = stack.pop()
        if n.type == 'TEX_IMAGE':
            return n.image
        for i in n.inputs:
            stack += [l.from_node for l in i.links]
    return None


def clusters(points, radius):
    """Single-linkage clusters (index lists), largest first. Plain numpy (Blender has no scipy);
    fine for the few hundred fingertip points this is used on."""
    near = np.linalg.norm(points[:, None, :] - points[None, :, :], axis=-1) < radius
    label = np.full(len(points), -1)
    groups = []
    for i in range(len(points)):
        if label[i] >= 0:
            continue
        label[i] = len(groups)
        stack, members = [i], [i]
        while stack:
            j = stack.pop()
            for k in np.where(near[j] & (label < 0))[0]:
                label[k] = label[i]
                stack.append(k)
                members.append(k)
        groups.append(np.array(members))
    return sorted(groups, key=len, reverse=True)


# --------------------------------------------------------------------------- load

def load_operator():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=OP_FBX)
    rig = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE')
    body = next(o for o in bpy.context.scene.objects if o.type == 'MESH')
    return rig, body


def load_meshy():
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=MESHY_GLB)
    obj = next(o for o in bpy.data.objects if o not in before and o.type == 'MESH')
    obj.data.transform(obj.matrix_world)
    obj.parent = None
    obj.matrix_world = Matrix.Identity(4)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)   # glTF splits vertices at UV seams
    bm.to_mesh(obj.data)
    bm.free()
    return obj


def vertex_colours(obj):
    """Average base-colour texel per vertex (sRGB values as stored)."""
    me = obj.data
    uv = np.empty(len(me.loops) * 2)
    me.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    vi = np.empty(len(me.loops), dtype=np.int64)
    me.loops.foreach_get("vertex_index", vi)
    img = image_array(bsdf_image(me.materials[0], "Base Color"))
    h, w = img.shape[:2]
    c = img[np.clip((uv[:, 1] * h).astype(int), 0, h - 1), np.clip((uv[:, 0] * w).astype(int), 0, w - 1), :3]
    col = np.zeros((len(me.vertices), 3))
    cnt = np.zeros(len(me.vertices))
    np.add.at(col, vi, c)
    np.add.at(cnt, vi, 1)
    return col / np.maximum(cnt, 1)[:, None]


def components(obj):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.verts.ensure_lookup_table()
    comp = np.full(len(bm.verts), -1)
    c = 0
    for v in bm.verts:
        if comp[v.index] >= 0:
            continue
        stack = [v]
        comp[v.index] = c
        while stack:
            x = stack.pop()
            for e in x.link_edges:
                y = e.other_vert(x)
                if comp[y.index] < 0:
                    comp[y.index] = c
                    stack.append(y)
        c += 1
    bm.free()
    return comp


# --------------------------------------------------------------------------- landmarks

def arm_landmarks(p, skin):
    """Landmarks of one Meshy arm (its own units). Returns dict of numpy points/dirs."""
    c = p.mean(0)
    ev, evec = np.linalg.eigh(np.cov((p - c).T))
    axis = evec[:, 2]
    if (p[skin].mean(0) - c) @ axis < 0:
        axis = -axis
    t = (p - c) @ axis
    t0, L = t.min(), t.max() - t.min()

    elbow = p[t < t0 + 0.025 * L].mean(0)

    # Wrist: the narrowest slice between the forearm and the hand.
    u = unit(np.cross(axis, [0.0, 0.0, 1.0]))
    v = np.cross(axis, u)
    best = None
    for f in np.linspace(0.45, 0.72, 28):
        m = np.abs(t - (t0 + f * L)) < 0.015 * L
        if m.sum() < 8:
            continue
        q = p[m] - c
        width = np.ptp(q @ u) + np.ptp(q @ v)
        if best is None or width < best[0]:
            best = (width, p[m].mean(0), f)
    wrist = best[1]

    # Fingers: the bare fingertips are five separate skin-coloured patches.
    tips_idx = [g for g in clusters(p[skin], 0.06 * L / 2.0) if len(g) >= 12][:5]
    if len(tips_idx) != 5:
        raise RuntimeError("found %d fingertip patches on an arm, expected 5" % len(tips_idx))
    skin_pts = p[skin]
    hand_dir = unit(np.mean([skin_pts[g].mean(0) for g in tips_idx], 0) - wrist)
    fingers = []
    for g in tips_idx:
        q = skin_pts[g]
        qc = q.mean(0)
        d = unit(np.linalg.eigh(np.cov((q - qc).T))[1][:, 2])
        if (qc - wrist) @ d < 0:
            d = -d
        s = (q - qc) @ d
        fingers.append({"dir": d, "tip": qc + d * s.max(), "base": qc + d * s.min(), "centre": qc,
                        "reach": (qc - wrist) @ hand_dir})
    # Thumb: the patch that reaches least far out from the wrist.
    fingers.sort(key=lambda f: f["reach"])
    thumb, rest = fingers[0], fingers[1:]
    side = unit(np.cross(hand_dir, np.cross(thumb["centre"] - wrist, hand_dir)))   # towards the thumb
    rest.sort(key=lambda f: -((f["centre"] - wrist) @ side))                       # index .. pinky
    return {"axis": axis, "elbow": elbow, "wrist": wrist, "hand_dir": hand_dir,
            "index": rest[0], "middle": rest[1], "ring": rest[2], "pinky": rest[3], "thumb": thumb,
            "wrist_frac": best[2]}


def similarity(src_a, src_b, dst_a, dst_b, src_side, dst_side):
    """Uniform scale + rotation + translation taking segment a->b onto a'->b', rolled about it so
    that src_side (projected) lines up with dst_side (projected)."""
    s = np.linalg.norm(dst_b - dst_a) / np.linalg.norm(src_b - src_a)
    fa, fb = unit(src_b - src_a), unit(dst_b - dst_a)
    r1 = v3(fa).rotation_difference(v3(fb))
    side1 = arr(r1 @ v3(src_side))
    side1 -= fb * (side1 @ fb)
    tgt = dst_side - fb * (dst_side @ fb)
    ang = math.atan2(np.cross(unit(side1), unit(tgt)) @ fb, unit(side1) @ unit(tgt))
    r = Quaternion(v3(fb), ang) @ r1
    R = np.array(r.to_matrix())
    return lambda x: (R @ ((np.asarray(x) - src_a).T)).T * s + dst_a, R, s


# --------------------------------------------------------------------------- main

def main():
    for path in (OP_FBX, MESHY_GLB):
        if not os.path.isfile(path):
            raise RuntimeError("missing " + path)
    os.makedirs(OUT_DIR, exist_ok=True)

    rig, body = load_operator()
    meshy = load_meshy()
    me = meshy.data
    co = np.empty(len(me.vertices) * 3)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    col = vertex_colours(meshy)
    skin = (col[:, 0] > 0.35) & (col[:, 0] > col[:, 2] * 1.25) & (col[:, 0] > col[:, 1] * 1.05)
    comp = components(meshy)
    arms = [k for k in np.argsort(-np.bincount(comp))[:2]]

    M = rig.matrix_world
    bones = rig.data.bones
    head = lambda n: arr(M @ bones[n].head_local)

    new_co = co.copy()
    fitted = {}
    for k in arms:
        m = comp == k
        lm = arm_landmarks(co[m], skin[m])
        # Meshy faces +Y with +X to the right; its right arm is the one at +X. Confirm with the
        # thumb: palm down, a right thumb points to the body's left (-X).
        x = "r" if co[m][:, 0].mean() > 0 else "l"
        thumb_side = (lm["thumb"]["centre"] - lm["wrist"])[0]
        if (x == "r") != (thumb_side < 0):
            print("WARNING: arm at x=%.2f has its thumb on the unexpected side" % co[m][:, 0].mean())

        E0, W0 = head("lowerarm_" + x), head("wrist_" + x)
        rig_side = head("pinky_01_" + x) - head("index_01_" + x)
        msh_side = lm["pinky"]["base"] - lm["index"]["base"]
        T, R, s = similarity(lm["elbow"], lm["wrist"], E0, W0, msh_side, rig_side)
        new_co[m] = T(co[m])
        fitted[x] = {"T": T, "lm": lm, "scale": s}
        print("ARMS: %s arm: scale %.1f cm/unit, wrist at %.0f%% of its length" % (
            x, s * 100, lm["wrist_frac"] * 100))

    me.vertices.foreach_set("co", new_co.ravel())
    me.update()

    # ---- refit wrist / hand / finger bones into the Meshy hands (rest pose)
    inv = M.inverted()
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode='EDIT')
    eb = rig.data.edit_bones

    def place(name, a, b):
        e = eb[name]
        e.head = inv @ v3(a)
        e.tail = inv @ v3(b)

    for x, f in fitted.items():
        T, lm = f["T"], f["lm"]
        W0 = arr(M @ bones["wrist_" + x].head_local)
        joints = {}
        for d in ("index", "middle", "ring", "pinky"):
            fi = lm[d]
            tip, base, dirn = T(fi["tip"]), T(fi["base"]), f["T"](fi["base"] + fi["dir"]) - T(fi["base"])
            dirn = unit(dirn)
            exposed = np.linalg.norm(tip - base)
            pip = base                                   # the glove ends at about the middle knuckle
            mcp = pip - dirn * exposed / 1.07            # proximal ~= middle + distal phalanx
            dip = pip + (tip - pip) * 0.58
            joints[d] = (mcp, pip, dip, tip)
        th = lm["thumb"]
        ttip, tbase = T(th["tip"]), T(th["base"])
        tdir = unit(ttip - tbase)
        texp = np.linalg.norm(ttip - tbase)
        t_ip = tbase + tdir * 0.15 * texp
        t_mcp = t_ip - tdir * 1.1 * texp
        mid_mcp = joints["middle"][0]
        H0 = W0 + 0.3 * (mid_mcp - W0)
        t_cmc = H0 + 0.45 * (t_mcp - H0)
        joints["thumb"] = (t_cmc, t_mcp, t_ip, ttip)

        place("wrist_" + x, W0, H0)
        place("hand_" + x, H0, mid_mcp)
        for d, (j0, j1, j2, j3) in joints.items():
            place("%s_01_%s" % (d, x), j0, j1)
            place("%s_02_%s" % (d, x), j1, j2)
            place("%s_03_%s" % (d, x), j2, j3)
        f["joints"] = joints
    bpy.ops.object.mode_set(mode='OBJECT')

    # ---- operator: keep only the upper arms (and the clavicle/shoulder area)
    names = {g.index: g.name for g in body.vertex_groups}
    drop = []
    for v in body.data.vertices:
        if v.groups:
            g = max(v.groups, key=lambda g: g.weight)
            if names[g.group].rsplit("_", 1)[0] in FOREARM_AND_HAND or names[g.group].split("_")[0] in DIGITS:
                drop.append(v.index)
    bm = bmesh.new()
    bm.from_mesh(body.data)
    bm.verts.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[bm.verts[i] for i in drop], context='VERTS')
    bm.to_mesh(body.data)
    bm.free()
    print("ARMS: kept %d operator upper-arm vertices" % len(body.data.vertices))

    # ---- skin the Meshy forearms/hands to the refitted rig
    meshy.name = meshy.data.name = "MeshyForearms"
    mat = meshy.data.materials[0]
    write_textures(mat)
    mat.name = "M_AH_MeshyArms"
    bpy.ops.object.select_all(action='DESELECT')
    meshy.select_set(True)
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.parent_set(type='ARMATURE_AUTO')
    weighted = sum(1 for v in meshy.data.vertices if v.groups and max(g.weight for g in v.groups) > 0)
    print("ARMS: automatic weights on %d / %d Meshy vertices" % (weighted, len(meshy.data.vertices)))

    # ---- one mesh, export
    bpy.ops.object.select_all(action='DESELECT')
    meshy.select_set(True)
    body.select_set(True)
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.join()
    body.name = body.data.name = OUT_NAME

    bpy.ops.object.select_all(action='DESELECT')
    body.select_set(True)
    rig.select_set(True)
    out = os.path.join(OUT_DIR, OUT_NAME + ".fbx")
    bpy.ops.export_scene.fbx(
        filepath=out, use_selection=True, object_types={'ARMATURE', 'MESH'}, use_mesh_modifiers=True,
        apply_unit_scale=True, apply_scale_options='FBX_SCALE_UNITS', axis_forward='-Y', axis_up='Z',
        add_leaf_bones=False, use_armature_deform_only=True, bake_anim=False, mesh_smooth_type='FACE',
        use_tspace=True, path_mode='STRIP', embed_textures=False)
    print("ARMS: exported %s (%d verts, materials: %s)" % (
        out, len(body.data.vertices), ", ".join(m.name for m in body.data.materials if m)))
    return rig, body, fitted


def write_textures(mat):
    base = bsdf_image(mat, "Base Color")
    normal = bsdf_image(mat, "Normal")
    mr = bsdf_image(mat, "Roughness") or bsdf_image(mat, "Metallic")
    if base:
        save_png(image_array(base), os.path.join(OUT_DIR, "T_AH_MeshyArms_D.png"), True)
    if normal:
        n = image_array(normal)
        n[..., 1] = 1.0 - n[..., 1]          # glTF/OpenGL -> Unreal/DirectX
        save_png(n, os.path.join(OUT_DIR, "T_AH_MeshyArms_N.png"), False)
    if mr:
        save_png(image_array(mr), os.path.join(OUT_DIR, "T_AH_MeshyArms_MR.png"), False)


if __name__ == "__main__":
    main()
