"""Anomaly Hunter: A-07 extended facility v1, Unreal Engine 5.8 Editor script.

PREP: In the Content Browser DUPLICATE the existing
  /Game/Maps/AH_ForestResearch_MeshyPass01  (or AH_ForestResearch_Small)
and name the DUPLICATE:
  /Game/Maps/AH_ForestResearch_A07_Expanded
OPEN THAT DUPLICATE before running this script with Tools > Execute Python Script.

The script REFUSES to modify any other level. Never overwrites assets/maps.
This builds low-poly BLOCKOUT STRUCTURE and lights/interaction anchors only.
All finished detailed meshes are the user's own imported Meshy assets.
No original TestArena, forest map, code or game defaults are changed.
No auto-save: inspect in viewport, then Ctrl+S.
"""
import math
import re
import unreal

TARGET_WORLD = "AH_ForestResearch_A07_Expanded"
MARK = "AH_A07_EXPANSION_V1"
ROOT_MESH = "/Game/ForestResearch/MeshyExpansion"
MODULES = ROOT_MESH + "/Modules"
MAT = "/Game/ForestResearch/Materials"
FOLDER = unreal.Name("AnomalyHunter/A07_Expanded")

lib = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
cube = lib.load_asset("/Engine/BasicShapes/Cube")
mat_conc = lib.load_asset(MAT + "/M_FR_Concrete")
mat_floor = lib.load_asset(MAT + "/M_FR_Floor")
mat_steel = lib.load_asset(MAT + "/M_FR_Steel")
mat_dark = lib.load_asset(MAT + "/M_FR_Charcoal")
mat_accent = lib.load_asset(MAT + "/M_FR_SafetyYellow")
mat_glass = lib.load_asset(MAT + "/M_FR_FrostedGlass")

def log(msg): unreal.log("[AH A07] " + str(msg))
def warn(msg): unreal.log_warning("[AH A07] " + str(msg))

def setprop(obj, prop, value):
    try:
        obj.set_editor_property(prop, value)
        return True
    except Exception as exc:
        warn("Could not set %s: %s" % (prop, exc))
        return False

def with_tag(a, value=MARK):
    try:
        tags = list(a.get_editor_property("tags"))
        if unreal.Name(value) not in tags:
            tags.append(unreal.Name(value))
            setprop(a, "tags", tags)
    except Exception as exc:
        warn("Couldn't set actor tag: " + str(exc))

def spawn(cls, label, xyz, yaw=0.0):
    if label in EXISTING:
        return EXISTING[label]
    actor = actors.spawn_actor_from_class(
        cls, unreal.Vector(*xyz), unreal.Rotator(pitch=0, yaw=yaw, roll=0))
    if not actor:
        warn("Could not spawn " + label)
        return None
    actor.set_actor_label(label)
    with_tag(actor)
    try:
        actor.set_folder_path(FOLDER)
    except Exception:
        pass
    EXISTING[label] = actor
    return actor

def block(label, xyz, dimensions, material=None):
    a = spawn(unreal.StaticMeshActor, label, xyz)
    if not a:
        return None
    if a.static_mesh_component.get_editor_property("static_mesh") is None:
        a.static_mesh_component.set_static_mesh(cube)
    a.set_actor_scale3d(unreal.Vector(
        dimensions[0]/100.0, dimensions[1]/100.0, dimensions[2]/100.0))
    if material:
        a.static_mesh_component.set_material(0, material)
    return a

def wall_x(label, xa, xb, y, height=330, mat=mat_conc):
    if xb > xa:
        block(label, ((xa + xb)/2, y, height/2),
              (xb-xa, 28, height), mat)

def wall_y(label, x, ya, yb, height=330, mat=mat_conc):
    if yb > ya:
        block(label, (x, (ya + yb)/2, height/2),
              (28, yb-ya, height), mat)

