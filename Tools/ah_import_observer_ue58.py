"""
Import Anomaly Hunter Observer rig and animation FBXs into UE 5.8.

Run Blender export generator first, then use Tools > Execute Python Script...
to run this from the Unreal Editor.

No existing enemy C++, AI parameters, maps, or meshes are overwritten.
FBX support can depend on whether UE 5.8 is using legacy FBX or Interchange:
check the Output Log and verify skeleton/animation types manually afterwards.
"""
import os
import unreal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "Assets", "ForestResearch", "Enemy", "Exports")
DST = "/Game/AH_ForestResearch/Enemy"
MESH = DST + "/SK_AH_Observer"
NAMES=("Idle","Walk","Run","Turn","Alert","Investigate","Reach","Retreat")
eal=unreal.EditorAssetLibrary
assettools=unreal.AssetToolsHelpers.get_asset_tools()

def setprop(ob,name,value):
    try:
        ob.set_editor_property(name,value)
        return True
    except Exception as exc:
        unreal.log_warning("[AH Observer] %s cannot be assigned: %s"%(name,exc))
        return False

def imported(path,name,options):
    if eal.does_asset_exist(DST+"/"+name):
        unreal.log("[AH Observer] Keeping existing "+name)
        return
    task=unreal.AssetImportTask()
    setprop(task,"filename",path)
    setprop(task,"destination_path",DST)
    setprop(task,"destination_name",name)
    setprop(task,"automated",True)
    setprop(task,"replace_existing",False)
    setprop(task,"save",True)
    if options:
        setprop(task,"options",options)
    assettools.import_asset_tasks([task])
    unreal.log("[AH Observer] Imported candidate: "+str(task.get_editor_property("imported_object_paths")))

def mesh_options():
    opts=unreal.FbxImportUI()
    setprop(opts,"import_as_skeletal",True)
    setprop(opts,"mesh_type_to_import",unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    setprop(opts,"import_mesh",True)
    setprop(opts,"import_animations",False)
    setprop(opts,"import_materials",True)
    setprop(opts,"import_textures",False)
    setprop(opts,"create_physics_asset",False)
    return opts

def animation_options(skeleton):
    opts=unreal.FbxImportUI()
    setprop(opts,"import_as_skeletal",True)
    setprop(opts,"mesh_type_to_import",unreal.FBXImportType.FBXIT_ANIMATION)
    setprop(opts,"import_mesh",False)
    setprop(opts,"import_animations",True)
    setprop(opts,"skeleton",skeleton)
    return opts

def main():
    meshfbx=os.path.join(SRC,"SK_AH_Observer.fbx")
    if not os.path.isfile(meshfbx):
        unreal.log_error("[AH Observer] Run the Blender model/animation generator first: "+meshfbx)
        return
    if not eal.does_directory_exist(DST):
        eal.make_directory(DST)
    imported(meshfbx,"SK_AH_Observer",mesh_options())
    mesh=eal.load_asset(MESH)
    if not mesh or mesh.get_class().get_name()!="SkeletalMesh":
        unreal.log_error("[AH Observer] Skeletal mesh import failed or imported with a different name.")
        unreal.log_error("Use Content Browser > Import on SK_AH_Observer.fbx, select Skeletal Mesh.")
        return
    skeleton=mesh.get_editor_property("skeleton")
    if not skeleton:
        unreal.log_error("[AH Observer] No Skeleton assigned. Resolve before importing clips.")
        return
    for motion in NAMES:
        fbx=os.path.join(SRC,"AN_AH_Observer_"+motion+".fbx")
        if not os.path.isfile(fbx):
            unreal.log_warning("[AH Observer] Missing animation FBX: "+fbx)
            continue
        imported(fbx,"AN_AH_Observer_"+motion,animation_options(skeleton))
    unreal.log("[AH Observer] Import finished. Check Skeleton Tree and previews.")
    unreal.log("[AH Observer] No enemy behaviour or gameplay logic was changed.")
    unreal.log("[AH Observer] Create a child BP of AEnemy and assign mesh/AnimBP manually when ready.")

main()
