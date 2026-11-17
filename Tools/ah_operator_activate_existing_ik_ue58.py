"""UE 5.8 Editor only: point the existing native operator IK rig at the imported arms.

This is non-destructive. It does NOT edit source code, modify levels,
overwrite assets or create any equipment.
Run in the Unreal Editor: Tools > Execute Python Script.
"""
import unreal

SOURCE = '/Game/character'
DEST_FOLDER = '/Game/Generated/Operator'
DEST = DEST_FOLDER + '/SK_AH_ContainmentOperator_FP'
EAL = unreal.EditorAssetLibrary

def log(message):
    unreal.log('[AH Operator IK] ' + message)

def main():
    if EAL.does_asset_exist(DEST):
        log('Expected operator mesh already exists. No changes: '+DEST)
        log('Open it and verify that the correct rigged v0.3 skeleton is assigned.')
        return
    if not EAL.does_asset_exist(SOURCE):
        unreal.log_error('[AH Operator IK] Imported source mesh not found: '+SOURCE)
        unreal.log_error('Edit SOURCE to the actual skeletal mesh path in your Content Browser.')
        return
    mesh = EAL.load_asset(SOURCE)
    if not mesh or 'SkeletalMesh' not in mesh.get_class().get_name():
        unreal.log_error('[AH Operator IK] Source is not a SkeletalMesh. No changes.')
        return
    if not EAL.does_directory_exist(DEST_FOLDER):
        EAL.make_directory(DEST_FOLDER)
    created = EAL.duplicate_asset(SOURCE, DEST)
    if not created:
        unreal.log_error('[AH Operator IK] Duplication failed.')
        return
    EAL.save_loaded_asset(created)
    log('Duplicated imported arms into: '+DEST)
    log('Original '+SOURCE+' remains unchanged.')
    log('Restart Play to allow the existing OperatorArmsComponent to load it.')
    log('Inspect Output Log for any arm rig calibration errors.')

main()
