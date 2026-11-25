# Menus, settings and packaging a playable .exe

## Menus (AAHPlayerController + AMyHUD, no UMG / Blueprints)

| Menu | When | Items |
|---|---|---|
| Title | once per session, when the game starts (paused behind it) | CONTINUE (if there's a checkpoint) / NEW GAME / SETTINGS / QUIT GAME |
| Pause | **Esc** (in the editor use **P** - Esc stops Play-In-Editor) | RESUME / SETTINGS / RELOAD LAST CHECKPOINT / QUIT TO TITLE / QUIT GAME |
| Settings | from either | MOUSE SENSITIVITY 0.2-3.0, INVERT MOUSE Y, FIELD OF VIEW 80-115, MASTER VOLUME, BRIGHTNESS -2..+2 EV, BODYCAM EFFECTS full / reduced / off, MOTION BLUR, BACK |

- Mouse: hover to select, click; on a setting click its left half to lower / right half to raise.
- Keyboard: Up/Down (W/S), Left/Right (A/D), Enter/Space, Esc/P back.
- Settings save straight away to `Saved/SaveGames/AHSettings.sav` and apply live.
- Skip the title while testing: console `ah.SkipTitle 1`.
- QUIT TO TITLE keeps your checkpoint (CONTINUE); NEW GAME clears it.

## Packaging

1. Make sure the A-07 map is saved and everything is imported (the build / setup / dress / kit /
   retexture / story scripts, `Tools/build_bodycam_fx_ue58.py`, `Tools/import_audio_ue58.py`).
2. In Git Bash:
   ```
   cd /c/UE5Projects/MyProject5
   python Tools/setup_packaging.py
   ```
   It edits your local `Config/DefaultEngine.ini` (the .exe starts on the A-07 map) and
   `Config/DefaultGame.ini` (always cook /Game/Generated, /Game/ForestResearch, /Game/Characters - the
   game loads sounds, the lens, the Gaunt, the arms, the weapons by path, and Unreal only packages what's
   referenced directly; cook the A-07 map; Shipping, one .pak). Safe to run again.
3. Restart the editor, then **Platforms > Windows > Package Project**, pick an empty folder.
   First time takes a while (shaders). You get `Windows/MyProject5.exe` - zip the `Windows` folder to
   share it.
4. Something missing in the .exe that works in the editor? It's loaded by path from a folder that isn't
   cooked: add it under Project Settings > Packaging > Additional Asset Directories to Cook.