def west_east_wall(label, x, ya, yb, hole=None, height=330):
    if not hole:
        wall_y(label, x, ya, yb, height)
        return
    cy, width = hole
    wall_y(label + "_A", x, ya, cy-width/2, height)
    wall_y(label + "_B", x, cy+width/2, yb, height)
    # 270-cm tall opening, capped by a header.
    if height > 270:
        block(label + "_Header", (x, cy, (height+270)/2),
              (34, width, height-270), mat_steel)

def north_south_wall(label, xa, xb, y, hole=None, height=330):
    if not hole:
        wall_x(label, xa, xb, y, height)
        return
    cx, width = hole
    wall_x(label + "_A", xa, cx-width/2, y, height)
    wall_x(label + "_B", cx+width/2, xb, y, height)
    if height > 270:
        block(label + "_Header", (cx, y, (height+270)/2),
              (width, 34, height-270), mat_steel)

def room(label, x0, x1, y0, y1, height=330,
         west=None, east=None, south=None, north=None):
    block(label+"_Floor", ((x0+x1)/2,(y0+y1)/2,-15),
          (x1-x0,y1-y0,30),mat_floor)
    block(label+"_Roof", ((x0+x1)/2,(y0+y1)/2,height+13),
          (x1-x0,y1-y0,26),mat_dark)
    west_east_wall(label+"_West", x0,y0,y1,west,height)
    west_east_wall(label+"_East", x1,y0,y1,east,height)
    north_south_wall(label+"_South",x0,x1,y0,south,height)
    north_south_wall(label+"_North",x0,x1,y1,north,height)

def ceiling_light(label, xyz, strength=850, colour=(.66,.82,1.0),
                  radius=660, powered=False):
    a = spawn(unreal.PointLight, label, xyz)
    if not a:
        return
    c = a.get_component_by_class(unreal.PointLightComponent)
    if c:
        c.set_intensity(strength)
        c.set_attenuation_radius(radius)
        c.set_light_color(unreal.LinearColor(*colour, 1.0))
        setprop(c,"cast_shadows",True)
        if powered:
            with_tag(a,"AH_PoweredLight")
            c.set_visibility(False)

def meshy(label, path, xy_bottom, max_size, yaw=0.0):
    mesh = lib.load_asset(path)
    if not mesh or not isinstance(mesh, unreal.StaticMesh):
        warn("Missing Meshy model: " + path + " | geometry placeholder NOT generated")
        return
    if label in EXISTING:
        return
    try:
        bounds = mesh.get_bounding_box()
        low, high = bounds.min, bounds.max
    except Exception as exc:
        warn("Cannot read Meshy bounds for %s: %s" % (path, exc))
        return
    sx,sy,sz = high.x-low.x, high.y-low.y, high.z-low.z
    if min(sx,sy,sz) <= 0.00001:
        warn("Invalid bounds: " + path)
        return
    factor = min(max_size[0]/sx, max_size[1]/sy, max_size[2]/sz)
    if not (0.00001 < factor < 10000):
        warn("Unreasonable scale: " + path)
        return
    midx,midy = (high.x+low.x)/2, (high.y+low.y)/2
    r=math.radians(yaw)
    xoff = (midx*math.cos(r)-midy*math.sin(r))*factor
    yoff = (midx*math.sin(r)+midy*math.cos(r))*factor
    x,y,z = xy_bottom
    a=spawn(unreal.StaticMeshActor,label,(x-xoff,y-yoff,z-low.z*factor),yaw)
    if not a:
        return
    a.static_mesh_component.set_static_mesh(mesh)
    a.set_actor_scale3d(unreal.Vector(factor,factor,factor))
    a.set_actor_enable_collision(False)
    a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    log("Meshy placed: %s | max envelope %s cm | factor %.3f" %
        (label, max_size, factor))

