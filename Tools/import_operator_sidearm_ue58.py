"""Import the stylized, nonfunctional Anomaly Hunter field sidearm into UE 5.8.

Usage: Open MyProject5 in the editor, select Tools > Execute Python Script
and choose this file. The script imports ONLY the sidearm OBJ into a fresh
folder. It does not touch maps, operator arms, gameplay, or existing assets.

The static mesh name is expected by AGun::BeginPlay on this feature branch.
"""
import os
import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
SRC = os.path.join(ROOT, 'Assets', 'OperatorSidearm', 'SM_AH_FieldSidearm.obj')
DST = '/Game/Generated/Operator/Props'
ASSET = DST + '/SM_AH_FieldSidearm'
eal = unreal.EditorAssetLibrary

def main():
    if eal.does_asset_exist(ASSET):
        unreal.log('[AH FieldSidearm] Already imported, keeping existing asset: ' + ASSET)
        return
    if not os.path.isfile(SRC):
        unreal.log_error('[AH FieldSidearm] OBJ source not found: ' + SRC)
        return

    if not eal.does_directory_exist(DST):
        eal.make_directory(DST)

    task = unreal.AssetImportTask()
    task.set_editor_property('filename', SRC)
    task.set_editor_property('destination_path', DST)
    task.set_editor_property('destination_name', 'SM_AH_FieldSidearm')
    task.set_editor_property('replace_existing', False)
    task.set_editor_property('automated', True)
    task.set_editor_property('save', True)

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    if not eal.does_asset_exist(ASSET):
        unreal.log_error('[AH FieldSidearm] No static mesh was imported. Check UE 5.8 Interchange import log.')
        return
    mesh = eal.load_asset(ASSET)
    if not mesh or mesh.get_class().get_name() != 'StaticMesh':
        unreal.log_error('[AH FieldSidearm] Imported asset is not a StaticMesh.')
        return

    unreal.log('[AH FieldSidearm] Success. Imported ' + ASSET)
    unreal.log('[AH FieldSidearm] Restart PIE to load the optional sidearm visual.')
    unreal.log('[AH FieldSidearm] If facing/scaling looks wrong, verify OBJ import transform: 1 unit = 1 cm, +X forward, +Z up.')
    unreal.log('[AH FieldSidearm] This is a nonfunctional game prop with no mechanical internals.')

main()
