"""
build_bodycam_fx_ue58.py - run INSIDE the Unreal Editor (Tools > Execute Python Script...).

Makes the two assets the bodycam look uses (C++ skips them silently if they're missing):
  1. /Game/Generated/PostProcess/M_BodycamLens  - post-process material: fisheye-style
     barrel distortion + the footage glitches, all driven by UBodycamLensComponent:
       Distortion  barrel bulge
       Skew        rolling shutter: rows are read top to bottom, so a fast pan shears the picture
       Squash      rolling shutter on a fast tilt: the picture stretches / squashes vertically
       Blocks      fraction of compression macroblocks that fetch from the wrong place (0..1)
       Tear, TearY a band of rows slipping sideways, and where it is (0..1 down the screen)
       Seed        changes which blocks break
  2. /Game/Generated/Audio/SW_Torch_Click       - torch switch click (Assets/Audio/SW_Torch_Click.wav)

Safe to re-run: it replaces what it made before and touches nothing else (no maps, no
other assets). Tools/ue_setup_project.py also runs this at the end.
"""

import os
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
PP_DST = "/Game/Generated/PostProcess"
AUDIO_DST = "/Game/Generated/Audio"
LENS_NAME = "M_BodycamLens"
CLICK_WAV = os.path.join(PROJECT_DIR, "Assets", "Audio", "SW_Torch_Click.wav")

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

# Barrel distortion. Output pixel at viewport UV -> where to sample the rendered frame.
# r2 is normalised so the screen corners are 1; the zoom keeps the corners exactly in
# place, so nothing outside the frame is ever sampled. Centre is magnified by (1+K).
BARREL_HLSL = """
float2 c = UV - 0.5;
float aspect = Size.x / max(Size.y, 1.0);
float2 a = c * float2(aspect, 1.0);
float r2 = dot(a, a) / (0.25 * (aspect * aspect + 1.0));
float k = max(K, 0.0);
float2 s = c * (1.0 + k * r2) / (1.0 + k) + 0.5;
s.x += Skew * (UV.y - 0.5);
s.y = (s.y - 0.5) * (1.0 + Squash) + 0.5;
float2 blk = floor(UV * float2(32.0, 18.0));
float h = frac(sin(dot(blk + floor(Seed * 7.0), float2(12.9898, 78.233))) * 43758.5453);
if (h < Blocks) { s += (float2(frac(h * 91.7), frac(h * 37.3)) - 0.5) * 0.12; }
float band = 1.0 - smoothstep(0.0, 0.015, abs(UV.y - TearY));
s.x += band * Tear;
return saturate(s);
"""


def log(msg):
    unreal.log("[Bodycam FX] " + msg)


def warn(msg):
    unreal.log_warning("[Bodycam FX] " + msg)


def try_set(obj, prop, value):
    try:
        obj.set_editor_property(prop, value)
        return True
    except Exception as e:  # property names drift between engine versions
        warn("couldn't set %s: %s" % (prop, e))
        return False


def build_lens_material():
    path = PP_DST + "/" + LENS_NAME
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    if not eal.does_directory_exist(PP_DST):
        eal.make_directory(PP_DST)

    mat = asset_tools.create_asset(LENS_NAME, PP_DST, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    # Default blendable location (after tonemapping) is what we want: warp the finished image.

    screen = mel.create_material_expression(mat, unreal.MaterialExpressionScreenPosition, -900, 0)
    size = mel.create_material_expression(mat, unreal.MaterialExpressionViewSize, -900, 160)
    k = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 300)
    k.set_editor_property("parameter_name", "Distortion")
    k.set_editor_property("default_value", 0.18)

    custom = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -550, 100)
    custom.set_editor_property("description", "BodycamBarrel")
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    custom.set_editor_property("code", BARREL_HLSL.strip())
    inputs = []
    for name in ("UV", "Size", "K", "Skew", "Squash", "Blocks", "Seed", "Tear", "TearY"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)

    # glitch parameters (all 0 = clean picture)
    glitch = {}
    for i, (name, default) in enumerate((("Skew", 0.0), ("Squash", 0.0), ("Blocks", 0.0), ("Seed", 0.0),
                                         ("Tear", 0.0), ("TearY", 0.5))):
        p = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 440 + i * 110)
        p.set_editor_property("parameter_name", name)
        p.set_editor_property("default_value", default)
        glitch[name] = p

    scene = mel.create_material_expression(mat, unreal.MaterialExpressionSceneTexture, -250, 100)
    scene.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    try_set(scene, "filtered", True)

    ok = True
    ok &= mel.connect_material_expressions(screen, "ViewportUV", custom, "UV")
    ok &= mel.connect_material_expressions(size, "", custom, "Size")
    ok &= mel.connect_material_expressions(k, "", custom, "K")
    for name, p in glitch.items():
        ok &= mel.connect_material_expressions(p, "", custom, name)
    ok &= mel.connect_material_expressions(custom, "", scene, "UVs")
    ok &= mel.connect_material_property(scene, "Color", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if not ok:
        warn("some material pins didn't connect - open %s and check the graph" % path)

    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log("built " + path)


def import_click():
    if not os.path.isfile(CLICK_WAV):
        warn("missing %s - run: python Tools/generate_assets.py" % CLICK_WAV)
        return
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", CLICK_WAV)
    task.set_editor_property("destination_path", AUDIO_DST)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    asset_tools.import_asset_tasks([task])
    log("imported " + AUDIO_DST + "/SW_Torch_Click")


def main():
    build_lens_material()
    import_click()
    log("done - press Play. F toggles the torch; tune it all on the pawn's BodycamLens / Torch components.")


main()