def interaction(label, action_key, pos, reach=240):
    cls = unreal.load_class(None, "/Script/MyProject5.FacilityInteractionPoint")
    enum_cls = getattr(unreal, "EFacilityAction", None)
    if not cls or enum_cls is None:
        warn("Mission C++ classes unavailable. Compile the project to spawn interactables.")
        return
    # Reflected enum names are usually UPPER_SNAKE_CASE in Unreal Python.
    upper = re.sub("([a-z0-9])([A-Z])", r"\1_\2", action_key).upper()
    val = None
    for key in (upper, action_key.upper(), action_key):
        try:
            val = getattr(enum_cls,key)
            break
        except AttributeError:
            pass
    if val is None:
        warn("Cannot resolve EFacilityAction." + upper)
        return
    p=spawn(cls,label,pos)
    if p:
        setprop(p,"action",val)
        setprop(p,"label",label.replace("A07_","").replace("_"," "))
        setprop(p,"reach_distance",float(reach))
        log("Interactable: G -> %s" % action_key)

def corridor(label,x0,x1,y0,y1,height=330):
    # The corridor is open at both x ends, unlike an enclosed room.
    block(label+"_Floor",((x0+x1)/2,(y0+y1)/2,-15),
          (x1-x0,y1-y0,30),mat_floor)
    block(label+"_Ceiling",((x0+x1)/2,(y0+y1)/2,height+13),
          (x1-x0,y1-y0,26),mat_dark)
    wall_x(label+"_South",x0,x1,y0,height)
    wall_x(label+"_North",x0,x1,y1,height)

