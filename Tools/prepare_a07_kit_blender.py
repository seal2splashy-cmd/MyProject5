"""Turn the user's Meshy A-07 facility downloads into a game-ready kit for MyProject5.

Inputs: the extracted Meshy ZIPs (FBX + PNGs), anywhere under a folder you pass in (default
Assets/Meshy, git-ignored). Each model is recognised by its folder/file name:

  ContainmentVessel   -> SM_A07K_Vessel       glass containment tube, 5.2 m tall
  DeconChamber        -> SM_A07K_DeconPod     horizontal decontamination capsule, 3.2 m long
  LabControlConsole   -> SM_A07K_Console      lab control desk, 1.9 m wide
  containment_entrance-> SM_A07K_VaultDoor    sealed "B-7" vault door in its frame, 2.3 m wide
  damaged_concrete_wall-> SM_A07K_WallPanel   concrete wall panel with a steel edge column, 3 m wide
  containment_mounting-> SM_A07K_MountRing    round containment pedestal, 5 m across
  elevated_catwalk    -> SM_A07K_Catwalk      railed catwalk section, 6 m long

Per model: real size, pivot at the bottom centre, front (the detailed side) facing +Y in Unreal,
decimated to a game budget (Meshy's 0.5-3 M triangle scans -> 20-90 k; Nanite still fine),
2K textures: _D.jpg (base colour), _N.png (normal, flipped to DirectX for Unreal), _MR.jpg (G rough, B metal),
and a texture-free FBX in Assets/MeshyFacility/Kit/<Name>/. Then Tools/import_a07_kit_ue58.py.

It also MEASURES the catwalk (deck height, deck width, rail height) and prints them: the level
script uses those numbers for the walkable collision.

Run:  blender -b -P Tools/prepare_a07_kit_blender.py -- [input folder] [only these names, e.g. WallPanel]
"""
import glob
import json
import os
import sys

import bpy          # first, so mathutils is available with the pip bpy module
import numpy as np
from mathutils import Matrix, Vector

SCRIPT_NAME = "prepare_a07_kit_blender.py"
TEX_SIZE = 2048

# key: (words to find the download, output name, size axis, real size (m), target triangles)
KIT = [
    ("vessel", ["containmentvessel"], "Vessel", "z", 5.2, 60000),
    ("decon", ["deconchamber"], "DeconPod", "x", 3.2, 60000),
    ("console", ["labcontrolconsole", "controlconsole"], "Console", "x", 1.9, 40000),
    ("door", ["containment_entrance"], "VaultDoor", "x", 2.3, 120000),
    ("wall", ["damaged_concrete_wall"], "WallPanel", "x", 3.0, 120000),
    ("mount", ["containment_mounting"], "MountRing", "x", 5.0, 90000),
    ("catwalk", ["elevated_catwalk"], "Catwalk", "x", 6.0, 60000),
]


def script_folder():
    try:
        here = os.path.abspath(__file__)
        if os.path.isfile(here):
            return os.path.dirname(here)
    except NameError:
        pass
    for text in bpy.data.texts:
        path = bpy.path.abspath(text.filepath) if text.filepath else ""
        if path and os.path.basename(path) == SCRIPT_NAME:
            return os.path.dirname(path)
    return os.path.join(os.getcwd(), "Tools")


ROOT = os.path.dirname(script_folder())
OUT = os.path.join(ROOT, "Assets", "MeshyFacility", "Kit")


def find_download(src, words):
    for fbx in sorted(glob.glob(os.path.join(src, "**", "*.fbx"), recursive=True)):
        key = (os.path.dirname(fbx) + "/" + os.path.basename(fbx)).lower().replace(" ", "")
        if any(w in key for w in words):
            return fbx
    return None


def textures_next_to(fbx):
    """Meshy names: <base>.png (colour), *_normal, *_roughness, *_metallic (+ *_metallic_roughness)."""
    out = {}
    for png in glob.glob(os.path.join(os.path.dirname(fbx), "*.png")):
        n = os.path.basename(png).lower()
        if n.endswith("_metallic_roughness.png"):
            continue
        if n.endswith("_normal.png"):
            out["N"] = png
        elif n.endswith("_roughness.png"):
            out["R"] = png
        elif n.endswith("_metallic.png"):
            out["M"] = png
        else:
            out.setdefault("D", png)
    return out


def load_fbx(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=path)
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    if not meshes:
        raise RuntimeError("no mesh in " + path)
    bpy.ops.object.select_all(action='DESELECT')
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    obj.data.transform(obj.matrix_world)
    obj.matrix_world = Matrix.Identity(4)
    obj.parent = None
    return obj


def verts(obj):
    co = np.empty(len(obj.data.vertices) * 3)
    obj.data.vertices.foreach_get("co", co)
    return co.reshape(-1, 3)


def tri_count(obj):
    obj.data.calc_loop_triangles()
    return len(obj.data.loop_triangles)


def decimate(obj, target):
    tris = tri_count(obj)
    if tris <= target * 1.1:
        return tris
    mod = obj.modifiers.new("dec", 'DECIMATE')
    mod.decimate_type = 'COLLAPSE'
    mod.ratio = target / float(tris)
    mod.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return tri_count(obj)


