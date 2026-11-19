"""
rebuild_materials_ue58.py - run INSIDE the Unreal Editor (Tools > Execute Python Script...).

Rebuilds only the generated level materials in /Game/Generated/Materials (M_Concrete_World,
M_FloorTiles_World, M_RustMetal_World, M_Wood_World, M_RustMetal, M_Wood) using the graphs in
Tools/ue_setup_project.py. Each material is cleared and rebuilt in place, so maps keep their
assignments. Nothing else is touched: no imports, no maps.

Use it after a fix to those graphs instead of re-running the whole setup.
"""

import os
import runpy
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
setup = runpy.run_path(os.path.join(PROJECT_DIR, "Tools", "ue_setup_project.py"),
                       init_globals={"MYPROJECT5_SETUP_AS_LIBRARY": True},
                       run_name="ue_setup_project_lib")

mats = setup["build_materials"]()
built = sorted(k for k, v in mats.items() if v)
missing = sorted(k for k, v in mats.items() if not v)
unreal.log("[Rebuild materials] rebuilt: " + ", ".join(built))
if missing:
    unreal.log_warning("[Rebuild materials] skipped (textures not imported, run ue_setup_project.py): "
                       + ", ".join(missing))
