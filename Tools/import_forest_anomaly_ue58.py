"""Safe, opt-in forest anomaly importer for Unreal Engine 5.8 Editor.

Works in the same project as build_forest_research_ue58.py.
Static preview OBJ is committed to Git. Skeletal FBXs are generated
locally using Tools/build_forest_anomaly_blender.py (Blender required).

Does not replace existing characters, maps, materials or actor defaults.
"""
import os
import unreal

ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PREVIEW=os.path.join(ROOT,"Assets","ForestAnomaly","SK_AH_ForestAnomaly_Preview.obj")
ANIM_DIR=os.path.join(ROOT,"Tools","Exports","ForestAnomaly")
FOLDER="/Game/Characters/ForestAnomaly"
ANIM_DST=FOLDER+"/Animations"
SKEL_NAME="SK_AH_ForestAnomaly"
PREVIEW_NAME="SK_AH_ForestAnomaly_Preview"
ANIMS=["Idle","Walk","Run","Turn","Alert","Investigate","Reach"]

library=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()

def log(txt):
    unreal.log("[AH ForestAnomaly] "+str(txt))

def makefolder(path):
    if not library.does_directory_exist(path):
        library.make_directory(path)

def generic_import(path,dest,name,options=None):
    if library.does_asset_exist(dest+"/"+name):
        log("Keeping existing asset: "+dest+"/"+name)
        return library.load_asset(dest+"/"+name)
    if not os.path.isfile(path):
        log("Missing source (skipped): "+path)
        return None
    task=unreal.AssetImportTask()
    task.set_editor_property("filename",path)
    task.set_editor_property("destination_path",dest)
    task.set_editor_property("destination_name",name)
    task.set_editor_property("replace_existing",False)
    task.set_editor_property("automated",True)
    task.set_editor_property("save",True)
    if options:task.set_editor_property("options",options)
    tools.import_asset_tasks([task])
    found=library.load_asset(dest+"/"+name)
    if not found:
        unreal.log_warning("[AH ForestAnomaly] Import may require manual setup: "+name)
    return found

def fbx_skeletal_options():
    ui=unreal.FbxImportUI()
    ui.set_editor_property("import_as_skeletal",True)
    ui.set_editor_property("import_mesh",True)
    ui.set_editor_property("import_animations",False)
    ui.set_editor_property("create_physics_asset",True)
    ui.set_editor_property("import_materials",False)
    ui.set_editor_property("import_textures",False)
    return ui

def fbx_animation_options(skeleton):
    ui=unreal.FbxImportUI()
    ui.set_editor_property("import_as_skeletal",True)
    ui.set_editor_property("import_mesh",False)
    ui.set_editor_property("import_animations",True)
    ui.set_editor_property("skeleton",skeleton)
    ui.set_editor_property("import_materials",False)
    ui.set_editor_property("import_textures",False)
    return ui

def main():
    makefolder(FOLDER)
    makefolder(ANIM_DST)
    preview=generic_import(PREVIEW,FOLDER,PREVIEW_NAME)
    if preview: log("Static concept preview available in "+FOLDER)

    skeletal_path=os.path.join(ANIM_DIR,SKEL_NAME+".fbx")
    if not os.path.isfile(skeletal_path):
        log("Rigged FBX not generated yet. Run Blender script first.")
        log("For now, preview only the separate static OBJ. No enemy AI was changed.")
        return

    try:
        skin=generic_import(skeletal_path,FOLDER,SKEL_NAME,fbx_skeletal_options())
        if not skin or skin.get_class().get_name()!="SkeletalMesh":
            log("Skeletal import incomplete; review UE 5.8 Interchange dialog/log.")
            return
        skeleton=skin.get_editor_property("skeleton")
        if not skeleton:
            log("No Skeleton asset detected, cannot import animations yet.")
            return
        for name in ANIMS:
            source=os.path.join(ANIM_DIR,"AN_AH_FA_"+name+".fbx")
            try:
                generic_import(source,ANIM_DST,"AN_AH_FA_"+name,
                               fbx_animation_options(skeleton))
            except Exception as exc:
                unreal.log_warning("[AH ForestAnomaly] %s import requires manual FBX setup: %s"%(name,exc))
        log("Done. Preview animation assets on the imported skeleton before making an enemy BP.")
        log("Behaviour remains untouched; create a Blueprint child of AEnemy or ASmartEnemy to assign this mesh.")
    except Exception as exc:
        unreal.log_error("[AH ForestAnomaly] Import interrupted: "+str(exc))

main()