def normalise(obj, axis, size_m):
    """Real size, pivot bottom-centre. Meshy's front is Blender -Y, which is Unreal +Y: kept."""
    v = verts(obj)
    lo, hi = v.min(0), v.max(0)
    span = hi[{"x": 0, "y": 1, "z": 2}[axis]] - lo[{"x": 0, "y": 1, "z": 2}[axis]]
    s = size_m / span
    centre = Vector(((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, lo[2]))
    obj.data.transform(Matrix.Scale(s, 4) @ Matrix.Translation(-centre))
    obj.data.update()
    v = verts(obj)
    return v.max(0) - v.min(0)


def write_textures(tex, name, folder):
    def load(path, colour):
        img = bpy.data.images.load(path)
        img.colorspace_settings.name = 'sRGB' if colour else 'Non-Color'
        if img.size[0] != TEX_SIZE:
            img.scale(TEX_SIZE, TEX_SIZE)
        px = np.empty(TEX_SIZE * TEX_SIZE * 4, dtype=np.float32)
        img.pixels.foreach_get(px)
        bpy.data.images.remove(img)
        return px.reshape(TEX_SIZE, TEX_SIZE, 4)

    def save(arr, path, colour):
        # colour + masks as JPEG (q92, a third of the size), normals stay lossless PNG
        img = bpy.data.images.new("out", TEX_SIZE, TEX_SIZE, alpha=False)
        img.colorspace_settings.name = 'sRGB' if colour else 'Non-Color'
        img.pixels.foreach_set(np.ascontiguousarray(arr, dtype=np.float32).ravel())
        img.filepath_raw = path
        if path.endswith(".jpg"):
            img.file_format = 'JPEG'
            bpy.context.scene.render.image_settings.quality = 92
        else:
            img.file_format = 'PNG'
        img.save()
        bpy.data.images.remove(img)

    if "D" in tex:
        save(load(tex["D"], True), os.path.join(folder, "T_A07K_%s_D.jpg" % name), True)
    if "N" in tex:
        n = load(tex["N"], False)
        n[..., 1] = 1.0 - n[..., 1]          # Meshy/OpenGL (green up) -> Unreal/DirectX (green down)
        save(n, os.path.join(folder, "T_A07K_%s_N.png" % name), False)
    if "R" in tex or "M" in tex:
        mr = np.ones((TEX_SIZE, TEX_SIZE, 4), dtype=np.float32)
        mr[..., 1] = load(tex["R"], False)[..., 0] if "R" in tex else 0.7
        mr[..., 2] = load(tex["M"], False)[..., 0] if "M" in tex else 0.0
        save(mr, os.path.join(folder, "T_A07K_%s_MR.jpg" % name), False)


def measure_catwalk(obj):
    """Deck = the height with the most upward-facing surface; rails = the top of the model."""
    me = obj.data
    me.calc_loop_triangles()
    zs, areas, xs, ys = [], [], [], []
    for t in me.loop_triangles:
        if t.normal.z > 0.9:
            c = t.center
            zs.append(c.z)
            areas.append(t.area)
            xs.append(c.x)
            ys.append(c.y)
    zs, areas, xs, ys = map(np.array, (zs, areas, xs, ys))
    hist, edges = np.histogram(zs, bins=80, weights=areas)
    k = int(np.argmax(hist))
    deck_z = float((edges[k] + edges[k + 1]) / 2)
    on_deck = np.abs(zs - deck_z) < (edges[1] - edges[0]) * 1.5
    v = verts(obj)
    return {
        "deck_height_cm": round(deck_z * 100, 1),
        "deck_width_cm": round(float(np.percentile(ys[on_deck], 98) - np.percentile(ys[on_deck], 2)) * 100, 1),
        "deck_length_cm": round(float(np.percentile(xs[on_deck], 99.5) - np.percentile(xs[on_deck], 0.5)) * 100, 1),
        "top_cm": round(float(v[:, 2].max()) * 100, 1),
        "width_cm": round(float(v[:, 1].max() - v[:, 1].min()) * 100, 1),
        "length_cm": round(float(v[:, 0].max() - v[:, 0].min()) * 100, 1),
    }


def export(obj, name, folder):
    obj.name = obj.data.name = "SM_A07K_" + name
    if obj.data.materials:
        obj.data.materials[0].name = "M_A07K_" + name
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=os.path.join(folder, "SM_A07K_%s.fbx" % name), use_selection=True,
                             object_types={'MESH'}, mesh_smooth_type='FACE', path_mode='STRIP',
                             embed_textures=False, add_leaf_bones=False, bake_anim=False,
                             axis_forward='-Z', axis_up='Y')


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    src = os.path.abspath(args[0]) if args else os.path.join(ROOT, "Assets", "Meshy")
    only = set(args[1:])
    os.makedirs(OUT, exist_ok=True)
    report_path = os.path.join(OUT, "kit_report.json")
    report = json.load(open(report_path)) if os.path.isfile(report_path) else {}
    for key, words, name, axis, size_m, target in KIT:
        if only and name not in only:
            continue
        fbx = find_download(src, words)
        if not fbx:
            print("[A07 kit] %s: no download matching %s under %s - skipped" % (name, words, src))
            continue
        folder = os.path.join(OUT, name)
        os.makedirs(folder, exist_ok=True)
        obj = load_fbx(fbx)
        before = tri_count(obj)
        after = decimate(obj, target)
        dims = normalise(obj, axis, size_m)
        info = {"source": os.path.relpath(fbx, src), "tris_before": before, "tris_after": after,
                "size_cm": [round(float(d) * 100, 1) for d in dims]}
        if name == "Catwalk":
            info["catwalk"] = measure_catwalk(obj)
        write_textures(textures_next_to(fbx), name, folder)
        export(obj, name, folder)
        report[name] = info
        print("[A07 kit] %s: %d -> %d tris, %s cm" % (name, before, after, info["size_cm"]))
    with open(report_path, "w") as f:
        json.dump(report, f, indent=2)
    print("[A07 kit] wrote " + report_path)


main()
