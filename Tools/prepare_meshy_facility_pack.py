"""Unpack the user's Meshy ZIP into Git-LFS-ready source folders.

Usage (Git Bash from repository root):
  python Tools/prepare_meshy_facility_pack.py "/c/Users/<you>/Downloads/Meshy_models_20261119_073604.zip"
Or use a normal Windows path as the argument.

Only copies files from three recognised Meshy folders. Will not replace
pre-existing files; archives with unsafe paths or unusually large files are
rejected. Meshy-made art and material textures are never generated here.
"""
from pathlib import Path
import sys
import zipfile

DEST = Path(__file__).resolve().parent.parent / "Assets" / "MeshyFacility"
FOLDERS = {
    "ContainmentVessel": "ContainmentVessel",
    "DeconChamberHorizontal": "DeconChamber",
    "LabControlConsole": "ControlConsole",
}

def main():
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python Tools/prepare_meshy_facility_pack.py <meshy_zip_path>")
    zpath = Path(sys.argv[1]).expanduser()
    if not zpath.is_file():
        raise SystemExit("ZIP not found: " + str(zpath))
    extracted = 0
    with zipfile.ZipFile(zpath) as z:
        for info in z.infolist():
            if info.is_dir():
                continue
            parts = Path(info.filename.replace("\\", "/")).parts
            if len(parts) != 2 or info.file_size > 200_000_000:
                raise ValueError("Unexpected file path/size in ZIP: " + info.filename)
            if not info.filename.lower().endswith((".fbx", ".png")):
                continue
            folder = next((v for k,v in FOLDERS.items() if k in parts[0]), None)
            if not folder:
                raise ValueError("Unknown Meshy asset directory: " + parts[0])
            parent = DEST / folder
            parent.mkdir(parents=True, exist_ok=True)
            target = parent / parts[1]
            if target.exists():
                if target.stat().st_size != info.file_size:
                    raise FileExistsError("Existing different-size asset, refusing overwrite: " + str(target))
                print("SKIP existing", target)
                continue
            with z.open(info) as src, target.open("xb") as dst:
                import shutil
                shutil.copyfileobj(src, dst, 8 * 1024 * 1024)
            extracted += 1
            print("EXTRACTED", target.relative_to(DEST.parent.parent))
    print("Ready:", extracted, "new files in", DEST)
    print("Stage only: git add Assets/MeshyFacility")

if __name__ == "__main__":
    main()
