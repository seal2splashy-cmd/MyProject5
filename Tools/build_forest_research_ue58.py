"""Anomaly Hunter: compact forest research facility, UE 5.8 editor builder.

Runs once in the Editor: Tools > Execute Python Script...
Creates ONLY /Game/Maps/AH_ForestResearch_Small and its named materials.
Does not modify TestArena, existing enemy classes, or game defaults.
Safe to re-run: if the map already exists, does nothing.
"""
import math
import random
import unreal

rng = random.Random(1802)
MAP = "/Game/Maps/AH_ForestResearch_Small"
MAT_PATH = "/Game/ForestResearch/Materials"
TAG = "AH_ForestResearch_v1"

assets = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
cube = lib.load_asset("/Engine/BasicShapes/Cube")
cylinder = lib.load_asset("/Engine/BasicShapes/Cylinder")
sphere = lib.load_asset("/Engine/BasicShapes/Sphere")

def log(message):
    unreal.log("[ForestResearch] " + str(message))

def prop(obj, name, value):
    try:
        obj.set_editor_property(name, value)
    except Exception as exc:
        unreal.log_warning("[ForestResearch] couldn't set %s: %s" % (name, exc))

def material(name, rgb, roughness=0.86, metallic=0.0):
    path = MAT_PATH + "/" + name
    if lib.does_asset_exist(path):
        return lib.load_asset(path)
    mat = assets.create_asset(name, MAT_PATH, unreal.Material,
                              unreal.MaterialFactoryNew())
    mel = unreal.MaterialEditingLibrary
    col = mel.create_material_expression(
        mat, unreal.MaterialExpressionConstant3Vector, -300, -100)
    prop(col, "constant", unreal.LinearColor(*rgb, 1.0))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(
        mat, unreal.MaterialExpressionConstant, -300, 130)
    prop(rough, "r", roughness)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    if metallic:
        metal = mel.create_material_expression(
            mat, unreal.MaterialExpressionConstant, -300, 350)
        prop(metal, "r", metallic)
        mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(mat)
    lib.save_loaded_asset(mat)
    return mat

def actor(cls, name, pos, rot=(0, 0, 0)):
    a = actors.spawn_actor_from_class(
        cls, unreal.Vector(*pos),
        unreal.Rotator(pitch=rot[0], yaw=rot[1], roll=rot[2]))
    if a is not None:
        a.set_actor_label(name)
        prop(a, "tags", list(a.get_editor_property("tags")) + [unreal.Name(TAG)])
    return a

M = {}

def mesh(name, pos, size, mat="concrete", shape="cube", rotation=(0, 0, 0)):
    a = actor(unreal.StaticMeshActor, name, pos, rotation)
    if not a:
        return None
    smc = a.static_mesh_component
    smc.set_static_mesh({"cube":cube, "cylinder":cylinder, "sphere":sphere}[shape])
    smc.set_material(0, M[mat])
    # BasicShapes are 100 cm along all three local dimensions.
    a.set_actor_scale3d(unreal.Vector(*[v / 100.0 for v in size]))
    return a

def wallx(name, xa, xb, y, z=160, height=320, thick=28, mat="concrete"):
    if xb > xa:
        mesh(name, ((xa + xb)/2, y, z), (xb-xa, thick, height), mat)

def wally(name, x, ya, yb, z=160, height=320, thick=28, mat="concrete"):
    if yb > ya:
        mesh(name, (x, (ya + yb)/2, z), (thick, yb-ya, height), mat)

def room_edge(name, y, door_x, left, right):
    # side hallway wall with a 240-cm-wide doorway
    wallx(name+"_A", left, door_x-120, y)
    wallx(name+"_B", door_x+120, right, y)
    mesh(name+"_Lintel", (door_x,y,277), (240,34,86), "metal")

def light(name, pos, strength, rgb, radius):
    a = actor(unreal.PointLight, name, pos)
    if not a: return
    comp = a.get_component_by_class(unreal.PointLightComponent)
    comp.set_intensity(strength)
    comp.set_light_color(unreal.LinearColor(*rgb, 1.0))
    comp.set_attenuation_radius(radius)
    prop(comp, "cast_shadows", True)

def foliage_tree(name, x, y, scale=1.0):
    # Offline-friendly placeholder tree. Swap individual meshes for foliage
    # instances / Megascans later; keep clear of the entrance route.
    mesh(name+"_Trunk", (x,y,160*scale), (36*scale,36*scale,320*scale),
         "wood", "cylinder")
    mesh(name+"_CanopyLow", (x,y,360*scale), (200*scale,190*scale,220*scale),
         "pine", "sphere")
    mesh(name+"_CanopyHigh", (x,y,490*scale), (135*scale,135*scale,170*scale),
         "pine", "sphere")