def main():
    world = editor.get_editor_world() if editor else None
    actual = world.get_name() if world else "(none)"
    if actual != TARGET_WORLD:
        unreal.log_error(
            "[AH A07] REFUSED: you have '" + actual + "' open. "
            "Duplicate AH_ForestResearch_MeshyPass01 (or Small) as "
            + TARGET_WORLD + ", OPEN THE DUPLICATE, and rerun.")
        return

    # Ensure the original research-facility layout really exists in the copy.
    old_rear = EXISTING.get("FR_RearWall")
    if not old_rear:
        unreal.log_error("[AH A07] Missing FR_RearWall: this is not the original forest map. Aborting.")
        return
    if not cube:
        unreal.log_error("[AH A07] Unreal BasicShapes/Cube missing. Aborting.")
        return

    # In this COPY ONLY, reveal a 360-cm-wide gap at the existing back wall.
    # The rear wall stays as an actor, hidden/disabled for rollback.
    old_rear.set_actor_hidden_in_game(True)
    old_rear.set_actor_enable_collision(False)
    old_rear.static_mesh_component.set_visibility(False)
    wall_y("A07_OriginalRearSouth",4000,-1500,-190)
    wall_y("A07_OriginalRearNorth",4000,190,1500)
    block("A07_OriginalRearHeader",(4000,0,300),(35,380,60),mat_steel)

    # Main connection from the original 6-room facility.
    corridor("A07_Link",4000,4700,-230,230)
    # Restricted airlock: width 11m, length 13m, two aligned doors.
    room("A07_Airlock",4700,6000,-550,550,350,
         west=(0,370), east=(0,370))
    # Shared central service hall, with open ends.
    # Central hall north branch must be open toward Observation at x6500..6900.
    block("A07_SpineFloor",(6700,0,-15),(1400,460,30),mat_floor)
    block("A07_SpineRoof",(6700,0,343),(1400,460,26),mat_dark)
    wall_x("A07_SpineSouth",6000,7400,-230)
    wall_x("A07_SpineNorthWest",6000,6500,230)
    wall_x("A07_SpineNorthEast",6900,7400,230)

    # Control room adjoining the north side of the central access corridor.
    # A short northern vestibule connects to the room's southern door.
    # North connecting vestibule: X-side walls, with OPEN north/south endpoints.
    block("A07_ControlAccessFloor",(6700,390,-15),(400,320,30),mat_floor)
    block("A07_ControlAccessRoof",(6700,390,363),(400,320,26),mat_dark)
    wall_y("A07_ControlAccessWest",6500,230,550)
    wall_y("A07_ControlAccessEast",6900,230,550)
    room("A07_Observation",6250,7400,550,1850,350,
         south=(6700,360),east=(1150,690))
    # Observation window frame, glass has collision and remains walk-through proof.
    block("A07_ObservationWindowSill",(7400,1150,105),(45,690,210),mat_steel)
    block("A07_ObservationWindowHeader",(7400,1150,325),(45,690,50),mat_steel)
    block("A07_ObservationWindowGlass",(7400,1150,235),(18,655,165),mat_glass)

    # Huge A-07 chamber; west wall has an aligned corridor doorway at y=0
    # plus a control-room viewing window at y=805..1495.
    block("A07_ChamberFloor",(9300,0,-20),(3800,3800,40),mat_floor)
    block("A07_ChamberRoof",(9300,0,915),(3800,3800,30),mat_dark)
    wall_x("A07_ChamberSouth",7400,11200,-1900,900)
    wall_x("A07_ChamberNorth",7400,11200,1900,900)
    # Wall between central door and observation window, including above window.
    for label,ya,yb in [
        ("A07_ChamberWestBelowDoor",-1900,-190),
        ("A07_ChamberWestMid",190,805),
        ("A07_ChamberWestAboveWindow",1495,1900)]:
        wall_y(label,7400,ya,yb,900)
    block("A07_ChamberWestDoorHeader",(7400,0,585),(35,380,630),mat_conc)
    # window upper wall, leaving a 165cm glazed opening z 150..315
    block("A07_ChamberWestWindowUpper",(7400,1150,610),
          (35,690,580),mat_conc)
    # Right wall with maintenance doorway y=-900, standard 2.7m opening.
    west_east_wall("A07_ChamberEast",11200,-1900,1900,(-900,350),900)

    # Maintenance room, then the narrow service tunnel to a second extraction.
    room("A07_Maintenance",11200,13200,-1600,-250,440,
         west=(-900,350),east=(-900,350))
    room("A07_UtilityTunnel",13200,15300,-1350,-450,340,
         west=(-900,350),east=(-900,350))

    # No artwork generated: Unreal blockout structural pieces and visual trim.
    for i,x in enumerate(range(4950,6000,300)):
        block("A07_AirlockFloorStripe_%d" % i,(x,-380,2),(16,90,4),mat_accent)
    for i,x in enumerate(range(7700,11100,550)):
        block("A07_CeilingGirder_%d" % i,(x,0,855),(28,3600,65),mat_steel)
    for side in (-1,1):
        y=side*1760
        block("A07_IndustrialPipe_%s" % side,(9300,y,795),
              (3400,26,26),mat_steel)
    for x in (7800,8400,10200,10800):
        for y in (-1600,1600):
            block("A07_HazardTrim_%d_%d"%(x,y),(x,y,3),
                  (180,16,5),mat_accent)

    # User-supplied imported Meshy art: scale uniformly by verified bounds.
    meshy("A07_MeshyMountingPlatform",
          MODULES+"/MountingPlatform/SM_A07_MountingPlatform",
          (9300,0,0),(730,730,130))
    meshy("A07_MeshyContainmentVessel",
          ROOT_MESH+"/ContainmentVessel/SM_A07_ContainmentVessel",
          (9300,0,130),(610,610,590))
    meshy("A07_MeshyCatwalkNorth",
          MODULES+"/Catwalk/SM_A07_Catwalk",
          (9310,1200,190),(700,260,330))
    meshy("A07_MeshyCatwalkSouth",
          MODULES+"/Catwalk/SM_A07_Catwalk",
          (9310,-1200,190),(700,260,330),180)
    meshy("A07_MeshyDamagedWall",
          MODULES+"/DamagedWall/SM_A07_DamagedWall",
          (10600,-1790,0),(440,110,520))
    meshy("A07_MeshyContainmentEntrance",
          MODULES+"/ContainmentEntrance/SM_A07_ContainmentEntrance",
          (7380,0,0),(180,370,315),90)
    meshy("A07_MeshyDeconChamber",
          ROOT_MESH+"/DeconChamber/SM_A07_DeconChamber",
          (5450,370,0),(540,320,280),90)
    meshy("A07_MeshyControlConsole",
          ROOT_MESH+"/ControlConsole/SM_A07_ControlConsole",
          (6840,1320,0),(380,200,230))
    # Simple invisible central obstruction. Never collide with complex Meshy polygons.
    collision=block("A07_CentralSimpleCollision",(9300,0,300),
                    (530,530,600),None)
    if collision:
        collision.static_mesh_component.set_visibility(False)

    # Lights: powered white lights initially OFF, always-on red emergency lights.
    for label,xyz in [
        ("A07_AirlockPowerLamp",(5350,0,300)),
        ("A07_ObservationPowerLamp",(6700,1130,305)),
        ("A07_ChamberPowerLamp1",(8300,0,760)),
        ("A07_ChamberPowerLamp2",(10300,0,760)),
        ("A07_MaintenancePowerLamp",(12200,-950,380)),
    ]:
        ceiling_light(label,xyz,1400,(.75,.87,1.0),790,True)
    for label,xyz in [
        ("A07_RedAirlock",(4740,0,295)),
        ("A07_RedObservation",(7240,1300,295)),
        ("A07_RedChamberWest",(7520,0,630)),
        ("A07_RedChamberEast",(11050,-930,630)),
        ("A07_RedMaintenance",(12700,-900,355)),
        ("A07_RedTunnelExit",(15100,-900,285)),
    ]:
        ceiling_light(label,xyz,510,(1.0,.12,.08),490,False)

    # Interactables connect to the existing unmodified C++ G-key mission.
    # The C++ classes must be compiled and visible in this project first.
    interaction("A07_MaintenanceAccessCard","AccessCard",(530,-700,115))
    interaction("A07_BackupPower","RestorePower",(3250,-980,125))
    interaction("A07_SecurityDoor","RestrictedDoor",(4610,0,140),320)
    interaction("A07_Decontamination","Decontamination",(5520,-365,115))
    interaction("A07_ObservationConsole","ObservationConsole",(6820,1150,130))
    interaction("A07_ContainmentConsole","ContainmentConsole",(10550,0,125))
    interaction("A07_EmergencyExtraction","Extraction",(15000,-900,135))
    # Static restricted barrier may be hidden by FacilityMissionDirector.
    security = block("A07_RestrictedVisualDoor",(4600,0,130),
                     (38,340,260),mat_steel)
    if security:
        with_tag(security,"AH_RestrictedVisualDoor")

    director_cls = unreal.load_class(None, "/Script/MyProject5.FacilityMissionDirector")
    if director_cls:
        director = spawn(director_cls,"A07_MissionDirector",(6600,0,150))
        if director:
            setprop(director,"spawn_gaunt_after_containment",True)  # Tools/setup_a07_playable_ue58.py also sets the release point
    else:
        warn("FacilityMissionDirector unavailable. Build/reopen project for G-key mission.")

    nav_cls=getattr(unreal,"NavMeshBoundsVolume",None)
    if nav_cls:
        nav = spawn(nav_cls,"A07_ExtendedNavBounds",(9500,0,425))
        if nav:
            nav.set_actor_scale3d(unreal.Vector(61,27,5.5))

    log("Expanded structure finished in current DUPLICATE only.")
    log("Inspect player clearance, door opening and imported mesh orientations.")
    log("NO AUTOSAVE. Use Ctrl+S only after reviewing the new level.")
    log("Important: elevated Meshy catwalks are VISUAL-ONLY, not walkable.")
    log("If you want walkable platforms, add simple collision and supported steps/ramps.")
    log("Power lights initially OFF; test gameplay after C++ classes compile.")
    log("The original research facility maps and TestArena are not altered.")

world = editor.get_editor_world() if editor else None
EXISTING = {}
if world and world.get_name() == TARGET_WORLD:
    EXISTING = {a.get_actor_label(): a
                for a in actors.get_all_level_actors() if a}
main()
