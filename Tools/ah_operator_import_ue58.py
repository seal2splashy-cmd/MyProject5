"""Anomaly Hunter UE 5.8 operator asset importer (EDITOR-ONLY).

Place this script inside MyProject5/Tools/ and run Tools > Execute Python Script.
This script only CREATES new assets in /Game/Characters/Operator_FP.
It does not delete existing content, modify levels or edit C++.

Copy the generated animation FBXs to:
  MyProject5/Assets/OperatorFP/Animations/
Copy the v0.3 textures to:
  MyProject5/Assets/OperatorFP/Textures/
Place the fictional containment scanner GLB in:
  MyProject5/Assets/OperatorFP/Scanner/

Set DRY_RUN=True to inspect the planned actions without importing anything.
"""
import os
import unreal

DRY_RUN = False
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJECT, 'Assets', 'OperatorFP')
ROOT = '/Game/Characters/Operator_FP'
ANIM_DEST = ROOT + '/Animations'
TEX_DEST = ROOT + '/Textures'
MAT_DEST = ROOT + '/Materials'
PROPS_DEST = ROOT + '/Props'
# The operator arms skeletal mesh the animations are imported against. This is where
# Tools/ue_setup_project.py puts it and where UOperatorArmsComponent loads it from.
# (Not '/Game/character': in MyProject5 that is the old Mixamo arms, a different skeleton.)
MESH_ASSET_PATH = '/Game/Generated/Operator/SK_AH_ContainmentOperator_FP'

EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary


def log(msg): unreal.log('[AH Operator v0.4] ' + msg)
def warning(msg): unreal.log_warning('[AH Operator v0.4] ' + msg)

def safe_set(obj, key, val):
    try: obj.set_editor_property(key, val); return True
    except Exception as exc:
        warning(f'Cannot set {key}: {exc}')
        return False


def ensure_folder(folder):
    if not EAL.does_directory_exist(folder) and not DRY_RUN:
        EAL.make_directory(folder)


def import_one(filepath, dest, **opts):
    name = os.path.splitext(os.path.basename(filepath))[0]
    ensure_folder(dest)
    target = dest+'/'+name
    if EAL.does_asset_exist(target):
        log('Already exists; preserving '+target)
        return EAL.load_asset(target)
    if DRY_RUN:
        log('Would import '+filepath+' into '+dest)
        return None
    task=unreal.AssetImportTask()
    safe_set(task,'filename',filepath)
    safe_set(task,'destination_path',dest)
    safe_set(task,'automated',True)
    safe_set(task,'replace_existing',False)
    safe_set(task,'save',True)
    for key,val in opts.items(): safe_set(task,key,val)
    AT.import_asset_tasks([task])
    result=task.get_editor_property('imported_object_paths') or []
    log('Imported '+name+': '+', '.join(map(str,result)))
    return EAL.load_asset(str(result[0])) if result else None


def material(stem, name, metallic):
    dest=MAT_DEST+'/'+name
    if EAL.does_asset_exist(dest):
        log('Keeping existing material '+dest)
        return
    sources={
        'BC':TEX_DEST+f'/T_{stem}_BC',
        'N':TEX_DEST+f'/T_{stem}_N',
        'R':TEX_DEST+f'/T_{stem}_R',
        'AO':TEX_DEST+f'/T_{stem}_AO',
    }
    if not EAL.does_asset_exist(sources['BC']):
        warning('Missing base-colour map for '+stem+'; skipping material')
        return
    if DRY_RUN:
        log('Would create material '+dest)
        return
    ensure_folder(MAT_DEST)
    mat=AT.create_asset(name,MAT_DEST,unreal.Material,unreal.MaterialFactoryNew())
    for i,(suffix,source_path) in enumerate(sources.items()):
        tex=EAL.load_asset(source_path)
        if not tex: continue
        smp=MEL.create_material_expression(mat,unreal.MaterialExpressionTextureSample,-380,i*220)
        safe_set(smp,'texture',tex)
        if suffix=='N':safe_set(smp,'sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        if suffix in ('R','AO'):safe_set(smp,'sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        prop={
            'BC':unreal.MaterialProperty.MP_BASE_COLOR,
            'N':unreal.MaterialProperty.MP_NORMAL,
            'R':unreal.MaterialProperty.MP_ROUGHNESS,
            'AO':unreal.MaterialProperty.MP_AMBIENT_OCCLUSION,
        }[suffix]
        MEL.connect_material_property(smp,'R' if suffix in ('R','AO') else 'RGB',prop)
    metallic_node=MEL.create_material_expression(mat,unreal.MaterialExpressionConstant,0,1100)
    safe_set(metallic_node,'r',metallic)
    MEL.connect_material_property(metallic_node,'',unreal.MaterialProperty.MP_METALLIC)
    MEL.layout_material_expressions(mat)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)
    log('Created '+dest)


