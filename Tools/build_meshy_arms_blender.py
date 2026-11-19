"""Put the Meshy tactical forearms/gloves onto the operator arm rig.

Input: the Meshy download in Assets/Meshy/arms/ (the FBX and its PNGs, as Meshy zips them) or
Assets/Meshy/arms.glb - git-ignored. Needs two separate arms, five digits each, hands flat and
straight (Meshy prompt in Docs/OPERATOR_ARMS_MESHY.md). It checks and stops with a clear
message if it can't find exactly a thumb and four fingers on each hand.

The game's arms (UOperatorArmsComponent) are driven entirely by IK on the operator rig's
43 named bones, so a new arm model has to live on that exact skeleton. The Meshy model is
forearms + hands only (cut at the elbow), posed reaching forward with spread fingers. This script:

  1. loads the current rigged arms (Assets/OperatorArms/SK_AH_ContainmentOperator_FP.fbx)
     and the Meshy model,
  2. finds, on each Meshy arm, the five digits (the bare skin of the fingerless gloves, found
     from the texture), which is the thumb, which hand it is (from which side the thumb is
     on), the finger joints, the wrist and the elbow (from hand proportions),
  3. scales/turns each Meshy arm (one scale for both) so its wrist lands on the rig's wrist
     and its forearm lies along the rig's forearm, rolled to the rig's palm-down hand,
  4. refits the wrist, hand and finger bones into the Meshy hand (same names, same hierarchy),
  5. keeps the operator's upper arms (clavicle/upperarm) and drops its forearms and hands,
  6. skins the Meshy part to the rig (automatic weights, smoothed, max 4 bones per vertex)
     and joins everything into one mesh,
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
# The Meshy download: either the folder as Meshy zips it (FBX + PNGs) or a single GLB.
MESHY_DIR = os.path.join(ROOT, "Assets", "Meshy", "arms")
MESHY_GLB = os.path.join(ROOT, "Assets", "Meshy", "arms.glb")
OUT_DIR = os.path.join(ROOT, "Assets", "OperatorArmsMeshy")
OUT_NAME = "SK_AH_OperatorArms_Meshy"
TEX_SIZE = 2048
SMOOTH_WEIGHTS = 3          # passes of weight smoothing on the Meshy skin (0 = raw heat weights)

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


def meshy_source():
    """(model file, {'D','N','MR','R','M': png path}) for the Meshy download."""
    import glob
    pngs = {}
    if os.path.isdir(MESHY_DIR):
        models = sorted(glob.glob(os.path.join(MESHY_DIR, "*.fbx")) + glob.glob(os.path.join(MESHY_DIR, "*.glb")))
        for f in glob.glob(os.path.join(MESHY_DIR, "*.png")):
            n = os.path.basename(f).lower()
            key = ("MR" if "metallic_roughness" in n else "N" if "normal" in n else
                   "R" if "roughness" in n else "M" if "metallic" in n else "D")
            pngs[key] = f
        if models:
            return models[0], pngs
    if os.path.isfile(MESHY_GLB):
        return MESHY_GLB, pngs
    raise RuntimeError("Put the Meshy arms download in %s (FBX + PNGs) or %s" % (MESHY_DIR, MESHY_GLB))


def load_meshy(path):
    before = set(bpy.data.objects)
    if path.lower().endswith(".glb"):
        bpy.ops.import_scene.gltf(filepath=path)
    else:
        bpy.ops.import_scene.fbx(filepath=path)
    obj = next(o for o in bpy.data.objects if o not in before and o.type == 'MESH')
    obj.data.transform(obj.matrix_world)
    obj.parent = None
    obj.matrix_world = Matrix.Identity(4)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)   # exporters split vertices at UV seams
    for f in bm.faces:
        f.smooth = True
    bm.to_mesh(obj.data)
    bm.free()
    # Meshy's own normals don't match the welded low-poly surface (the skin comes out patchy),
    # so throw them away: smooth shading, sharp only on hard creases (armour plate edges).
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    if obj.data.has_custom_normals:
        bpy.ops.mesh.customdata_custom_splitnormals_clear()
    try:
        bpy.ops.object.shade_smooth_by_angle(angle=math.radians(50))
    except Exception:
        pass
    return obj


def vertex_colours(obj, base_png=None):
    """Average base-colour texel per vertex (sRGB values as stored)."""
    me = obj.data
    uv = np.empty(len(me.loops) * 2)
    me.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    vi = np.empty(len(me.loops), dtype=np.int64)
    me.loops.foreach_get("vertex_index", vi)
    src = bsdf_image(me.materials[0], "Base Color") if me.materials and me.materials[0] else None
    if (src is None or not src.has_data) and base_png:
        src = bpy.data.images.load(base_png)
    img = image_array(src)
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
    """Joint positions for one Meshy arm, in the model's own units.

    Works on a straight arm with a flat hand (the pose the Meshy prompt asks for). The bare skin
    of the fingerless glove gives the five digits; seen across the hand they're five separate
    patches. The thumb is the one that reaches least far; index..pinky are ordered away from it.
    """
    c = p.mean(0)
    axis = np.linalg.eigh(np.cov((p - c).T))[1][:, 2]
    if (p[skin].mean(0) - c) @ axis < 0:
        axis = -axis                                   # elbow -> fingertips
    t = (p - c) @ axis
    L = np.ptp(t)
    sk = p[skin & (t > np.median(t))]                  # hand half only (no specks up the sleeve)

    # The thumb stands apart from the fingers; the four fingers touch at their bases, so at a
    # coarse radius the skin falls into two clumps: thumb (small) and fingers (big).
    q = sk - sk.mean(0)
    q -= np.outer(q @ axis, axis)
    lat0 = np.linalg.eigh(np.cov(q.T))[1][:, 2]        # roughly across the hand
    flat = np.stack([(sk - c) @ lat0, (sk - c) @ axis], 1)   # seen from the back of the hand
    groups = None
    for r in (0.02, 0.025, 0.016, 0.03):
        g = [x for x in clusters(flat, r * L) if len(x) >= 20]
        if len(g) >= 2 and len(g[0]) > 2.5 * len(g[1]) and len(g[0]) + len(g[1]) > 0.85 * len(sk):
            groups = g
            break
    if groups is None:
        raise RuntimeError("couldn't find the thumb apart from the fingers - check the Meshy model "
                           "has five separate digits on a flat hand (thumb not touching the fingers)")
    fing, thumb_g = sk[groups[0]], groups[1]

    # The four fingers lie side by side across the hand: split them along that direction
    # (1-D k-means, fingers are about equally wide).
    q = fing - fing.mean(0)
    q -= np.outer(q @ axis, axis)
    lat = np.linalg.eigh(np.cov(q.T))[1][:, 2]
    u = (fing - c) @ lat
    centres = np.quantile(u, [0.125, 0.375, 0.625, 0.875])
    for _ in range(50):
        lab = np.argmin(np.abs(u[:, None] - centres[None, :]), 1)
        new = np.array([u[lab == i].mean() if np.any(lab == i) else centres[i] for i in range(4)])
        if np.allclose(new, centres):
            break
        centres = new
    sizes = np.bincount(lab, minlength=4)
    if sizes.min() < 0.4 * sizes.max():
        raise RuntimeError("the fingers don't split into four similar ones (%s) - check the hand is flat "
                           "with the fingers straight and slightly apart" % sizes.tolist())
    groups = [thumb_g] + [groups[0][lab == i] for i in range(4)]

    digits = []
    for g in groups:
        d3 = sk[g]
        dc = d3.mean(0)
        dirn = unit(np.linalg.eigh(np.cov((d3 - dc).T))[1][:, 2])
        if dirn @ axis < 0:
            dirn = -dirn
        s = (d3 - dc) @ dirn
        digits.append({"tip": dc + dirn * s.max(), "base": dc + dirn * s.min(), "dir": dirn, "centre": dc})
    thumb, fingers = digits[0], digits[1:]
    fingers.sort(key=lambda d: abs((d["centre"] - thumb["centre"]) @ lat))   # index nearest the thumb
    index, middle, ring, pinky = fingers

    # Glove style: fully bare fingers (glove ends at the knuckles) or fingerless gloves that
    # cover the first phalanx. A whole finger is about as long as the knuckles are wide.
    knuckle_width = np.linalg.norm(pinky["base"] - index["base"])
    full = np.linalg.norm(middle["tip"] - middle["base"]) > 0.8 * knuckle_width

    joints = {}
    for name, f in (("index", index), ("middle", middle), ("ring", ring), ("pinky", pinky)):
        if full:
            mcp = f["base"] - f["dir"] * 0.03 * np.linalg.norm(f["tip"] - f["base"])
        else:
            mcp = f["base"] - f["dir"] * np.linalg.norm(f["tip"] - f["base"]) / 1.07
        joints[name] = [mcp, mcp + 0.48 * (f["tip"] - mcp), mcp + 0.78 * (f["tip"] - mcp), f["tip"]]

    mid_mcp, mid_tip = joints["middle"][0], joints["middle"][3]
    hand_dir = unit(mid_tip - mid_mcp)
    wrist_guess = mid_mcp - hand_dir * 1.05 * np.linalg.norm(mid_tip - mid_mcp)   # palm ~ middle finger
    wrist = c + axis * ((wrist_guess - c) @ axis)                                # centred in the arm
    elbow = wrist - axis * 1.35 * np.linalg.norm(mid_tip - wrist)                # forearm ~ 1.35 hands
    hand_head = wrist + 0.3 * (mid_mcp - wrist)

    tb, tt = thumb["base"], thumb["tip"]
    if full:
        t_mcp = tb
        t_ip = tb + 0.55 * (tt - tb)
    else:
        t_ip = tb + 0.15 * (tt - tb)
        t_mcp = t_ip - unit(tt - tb) * 1.1 * np.linalg.norm(tt - tb)
    joints["thumb"] = [hand_head + 0.45 * (t_mcp - hand_head), t_mcp, t_ip, tt]

    # Which hand: the thumb sits on the palm side, and for a right hand
    # (fingers x index->pinky) points out of the palm.
    palm = thumb["centre"] - np.mean([f["centre"] for f in fingers], 0)
    palm -= axis * (palm @ axis) + lat * (palm @ lat)
    is_right = np.cross(hand_dir, joints["pinky"][0] - joints["index"][0]) @ palm > 0

    return {"elbow": elbow, "wrist": wrist, "hand": hand_head, "joints": joints, "right": is_right,
            "full_fingers": full, "axis": axis}


def similarity(src_a, src_b, dst_a, dst_b, src_side, dst_side, scale=None):
    """Uniform scale + rotation + translation taking segment a->b onto a'->b', rolled about it so
    that src_side (projected) lines up with dst_side (projected). With a fixed scale, b lands
    exactly on b' and a lies along the line towards a'."""
    s = scale or np.linalg.norm(dst_b - dst_a) / np.linalg.norm(src_b - src_a)
    fa, fb = unit(src_b - src_a), unit(dst_b - dst_a)
    r1 = v3(fa).rotation_difference(v3(fb))
    side1 = arr(r1 @ v3(src_side))
    side1 -= fb * (side1 @ fb)
    tgt = dst_side - fb * (dst_side @ fb)
    ang = math.atan2(np.cross(unit(side1), unit(tgt)) @ fb, unit(side1) @ unit(tgt))
    r = Quaternion(v3(fb), ang) @ r1
    R = np.array(r.to_matrix())
    return lambda x: (R @ ((np.asarray(x) - src_b).T)).T * s + dst_b, R, s


def smooth_weights(obj, passes, factor=0.5, max_bones=4):
    """Laplacian smoothing of the skin weights over the mesh edges, then the top max_bones
    per vertex, normalised (plain numpy: the Blender operators need weight-paint mode)."""
    me = obj.data
    groups = list(obj.vertex_groups)
    W = np.zeros((len(me.vertices), len(groups)))
    for v in me.vertices:
        for g in v.groups:
            W[v.index, g.group] = g.weight
    ev = np.empty(len(me.edges) * 2, dtype=np.int64)
    me.edges.foreach_get("vertices", ev)
    a, b = ev[0::2], ev[1::2]
    deg = np.bincount(np.concatenate([a, b]), minlength=len(W)).astype(float)
    for _ in range(passes):
        acc = np.zeros_like(W)
        np.add.at(acc, a, W[b])
        np.add.at(acc, b, W[a])
        nb = acc / np.maximum(deg, 1)[:, None]
        W = np.where(deg[:, None] > 0, (1 - factor) * W + factor * nb, W)
    if W.shape[1] > max_bones:
        cut = np.sort(W, 1)[:, -max_bones][:, None]
        W[W < cut] = 0.0
    W /= np.maximum(W.sum(1, keepdims=True), 1e-9)
    for gi, g in enumerate(groups):
        g.remove(list(range(len(W))))
        idx = np.nonzero(W[:, gi] > 1e-4)[0]
        for i in idx:
            g.add([int(i)], float(W[i, gi]), 'REPLACE')


# --------------------------------------------------------------------------- main

def main():
    if not os.path.isfile(OP_FBX):
        raise RuntimeError("missing " + OP_FBX)
    src, pngs = meshy_source()
    os.makedirs(OUT_DIR, exist_ok=True)

    rig, body = load_operator()
    meshy = load_meshy(src)
    me = meshy.data
    co = np.empty(len(me.vertices) * 3)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    col = vertex_colours(meshy, pngs.get("D"))
    skin = (col[:, 0] > 0.35) & (col[:, 0] > col[:, 2] * 1.25) & (col[:, 0] > col[:, 1] * 1.05)
    comp = components(meshy)
    arms = [k for k in np.argsort(-np.bincount(comp))[:2]]

    M = rig.matrix_world
    bones = rig.data.bones
    head = lambda n: arr(M @ bones[n].head_local)

    new_co = co.copy()
    fitted = {}
    found = {}
    for k in arms:
        m = comp == k
        lm = arm_landmarks(co[m], skin[m])
        x = "r" if lm["right"] else "l"
        if x in found:
            raise RuntimeError("both Meshy arms look like %s hands - is one mirrored?" % ("right" if x == "r" else "left"))
        found[x] = (m, lm)
    if set(found) != {"l", "r"}:
        raise RuntimeError("need a left and a right arm in the Meshy model")

    # One scale for both arms (the rig's forearm over the Meshy forearm, averaged), so the
    # hands come out the same size; each wrist then lands exactly on the rig's wrist.
    fore = lambda x: np.linalg.norm(head("wrist_" + x) - head("lowerarm_" + x)) / \
        np.linalg.norm(found[x][1]["wrist"] - found[x][1]["elbow"])
    scale = 0.5 * (fore("l") + fore("r"))
    for x, (m, lm) in found.items():
        J = lm["joints"]
        E0, W0 = head("lowerarm_" + x), head("wrist_" + x)
        rig_side = head("pinky_01_" + x) - head("index_01_" + x)
        T, R, s = similarity(lm["elbow"], lm["wrist"], E0, W0, J["pinky"][0] - J["index"][0], rig_side, scale)
        new_co[m] = T(co[m])
        fitted[x] = {"T": T, "lm": lm, "scale": s}
        print("ARMS: %s hand, %s fingers, scale %.1f cm per Meshy unit" % (
            "right" if x == "r" else "left", "bare" if lm["full_fingers"] else "half-gloved", s * 100))

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
        joints = {d: [T(j) for j in js] for d, js in lm["joints"].items()}
        W0, H0 = T(lm["wrist"]), T(lm["hand"])
        place("wrist_" + x, W0, H0)
        place("hand_" + x, H0, joints["middle"][0])
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
    used = {p.material_index for p in body.data.polygons}     # glove/forearm-guard slots are empty now
    for i in reversed(range(len(body.data.materials))):
        if i not in used:
            body.data.materials.pop(index=i)
    print("ARMS: kept %d operator upper-arm vertices (%s)" % (
        len(body.data.vertices), ", ".join(m.name for m in body.data.materials if m)))

    # ---- skin the Meshy forearms/hands to the refitted rig
    meshy.name = meshy.data.name = "MeshyForearms"
    mat = meshy.data.materials[0]
    write_textures(mat, pngs)
    mat.name = "M_AH_MeshyArms"
    bpy.ops.object.select_all(action='DESELECT')
    meshy.select_set(True)
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.parent_set(type='ARMATURE_AUTO')
    # Heat weights on a remeshed low-poly hand crease hard at the knuckles: smooth them a bit,
    # then keep at most 4 bones per vertex, normalised.
    smooth_weights(meshy, SMOOTH_WEIGHTS)
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


def write_textures(mat, pngs):
    """2K textures for Unreal; Meshy's own PNGs if it shipped them, else the model's images."""
    def img(key, socket):
        if key in pngs:
            return bpy.data.images.load(pngs[key])
        return bsdf_image(mat, socket) if mat else None

    base, normal = img("D", "Base Color"), img("N", "Normal")
    if base:
        save_png(image_array(base), os.path.join(OUT_DIR, "T_AH_MeshyArms_D.png"), True)
    if normal:
        n = image_array(normal)
        n[..., 1] = 1.0 - n[..., 1]          # Meshy/OpenGL -> Unreal/DirectX
        save_png(n, os.path.join(OUT_DIR, "T_AH_MeshyArms_N.png"), False)
    if "MR" in pngs:
        mr = image_array(bpy.data.images.load(pngs["MR"]))          # G roughness, B metallic
    elif "R" in pngs:
        r = image_array(bpy.data.images.load(pngs["R"]))
        mr = np.ones_like(r)
        mr[..., 1] = r[..., 0]
        mr[..., 2] = image_array(bpy.data.images.load(pngs["M"]))[..., 0] if "M" in pngs else 0.0
    else:
        src = bsdf_image(mat, "Roughness") if mat else None
        mr = image_array(src) if src else None
    if mr is not None:
        save_png(mr, os.path.join(OUT_DIR, "T_AH_MeshyArms_MR.png"), False)


if __name__ == "__main__":
    main()
