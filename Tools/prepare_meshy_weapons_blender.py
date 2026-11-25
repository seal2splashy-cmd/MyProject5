"""Turn Meshy weapon downloads into game-ready assets for MyProject5.

Meshy exports every model scaled to ~1.9 m, in whatever orientation it likes, with an OpenGL
normal map and 4K textures. This script, per weapon:
  - finds which end is which (muzzle vs grip, taped end vs head) from the geometry/texture,
  - turns it to Unreal's frame (+X forward, +Z up) and scales it to real size,
  - puts the pivot where the game code expects it, and MEASURES the grip, grip angle and
    muzzle / hand positions so the C++ numbers can match,
  - writes 2K textures (DirectX normal for Unreal) and a texture-free FBX to Assets/Weapons.

Inputs (git-ignored, raw downloads): Assets/Meshy/<name>.glb
  pistol.glb -> SM_AH_Pistol    (AGun)
  pipe.glb   -> SM_AH_Pipe      (AMeleePipe)
Then run Tools/import_meshy_weapons_ue58.py in Unreal.

Run in Blender 4.x / 5.x (Text > Open, Run Script) or:  blender -b -P Tools/prepare_meshy_weapons_blender.py
Starts from an empty scene: save anything open first.
"""
import math
import os

import bpy          # first: with the pip 'bpy' module, bmesh/mathutils need it imported
import numpy as np
from mathutils import Matrix, Vector

SCRIPT_NAME = "prepare_meshy_weapons_blender.py"


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
SRC = os.path.join(ROOT, "Assets", "Meshy")
OUT = os.path.join(ROOT, "Assets", "Weapons")
TEX_SIZE = 2048
CM = 0.01


# --------------------------------------------------------------------------- helpers

def load_glb(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=path)
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    if len(meshes) != 1:
        raise RuntimeError("%s: expected one mesh, found %d" % (path, len(meshes)))
    obj = meshes[0]
    obj.data.transform(obj.matrix_world)       # bake any import transform into the vertices
    obj.matrix_world = Matrix.Identity(4)
    obj.parent = None
    return obj


def verts(obj):
    co = np.empty(len(obj.data.vertices) * 3)
    obj.data.vertices.foreach_get("co", co)
    return co.reshape(-1, 3)


def transform(obj, m):
    obj.data.transform(m)
    obj.data.update()


def texture_of(obj, socket):
    """The image feeding a Principled BSDF input (following through normal-map nodes)."""
    mat = obj.data.materials[0]
    bsdf = next(n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    stack = [bsdf.inputs[socket].links[0].from_node] if bsdf.inputs[socket].is_linked else []
    while stack:
        n = stack.pop()
        if n.type == 'TEX_IMAGE':
            return n.image
        for i in n.inputs:
            stack += [l.from_node for l in i.links]
    return None


def image_array(img):
    w, h = img.size
    px = np.empty(w * h * 4, dtype=np.float32)
    img.pixels.foreach_get(px)
    return px.reshape(h, w, 4)              # row 0 = bottom of the image (Blender convention)


def save_png(arr, path, colour):
    """arr: HxWx4 float (bottom row first). Resized to TEX_SIZE and written as PNG."""
    h, w = arr.shape[:2]
    img = bpy.data.images.new("tmp_out", w, h, alpha=True, float_buffer=False)
    img.colorspace_settings.name = 'sRGB' if colour else 'Non-Color'
    img.pixels.foreach_set(np.ascontiguousarray(arr, dtype=np.float32).ravel())
    if w != TEX_SIZE:
        img.scale(TEX_SIZE, TEX_SIZE)
    img.filepath_raw = path
    img.file_format = 'PNG'
    img.save()
    bpy.data.images.remove(img)


def write_textures(obj, name):
    base = texture_of(obj, "Base Color")
    normal = texture_of(obj, "Normal")
    mr = texture_of(obj, "Roughness") or texture_of(obj, "Metallic")
    if base:
        save_png(image_array(base), os.path.join(OUT, "T_AH_%s_D.png" % name), True)
    if normal:
        n = image_array(normal)
        n[..., 1] = 1.0 - n[..., 1]         # glTF/OpenGL (green up) -> Unreal/DirectX (green down)
        save_png(n, os.path.join(OUT, "T_AH_%s_N.png" % name), False)
    if mr:
        save_png(image_array(mr), os.path.join(OUT, "T_AH_%s_MR.png" % name), False)  # G rough, B metal


def export(obj, name):
    obj.name = obj.data.name = "SM_AH_" + name
    mat = obj.data.materials[0]
    mat.name = "M_AH_" + name
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(OUT, "SM_AH_%s.fbx" % name)
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={'MESH'},
                             mesh_smooth_type='OFF', path_mode='STRIP', embed_textures=False,
                             add_leaf_bones=False, bake_anim=False, axis_forward='-Z', axis_up='Y')
    return path