def build():
    if lib.does_asset_exist(MAP):
        log("Existing map detected. Refusing to overwrite: " + MAP)
        return
    log("Creating new separate map " + MAP)
    levels.new_level(MAP)
    M.update({
        "concrete":material("M_FR_Concrete",(.23,.26,.27)),
        "floor":material("M_FR_Floor",(.14,.18,.19),.73),
        "metal":material("M_FR_Steel",(.31,.36,.38),.56,.56),
        "dark":material("M_FR_Charcoal",(.063,.082,.09),.77),
        "accent":material("M_FR_SafetyYellow",(.68,.42,.085),.67),
        "glass":material("M_FR_FrostedGlass",(.26,.38,.41),.37),
        "lamp":material("M_FR_LightPanel",(.70,.82,.78),.32),
        "screen":material("M_FR_Display",(.02,.25,.30),.34),
        "rust":material("M_FR_Oxide",(.26,.14,.085),.88),
        "wood":material("M_FR_Bark",(.17,.12,.10),.99),
        "pine":material("M_FR_ForestGreen",(.043,.12,.077),.96),
        "soil":material("M_FR_ForestSoil",(.13,.125,.095),.97),
        "moss":material("M_FR_Moss",(.11,.20,.12),.98)
    })

    # BUILDING footprint x=0..4000, y=-1500..1500, playable floor z=0.
    mesh("FR_MainSlab",(2000,0,-16),(4000,3000,32),"floor")
    mesh("FR_Roof",(2000,0,332),(4000,3000,25),"dark")
    # central corridor: +/-200 cm y. Rooms on either side.
    wallx("FR_OuterNorth",0,4000,1500)
    wallx("FR_OuterSouth",0,4000,-1500)
    wally("FR_RearWall",4000,-1500,1500)
    # Front face with open entrance portal, x=0, y +/-150.
    wally("FR_FrontWallNorth",0,150,1500)
    wally("FR_FrontWallSouth",0,-1500,-150)
    mesh("FR_EntranceHeader",(0,0,273),(32,300,94),"metal")
    mesh("FR_EntryCanopy",(-175,0,330),(390,480,25),"metal")
    mesh("FR_WelcomeMat",(130,0,2),(210,280,6),"dark")
    for y in (-205,205):
        mesh("FR_EntranceSafetyFrame",(0,y,155),(36,24,310),"accent")
    # Room divider runs: three rooms north, three rooms south
    for y,sign in [(205,"N"),(-205,"S")]:
        for k,(left,right,door) in enumerate([(0,1300,720),(1300,2700,2000),(2700,4000,3360)]):
            room_edge("FR_HallWall_%s_%d"%(sign,k),y,door,left,right)
            mesh("FR_DoorThreshold_%s_%d"%(sign,k),(door,y,-1),(240,135,5),"metal")
            for yy in (y-12,y+12):
                mesh("FR_DoorTrack_%s_%d_%d"%(sign,k,int(yy)),
                     (door,yy,255),(250,9,15),"metal")
    for x in (1300,2700):
        wally("FR_PartitionNorth_%d"%x,x,205,1500)
        wally("FR_PartitionSouth_%d"%x,x,-1500,-205)
    # Floor/ceiling corridor visual guides
    for x in range(250,3850,400):
        mesh("FR_HallFloorMarker_%d"%x,(x,0,1),(85,11,4),"accent")
        mesh("FR_HallCeilingStrip_%d"%x,(x,0,318),(210,18,10),"lamp")
        light("FR_HallLight_%d"%x,(x,0,291),1050,(.71,.89,1.0),390)

    # Modular room functions
    def table(label,x,y,material_name="metal"):
        mesh(label+"_Top",(x,y,83),(210,95,9),material_name)
        for dx in (-88,88):
            for dy in (-34,34):
                mesh(label+"_Leg_%s_%s"%(dx,dy),(x+dx,y+dy,42),(10,10,80),"dark")
    def cabinet(label,x,y):
        mesh(label+"_Body",(x,y,106),(95,58,212),"dark")
        mesh(label+"_Door",(x-1,y-32,107),(87,6,195),"metal")
        for z in (35,94,153):
            mesh(label+"_Slot_%d"%z,(x,y-38,z),(70,7,4),"accent")
    def terminal(label,x,y):
        mesh(label+"_Monitor",(x,y,132),(75,12,48),"dark")
        mesh(label+"_Display",(x,y-7,133),(66,3,39),"screen")
        mesh(label+"_Stand",(x,y,104),(9,15,23),"metal")
        mesh(label+"_Keys",(x,y-46,92),(70,25,4),"dark")
    def shelf(label,x,y):
        for z in (38,105,173,241):
            mesh(label+"_Layer_%d"%z,(x,y,z),(180,50,8),"metal")
        for xx in (x-83,x+83):
            mesh(label+"_Pillar_%d"%xx,(xx,y,140),(8,50,278),"dark")
    # Room names and prop families
    # north west: reception/security
    table("FR_ReceptionDesk",520,925)
    terminal("FR_ReceptionTerminal",510,935)
    cabinet("FR_SecurityLocker1",155,1240)
    cabinet("FR_SecurityLocker2",270,1240)
    mesh("FR_SecurityWindow",(800,1472,192),(430,32,125),"glass")
    # north mid: laboratory
    for i,x in enumerate((1550,1910,2300)):
        table("FR_LabBench_%d"%i,x,960)
        terminal("FR_LabTerminal_%d"%i,x,960)
        mesh("FR_LabSampleBox_%d"%i,(x+70,1020,102),(36,35,24),"glass")
    for i,x in enumerate((1510,1750,2140,2440)):
        mesh("FR_LabCeilingPanel_%d"%i,(x,760,308),(150,90,10),"lamp")
    light("FR_LabWorkLight",(1960,800,275),2350,(.62,.85,1.0),750)
    # north east: server room
    for i,x in enumerate((2940,3180,3420,3660)):
        cabinet("FR_ServerRack_%d"%i,x,1110)
        mesh("FR_ServerStatus_%d"%i,(x,1075,161),(56,6,10),"screen")
    light("FR_ServerStatusGlow",(3350,1000,240),900,(.12,.55,.67),520)
    # south west: storage
    for i,(x,y) in enumerate(((300,-950),(570,-950),(880,-950),(400,-1290))):
        shelf("FR_StorageShelf_%d"%i,x,y)
    for i,(x,y) in enumerate(((280,-720),(620,-780),(850,-1200))):
        mesh("FR_Crate_%d"%i,(x,y,49),(94,100,98),"wood")
        mesh("FR_CrateBand_%d"%i,(x,y,99),(96,104,8),"metal")
    # south mid: medical / observation
    for i,x in enumerate((1630,2300)):
        mesh("FR_ObservationTable_%d"%i,(x,-1040,81),(260,95,12),"metal")
        mesh("FR_ObservationMattress_%d"%i,(x,-1040,92),(230,77,11),"glass")
        cabinet("FR_MedicalStorage_%d"%i,x,-1400)
    light("FR_ObservationLight",(2000,-1000,265),1300,(.80,.89,1.0),700)
    # south east: generator / maintenance
    for i,x in enumerate((2960,3460)):
        mesh("FR_GeneratorFrame_%d"%i,(x,-1020,102),(310,180,204),"metal")
        mesh("FR_GeneratorCore_%d"%i,(x,-1020,111),(210,148,150),"dark")
        mesh("FR_GeneratorStatus_%d"%i,(x,-1113,171),(83,9,25),"accent")
        for j in range(5):
            mesh("FR_GeneratorVent_%d_%d"%(i,j),
                 (x-95+j*45,-1118,116),(21,8,55),"rust")
    light("FR_MaintenanceLamp",(3350,-900,267),800,(1.0,.53,.30),600)

    # Structural and micro-detail language throughout the building.
    for x in range(200,3900,370):
        for y in (-1430,1430):
            mesh("FR_Brace_%d_%d"%(x,y),(x,y,160),(30,38,320),"metal")
        for y in (-280,280):
            mesh("FR_WireTray_%d_%d"%(x,y),(x,y,285),(260,20,14),"dark")
    for y in (-1425,1425):
        mesh("FR_FullPipe_%d"%y,(2000,y,282),(3850,13,13),"rust")
    for i,x in enumerate((570,1780,3270)):
        for y in (-1365,1365):
            mesh("FR_FireCabinet_%d_%d"%(i,y),(x,y,132),(60,26,90),"accent")
    for x in (1200,2600,3860):
        mesh("FR_HallEmergencyPanel_%d"%x,(x,185,160),(38,8,56),"accent")
    for x in range(120,3940,260):
        mesh("FR_Grate_%d"%x,(x,-158,2),(115,60,5),"dark")
        for k in range(4):
            mesh("FR_GrateSlot_%d_%d"%(x,k),
                 (x-37+k*25,-158,5),(11,56,2),"metal")
    # Exterior: paths + trees + fence and scattered rocks
    mesh("FR_OutdoorGround",(-1250,0,-32),(2600,4300,60),"soil")
    mesh("FR_EntryWalkway",(-950,0,2),(1900,330,14),"concrete")
    for x in range(-1800,-70,250):
        mesh("FR_PathSeam_%d"%x,(x,0,11),(8,330,4),"dark")
        for side in (-1,1):
            yy=side*205
            mesh("FR_PathLightPost_%d_%d"%(x,side),(x,yy,100),(13,13,200),"metal")
            mesh("FR_PathLightHead_%d_%d"%(x,side),(x,yy,210),(55,45,24),"lamp")
    for x in (-1600,-900,-250):
        light("FR_ForestPathLight_%d"%x,(x,0,255),800,(.65,.74,.69),550)
    # Trees clear of the walkable corridor and player start
    for i in range(31):
        x=rng.uniform(-2250,-110)
        y=rng.choice([-1,1])*rng.uniform(550,2000)
        foliage_tree("FR_Tree_%02d"%i,x,y,rng.uniform(.72,1.22))
        if i%3==0:
            mesh("FR_Rock_%02d"%i,
                 (x+100,y+120,12),(95,80,38),"concrete","sphere")
    for y in (-470,470):
        for x in range(-1800,-100,260):
            mesh("FR_FencePost_%d_%d"%(x,y),(x,y,95),(14,14,190),"metal")
        mesh("FR_FenceTop_%d"%y,(-980,y,185),(1780,12,15),"metal")
        mesh("FR_FenceBottom_%d"%y,(-980,y,35),(1780,12,12),"metal")
    # The anomaly: the Gaunt (AGauntAnomaly, live AI) in the server area if the C++ class
    # exists; otherwise the optional static, nonblocking preview. Import the assets first
    # (Tools/import_gaunt_ue58.py) or it shows as a capsule.
    gaunt_cls=unreal.load_class(None,"/Script/MyProject5.GauntAnomaly")
    preview=None if gaunt_cls else lib.load_asset("/Game/Characters/ForestAnomaly/SK_AH_ForestAnomaly_Preview")
    if gaunt_cls:
        if actor(gaunt_cls,"FR_Anomaly_Gaunt",(3500,650,100),(0,180,0)):
            log("Added the Gaunt anomaly (live AI) to the server area.")
    elif preview and preview.get_class().get_name()=="StaticMesh":
        specimen=actor(unreal.StaticMeshActor,"FR_Anomaly_VisualOnly",
                       (3500,650,0),(0,180,0))
        if specimen:
            component=specimen.static_mesh_component
            component.set_static_mesh(preview)
            component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            log("Added visual-only anomaly, no AI, to the server area.")
    else:
        log("No anomaly preview imported yet; forest map will still be complete.")
    # Safe initial spawn. Existing C++ GameMode selects AMyFPSPawn.
    actor(unreal.PlayerStart,"FR_PlayerStart",(-1650,0,102),(0,0,0))
    # Editor convenience: no enemy is spawned until the new model is tested.
    light("FR_EntranceWarmLight",(-80,0,270),1400,(1.0,.78,.46),550)
    sun=actor(unreal.DirectionalLight,"FR_Moonlight",(0,0,1300),(-33,-15,0))
    if sun:
        comp=sun.get_component_by_class(unreal.DirectionalLightComponent)
        if comp: comp.set_intensity(.35)
    # NavMesh over the building and the forest outside, so enemies path round walls and trees.
    # The editor gives a new volume a 200 cm cube brush: scale = half-size / 100.
    nav=actor(unreal.NavMeshBoundsVolume,"FR_NavMeshBounds",(725,0,175))
    if nav:
        nav.set_actor_scale3d(unreal.Vector(33.5,22.5,2.5))   # x -2625..4075, y +/-2250, z -75..425
        unreal.SystemLibrary.execute_console_command(
            unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(),"RebuildNavigation")
    levels.save_current_level()
    log("Saved " + MAP + ". Test Play; original TestArena was not changed.")
    log("Check doorways, collisions, lighting and PIE before making it a startup map.")

build()