def setup_textures():
    src=os.path.join(SOURCE,'Textures')
    if not os.path.isdir(src):
        warning('No textures at '+src+'; leave existing material setup unchanged.')
        return
    for f in sorted(os.listdir(src)):
        if not f.lower().endswith('.png'):continue
        t=import_one(os.path.join(src,f),TEX_DEST)
        if not t or DRY_RUN:continue
        if f.endswith('_N.png'):
            safe_set(t,'compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP)
            safe_set(t,'srgb',False)
        elif any(f.endswith(ext) for ext in ('_R.png','_AO.png','_Height.png')):
            safe_set(t,'compression_settings',unreal.TextureCompressionSettings.TC_MASKS)
            safe_set(t,'srgb',False)
        EAL.save_loaded_asset(t)
    for stem,mat,metallic in [
        ('ContainmentFabric','M_AH_Operator_Fabric',0.0),
        ('GlovePolymer','M_AH_Operator_Gloves',0.02),
        ('ForearmGuard','M_AH_Operator_Armour',0.12),
        ('Containment_Insignia','M_AH_Operator_Insignia',0.0),
    ]:
        material(stem,mat,metallic)


def setup_animations():
    mesh=EAL.load_asset(MESH_ASSET_PATH)
    if not mesh:
        warning('No mesh at '+MESH_ASSET_PATH+'. Run Tools/ue_setup_project.py first (it imports the operator arms), or fix MESH_ASSET_PATH.')
        return
    if 'SkeletalMesh' not in mesh.get_class().get_name():
        warning(MESH_ASSET_PATH+' is not a SkeletalMesh. Cannot import animation FBXs.')
        return
    try: skeleton=mesh.get_editor_property('skeleton')
    except Exception as exc:
        warning('Could not read skeleton: '+str(exc));return
    if not skeleton:
        warning('Mesh has no skeleton. Cannot import animation FBXs.')
        return
    src=os.path.join(SOURCE,'Animations')
    if not os.path.isdir(src):
        warning('No animation FBXs at '+src)
        return
    for name in sorted(os.listdir(src)):
        if not name.startswith('AH_FP_') or not name.lower().endswith('.fbx'):continue
        path=os.path.join(src,name)
        if DRY_RUN:
            log('Would import animation '+name)
            continue
        try:
            imp=unreal.FbxImportUI()
            safe_set(imp,'import_as_skeletal',True)
            safe_set(imp,'import_mesh',False)
            safe_set(imp,'import_animations',True)
            safe_set(imp,'import_materials',False)
            safe_set(imp,'import_textures',False)
            safe_set(imp,'skeleton',skeleton)
            enum_val=getattr(unreal.FBXImportType,'FBXIT_ANIMATION',None)
            if enum_val is not None:
                safe_set(imp,'mesh_type_to_import',enum_val)
            import_one(path,ANIM_DEST,options=imp)
        except Exception as exc:
            warning('Animation import failed for '+name+': '+str(exc))
            warning('Manual fallback: import as Animation, disable Import Mesh, select existing character skeleton.')


def setup_scanner():
    src=os.path.join(SOURCE,'Scanner','SM_AH_ContainmentScanner.glb')
    if os.path.isfile(src):
        try: import_one(src,PROPS_DEST)
        except Exception as exc:
            warning('GLB scanner import failed: '+str(exc))
            warning('Manual fallback: import scanner GLB from Content Browser.')
    else: warning('Scanner GLB absent: '+src)


def main():
    log('Engine project: '+PROJECT)
    log('No existing levels, C++ classes, pawns or assets will be overwritten.')
    for path in (ROOT,ANIM_DEST,TEX_DEST,MAT_DEST,PROPS_DEST):ensure_folder(path)
    setup_textures()
    setup_animations()
    setup_scanner()
    log('Done. Assign animation assets/material slots manually; see Docs/OPERATOR_V04_INTEGRATION.md')

main()