def _weld(p, canon):
    out = np.empty((canon.max() + 1, 3))
    out[canon.ravel()] = p
    return out


def slice_loops(obj, z):
    """Exact horizontal cross-section at height z: x-extent of each closed outline."""
    me = obj.data
    me.calc_loop_triangles()
    tv = np.empty(len(me.loop_triangles) * 3, dtype=np.int64)
    me.loop_triangles.foreach_get("vertices", tv)
    p = verts(obj)
    # glTF splits vertices along UV seams; weld by position so outlines join up across seams.
    _, canon = np.unique(np.round(p / 1e-6).astype(np.int64), axis=0, return_inverse=True)
    tv = canon.ravel()[tv].reshape(-1, 3)
    p = _weld(p, canon)
    above = p[tv, 2] > z
    crossing = above.any(1) & ~above.all(1)
    parent = {}

    def find(a):
        while parent.setdefault(a, a) != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    xs = {}
    for tri in tv[crossing]:
        cut = []
        for i, j in ((0, 1), (1, 2), (2, 0)):
            a, b = tri[i], tri[j]
            if (p[a, 2] > z) != (p[b, 2] > z):
                t = (z - p[a, 2]) / (p[b, 2] - p[a, 2])
                key = (min(a, b), max(a, b))
                xs[key] = p[a, 0] + t * (p[b, 0] - p[a, 0])
                cut.append(key)
        if len(cut) == 2:
            parent[find(cut[0])] = find(cut[1])
    loops = {}
    for key, x in xs.items():
        loops.setdefault(find(key), []).append(x)
    return sorted((min(v), max(v)) for v in loops.values() if len(v) > 2)


def to_unreal(v):
    """Blender point -> Unreal (cm, Y flipped on FBX import)."""
    return (v[0] / CM, -v[1] / CM, v[2] / CM)


# --------------------------------------------------------------------------- pistol

