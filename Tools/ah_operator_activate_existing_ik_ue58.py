"""UE 5.8 Editor only: point the existing native operator IK rig at the imported arms.

This is non-destructive. It does NOT edit source code, modify levels,
overwrite assets or create any equipment.
Run in the Unreal Editor: Tools > Execute Python Script.

Normal route: run Tools/ue_setup_project.py instead. It imports the fixed rig from
Assets/OperatorArms straight to DEST, so this helper then has nothing to do.
Only set SOURCE if you imported the operator arms by hand somewhere else.
"""
import unreal

# Skeletal mesh you imported by hand, e.g. '/Game/MyImports/SK_AH_ContainmentOperator_FP'.
# Leave empty if you use Tools/ue_setup_project.py.
SOURCE = ''
# The old Mixamo arms the pawn falls back to. They are not the operator rig (different
# bones), so copying them to DEST would make UOperatorArmsComponent fail calibration.
LEGACY_ARMS = '/Game/character'
DEST_FOLDER = '/Game/Generated/Operator'
DEST = DEST_FOLDER + '/SK_AH_ContainmentOperator_FP'
EAL = unreal.EditorAssetLibrary

def log(message):
    unreal.log('[AH Operator IK] ' + message)

def error(message):
    unreal.log_error('[AH Operator IK] ' + message)

def main():
    if EAL.does_asset_exist(DEST):
        log('Expected operator mesh already exists. No changes: '+DEST)
        log('Open it and verify that the correct rigged v0.3 skeleton is assigned.')
        return
    if not SOURCE:
        error('Operator mesh is not imported yet: '+DEST)
        error('Run Tools/ue_setup_project.py (it imports Assets/OperatorArms/SK_AH_ContainmentOperator_FP.fbx there),')
        error('or set SOURCE in this script to the operator skeletal mesh you imported by hand.')
        return
    if SOURCE.split('.')[0].rstrip('/') == LEGACY_ARMS:
        error(LEGACY_ARMS+' is the old Mixamo arms mesh, not the operator rig. No changes.')
        error('Run Tools/ue_setup_project.py, or set SOURCE to the operator skeletal mesh.')
        return
    if not EAL.does_asset_exist(SOURCE):
        error('Imported source mesh not found: '+SOURCE)
        error('Edit SOURCE to the actual skeletal mesh path in your Content Browser.')
        return
    mesh = EAL.load_asset(SOURCE)
    if not mesh or 'SkeletalMesh' not in mesh.get_class().get_name():
        error('Source is not a SkeletalMesh. No changes.')
        return
    if not EAL.does_directory_exist(DEST_FOLDER):
        EAL.make_directory(DEST_FOLDER)
    created = EAL.duplicate_asset(SOURCE, DEST)
    if not created:
        error('Duplication failed.')
        return
    EAL.save_loaded_asset(created)
    log('Duplicated imported arms into: '+DEST)
    log('Original '+SOURCE+' remains unchanged.')
    log('Restart Play to allow the existing OperatorArmsComponent to load it.')
    log('Inspect Output Log for any arm rig calibration errors.')

main()
