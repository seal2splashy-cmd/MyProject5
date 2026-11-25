"""Stage the user's four Meshy A-07 ZIP packages as original source files.

By default searches the Windows Downloads folder (Path.home()/Downloads).
Optionally supply the folder containing the four ZIPs, e.g.:
    python Tools/prepare_a07_meshy_modules.py "C:/Users/yourname/Downloads"
    python Tools/prepare_a07_meshy_modules.py "C:/Users/yourname/Downloads" --check

No 3D geometry or textures are generated. The original FBX and four PBR PNGs
from each ZIP are copied unchanged, and existing files are never overwritten.
"""
from pathlib import Path
import sys
import zipfile
import zlib

ROOT = Path(__file__).resolve().parent.parent
DEST = ROOT / "Assets" / "MeshyFacility"
PACKS = {
    "containment_mounting_.zip": "MountingPlatform",
    "elevated_catwalk_sect.zip": "Catwalk",
    "damaged_concrete_wall.zip": "DamagedWall",
    "containment_entrance_.zip": "ContainmentEntrance",
}
VALID_EXT = {".fbx", ".png"}


def crc32_file(path):
    checksum = 0
    with path.open("rb") as src:
        while True:
            block = src.read(4 * 1024 * 1024)
            if not block:
                break
            checksum = zlib.crc32(block, checksum)
    return checksum & 0xffffffff


def main():
    args = [a for a in sys.argv[1:] if a != "--check"]
    check_only = "--check" in sys.argv[1:]
    if len(args) > 1:
        raise SystemExit("Usage: python Tools/prepare_a07_meshy_modules.py [ZIP_directory] [--check]")
    folder = Path(args[0]).expanduser() if args else (Path.home() / "Downloads")
    folder = folder.resolve()
    if not folder.is_dir():
        raise SystemExit("ZIP folder not found: " + str(folder))
    # First check every archive before writing anything.
    planned = []
    for zipname, subfolder in PACKS.items():
        path = folder / zipname
        if not path.is_file():
            raise SystemExit("Missing " + str(path))
        with zipfile.ZipFile(path) as z:
            entries = [i for i in z.infolist() if not i.is_dir()]
            if len(entries) != 5:
                raise ValueError("%s should contain 1 FBX + 4 PNGs (found %s files)"
                                 % (zipname, len(entries)))
            for info in entries:
                parts = info.filename.replace("\\", "/").split("/")
                if (len(parts) != 2 or any(p in ("", ".", "..") for p in parts)
                        or Path(parts[1]).suffix.lower() not in VALID_EXT
                        or info.file_size <= 0 or info.file_size > 250_000_000):
                    raise ValueError("Unexpected archive member: " + info.filename)
            if sum(Path(i.filename).suffix.lower() == ".fbx" for i in entries) != 1:
                raise ValueError(zipname + " must contain exactly one FBX")
            if sum(Path(i.filename).suffix.lower() == ".png" for i in entries) != 4:
                raise ValueError(zipname + " must contain exactly four PNGs")
            planned.extend((zipname, subfolder, info.filename, info.file_size) for info in entries)

    for zipname, subfolder, member, size in planned:
        print("%s -> Assets/MeshyFacility/%s/%s (%0.1f MiB)"
              % (zipname, subfolder, Path(member).name, size / (1024 * 1024)))
    if check_only:
        print("Check only: no files copied.")
        return

    made = 0
    for zipname, subfolder in PACKS.items():
        destination = DEST / subfolder
        destination.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(folder / zipname) as z:
            for info in z.infolist():
                if info.is_dir():
                    continue
                target = destination / Path(info.filename.replace("\\", "/")).name
                if target.exists():
                    if target.stat().st_size != info.file_size or crc32_file(target) != info.CRC:
                        raise FileExistsError("Different file already exists; refusing overwrite: " + str(target))
                    print("UNCHANGED", target)
                    continue
                try:
                    with z.open(info) as src, target.open("xb") as out:
                        while True:
                            chunk = src.read(4 * 1024 * 1024)
                            if not chunk:
                                break
                            out.write(chunk)
                except Exception:
                    target.unlink(missing_ok=True)
                    raise
                made += 1
                print("COPIED", target)
    print("Completed: %d new source files. Use Git LFS to push Assets/MeshyFacility only." % made)


if __name__ == "__main__":
    main()