def prepare_pistol(path, length_cm=18.7):
    obj = load_glb(path)
    p = verts(obj)
    size = p.max(0) - p.min(0)

    # Long axis must be X and up must be Z (Meshy's GLB comes in like that; check anyway).
    if not (size[0] >= size[1] and size[2] >= size[1]):
        raise RuntimeError("pistol: unexpected orientation, size %s" % size)
    # The grip hangs below the rear: whichever end the lowest points sit nearer is the back.
    low = p[p[:, 2] < p[:, 2].min() + 0.2 * size[2]]
    if low[:, 0].mean() > p[:, 0].mean():
        transform(obj, Matrix.Rotation(math.pi, 4, 'Z'))      # muzzle to +X
    s = length_cm * CM / size[0]
    transform(obj, Matrix.Scale(s, 4))
    p = verts(obj)

    xmax, zmin = p[:, 0].max(), p[:, 2].min()
    front = p[p[:, 0] > xmax - 0.4 * CM]
    bore_z = (front[:, 2].min() + front[:, 2].max()) * 0.5

    # Grip: exact cross-sections from just above the base plate up to just under the trigger
    # guard. The rear-most outline in each slice is the grip; its middle traces the grip's axis.
    def grip_and_rest(z):
        # Ignore specks (stippling bumps, little floating bits): only outlines over 5 mm count.
        loops = [l for l in slice_loops(obj, z) if l[1] - l[0] > 0.5 * CM]
        if not loops:
            return None, []
        grip_loop = max(loops, key=lambda l: l[1] - l[0]) if z < zmin + 4 * CM else loops[0]
        in_front = [l for l in loops if l[0] > grip_loop[1] + 0.3 * CM]
        return grip_loop, in_front

    guard_bottom = None
    for z in np.arange(zmin + 1.5 * CM, bore_z, 0.25 * CM):
        _, in_front = grip_and_rest(z)
        if in_front:                               # the trigger guard appears in front of the grip
            guard_bottom = z
            break
    top = (guard_bottom if guard_bottom else zmin + 0.5 * (bore_z - zmin)) - 0.4 * CM
    centres = []
    for z in np.linspace(zmin + 1.5 * CM, top, 12):  # skip the flared base plate
        grip_loop, _ = grip_and_rest(z)
        if grip_loop:
            centres.append(((grip_loop[0] + grip_loop[1]) * 0.5, z))
    centres = np.array(centres)
    fit = np.polyfit(centres[:, 1], centres[:, 0], 1)              # x = a*z + b, up the grip
    axis = Vector((fit[0], 0.0, 1.0)).normalized()
    grip_z = zmin + 0.42 * (bore_z - zmin)
    grip = Vector((np.polyval(fit, grip_z), 0.0, grip_z))

    # Pivot so the grip centre lands at (-3, 0, -7) cm - where AGun puts the hands.
    target = Vector((-3 * CM, 0.0, -7 * CM))
    transform(obj, Matrix.Translation(target - grip))
    muzzle = Vector((xmax, 0.0, bore_z)) + (target - grip)

    write_textures(obj, "Pistol")
    out = export(obj, "Pistol")
    p = verts(obj)
    print("PISTOL: %s" % out)
    print("PISTOL: %d tris, %.1f x %.1f x %.1f cm" % (
        sum(len(f.vertices) - 2 for f in obj.data.polygons), *((p.max(0) - p.min(0)) / CM)))
    print("PISTOL: GripOffset (-3, 0, -7)  GripAxis (%.2f, 0, %.2f)  Muzzle (%.2f, %.2f, %.2f)" % (
        axis.x, axis.z, *to_unreal(muzzle)))


# --------------------------------------------------------------------------- pipe

