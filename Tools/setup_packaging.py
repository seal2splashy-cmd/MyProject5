"""
setup_packaging.py - get MyProject5 ready to package as a standalone game (.exe). Plain Python:

    cd /c/UE5Projects/MyProject5
    python Tools/setup_packaging.py

Edits your local Config files (only adds/changes the lines below, keeps everything else; run it as
often as you like):

  Config/DefaultEngine.ini   GameDefaultMap = AH_ForestResearch_A07_Expanded  (the .exe starts there;
                             the editor still opens TestArena)
  Config/DefaultGame.ini     [/Script/UnrealEd.ProjectPackagingSettings]
                             - always cook /Game/Generated, /Game/ForestResearch, /Game/Characters:
                               the game loads sounds, the lens material, the Gaunt, the arms, the
                               weapons... by path at runtime, and Unreal only packages what something
                               references directly - without this the .exe would be silent and broken
                             - cook the A-07 map
                             - Shipping build, packaged into a single .pak

Then in the editor: Platforms > Windows > Package Project, pick a folder. See Docs/MENUS_PACKAGING.md.
"""
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME_MAP = "/Game/Maps/AH_ForestResearch_A07_Expanded.AH_ForestResearch_A07_Expanded"
COOK_DIRS = ["/Game/Generated", "/Game/ForestResearch", "/Game/Characters"]
COOK_MAPS = ["/Game/Maps/AH_ForestResearch_A07_Expanded"]


def edit_section(text, section, lines_to_have, replace_keys=()):
    """Make sure [section] exists and contains each line; lines whose key is in replace_keys are replaced."""
    header = "[%s]" % section
    if header not in text:
        text = text.rstrip("\n") + "\n\n" + header + "\n"
    start = text.index(header) + len(header)
    nxt = re.search(r"^\[", text[start:], re.M)
    end = start + nxt.start() if nxt else len(text)
    body = text[start:end]
    for key in replace_keys:
        body = re.sub(r"^%s=.*\n?" % re.escape(key), "", body, flags=re.M)
    for line in lines_to_have:
        if line not in body:
            body = body.rstrip("\n") + "\n" + line + "\n"
    if not body.endswith("\n\n") and nxt:
        body += "\n"
    return text[:start] + body + text[end:]


def main():
    engine = os.path.join(ROOT, "Config", "DefaultEngine.ini")
    game = os.path.join(ROOT, "Config", "DefaultGame.ini")

    with open(engine, encoding="utf-8") as f:
        text = f.read()
    text = edit_section(text, "/Script/EngineSettings.GameMapsSettings",
                        ["GameDefaultMap=" + GAME_MAP], replace_keys=["GameDefaultMap"])
    with open(engine, "w", encoding="utf-8") as f:
        f.write(text)
    print("DefaultEngine.ini: GameDefaultMap -> A-07 map")

    with open(game, encoding="utf-8") as f:
        text = f.read()
    lines = ['+DirectoriesToAlwaysCook=(Path="%s")' % d for d in COOK_DIRS]
    lines += ['+MapsToCook=(FilePath="%s")' % m for m in COOK_MAPS]
    lines += ["BuildConfiguration=PPBC_Shipping", "UsePakFile=True", "bShareMaterialShaderCode=True"]
    text = edit_section(text, "/Script/UnrealEd.ProjectPackagingSettings", lines,
                        replace_keys=["BuildConfiguration", "UsePakFile"])
    with open(game, "w", encoding="utf-8") as f:
        f.write(text)
    print("DefaultGame.ini: always cook %s, cook the A-07 map, Shipping .pak" % ", ".join(COOK_DIRS))
    print("Now: open the editor, Platforms > Windows > Package Project.")


main()