def prepare_pipe(path, length_cm=92.0, thickness_scale=0.75):
    obj = load_glb(path)
    p = verts(obj)
    size = p.max(0) - p.min(0)
    long_axis = int(np.argmax(size))

    # Lay it along +X.
    if long_axis == 2:
        transform(obj, Matrix.Rotation(math.radians(90), 4, 'Y'))   # +Z -> +X
    elif long_axis == 1:
        transform(obj, Matrix.Rotation(math.radians(-90), 4, 'Z'))  # +Y -> +X
    p = verts(obj)

    # Which end is taped, and where? Sample the colour texture over the surface (area-weighted,
    # several points per triangle) and profile it along the pipe: the tape is the stretch that's
    # clearly less orange than the rust.
    me = obj.data
    me.calc_loop_triangles()
    n = len(me.loop_triangles)
    tri_loops = np.empty(n * 3, dtype=np.int64)
    me.loop_triangles.foreach_get("loops", tri_loops)
    tri_verts = np.empty(n * 3, dtype=np.int64)
    me.loop_triangles.foreach_get("vertices", tri_verts)
    area = np.empty(n)
    me.loop_triangles.foreach_get("area", area)
    uv = np.empty(len(me.loops) * 2)
    me.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)[tri_loops.reshape(-1, 3)]
    pos = p[tri_verts.reshape(-1, 3)]
    img = image_array(texture_of(obj, "Base Color"))
    h, w = img.shape[:2]
    xs, orange, wts = [], [], []
    for b in ((1/3, 1/3, 1/3), (.6, .2, .2), (.2, .6, .2), (.2, .2, .6), (.45, .45, .1), (.1, .45, .45)):
        b = np.array(b)[None, :, None]
        u = (uv * b).sum(1)
        c = img[np.clip((u[:, 1] * h).astype(int), 0, h - 1), np.clip((u[:, 0] * w).astype(int), 0, w - 1), :3]
        xs.append((pos * b).sum(1)[:, 0])
        orange.append(c[:, 0] - c[:, 2])
        wts.append(area)
    xs, orange, wts = np.concatenate(xs), np.concatenate(orange), np.concatenate(wts)
    lo, hi = xs.min(), xs.max()
    bins = np.linspace(lo, hi, 25)
    prof = np.array([np.average(orange[(xs >= a) & (xs < b2)], weights=wts[(xs >= a) & (xs < b2)])
                     if ((xs >= a) & (xs < b2)).any() else np.nan for a, b2 in zip(bins[:-1], bins[1:])])
    is_tape = prof < 0.6 * np.nanmedian(prof)
    # longest run of tape bins
    runs, start = [], None
    for i, t in enumerate(list(is_tape) + [False]):
        if t and start is None:
            start = i
        if not t and start is not None:
            runs.append((start, i - 1))
            start = None
    if not runs:
        raise RuntimeError("pipe: couldn't find the taped grip in the texture")
    a, b2 = max(runs, key=lambda r: r[1] - r[0])
    tape_bins = np.arange(a, b2 + 1)
    tape_mid = (bins[tape_bins.min()] + bins[tape_bins.max() + 1]) * 0.5
    if tape_mid > (lo + hi) * 0.5:
        transform(obj, Matrix.Rotation(math.pi, 4, 'Z'))          # taped end to -X
        tape_lo, tape_hi = -bins[tape_bins.max() + 1], -bins[tape_bins.min()]
        lo, hi = -hi, -lo
    else:
        tape_lo, tape_hi = bins[tape_bins.min()], bins[tape_bins.max() + 1]

    # Butt of the handle at the origin, real length, slimmer cross-section (Meshy made it chunky).
    s = length_cm * CM / (hi - lo)
    transform(obj, Matrix.Translation(Vector((-lo, 0, 0))))
    p = verts(obj)
    centre_yz = (p[:, 1:].max(0) + p[:, 1:].min(0)) * 0.5
    transform(obj, Matrix.Translation(Vector((0, -centre_yz[0], -centre_yz[1]))))
    transform(obj, Matrix.Diagonal((s, s * thickness_scale, s * thickness_scale, 1)))
    tape_lo, tape_hi = (tape_lo - lo) * s / CM, (tape_hi - lo) * s / CM

    write_textures(obj, "Pipe")
    out = export(obj, "Pipe")
    p = verts(obj)
    main = tape_lo + 0.72 * (tape_hi - tape_lo)
    second = tape_lo + 0.25 * (tape_hi - tape_lo)
    print("PIPE: %s" % out)
    print("PIPE: %d tris, %.1f cm long, %.1f cm thick; tape %.1f-%.1f cm from the butt" % (
        sum(len(f.vertices) - 2 for f in obj.data.polygons), (p[:, 0].max() - p[:, 0].min()) / CM,
        (p[:, 1].max() - p[:, 1].min()) / CM, tape_lo, tape_hi))
    print("PIPE: MainHandOffset %.1f  SecondHandOffset %.1f  blade %.1f-%.1f" % (
        main, second, tape_hi + 3.0, length_cm - 1.0))


def main():
    os.makedirs(OUT, exist_ok=True)
    jobs = (("pistol.glb", prepare_pistol), ("pipe.glb", prepare_pipe))
    done = 0
    for fname, fn in jobs:
        path = os.path.join(SRC, fname)
        if os.path.isfile(path):
            fn(path)
            done += 1
        else:
            print("skip: no %s" % path)
    if not done:
        raise RuntimeError("Put the Meshy downloads in %s as pistol.glb / pipe.glb" % SRC)


main()
