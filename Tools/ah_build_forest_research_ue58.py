"""
Anomaly Hunter: Forest Research Facility, UE 5.8 Editor-only map builder.

Create a small, primarily indoor forest research outpost with one exterior entry path.
SAFE BY DESIGN:
  * does not modify /Game/Maps/TestArena or any other existing map
  * refuses to run if /Game/Maps/AH_ForestResearch_Small already exists
  * uses separate /Game/AH_ForestResearch materials and assets
  * leaves current GameMode, enemies, controls and default maps untouched
  * decor is static, collision handled by UE basic shape meshes

Tools > Execute Python Script... > Tools/ah_build_forest_research_ue58.py
Requires Python Script Plugin and Editor Scripting Utilities.
"""
import math
import random
import unreal

SEED = 4109
RNG = random.Random(SEED)
MAP = "/Game/Maps/AH_ForestResearch_Small"
ROOT = "/Game/AH_ForestResearch"
MATERIAL_FOLDER = ROOT + "/Materials"
TAG = "AHForestResearch"
EDITOR = unreal.EditorAssetLibrary
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MAT_EDIT = unreal.MaterialEditingLibrary
ACTOR_EDITOR = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
CUBE = None
CYL = None
SPHERE = None
MATS = {}

def say(message):
    unreal.log("[AH Forest Research] " + message)

def setprop(obj, key, value):
    try:
        obj.set_editor_property(key, value)
        return True
    except Exception as error:
        unreal.log_warning("[AH Forest Research] %s: %s" % (key, error))
        return False

def color(name, rgb, roughness=0.85, metallic=0.0):
    path = MATERIAL_FOLDER + "/" + name
    if EDITOR.does_asset_exist(path):
        return EDITOR.load_asset(path)
    material = ASSET_TOOLS.create_asset(
        name, MATERIAL_FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    if not material:
        return None
    rgbnode = MAT_EDIT.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -270, -110)
    rgbnode.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    MAT_EDIT.connect_material_property(rgbnode, "", unreal.MaterialProperty.MP_BASE_COLOR)
    r = MAT_EDIT.create_material_expression(material, unreal.MaterialExpressionConstant, -250, 70)
    r.set_editor_property("r", float(roughness))
    MAT_EDIT.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    if metallic:
        m = MAT_EDIT.create_material_expression(material, unreal.MaterialExpressionConstant, -250, 230)
        m.set_editor_property("r", float(metallic))
        MAT_EDIT.connect_material_property(m, "", unreal.MaterialProperty.MP_METALLIC)
    MAT_EDIT.recompile_material(material)
    EDITOR.save_loaded_asset(material)
    return material

def spawn(kind, at, rot=(0, 0, 0), name=None):
    # Rotator positional order differs across UE script examples; use named arguments.
    rotation = unreal.Rotator(pitch=rot[0], yaw=rot[1], roll=rot[2])
    actor = ACTOR_EDITOR.spawn_actor_from_class(kind, unreal.Vector(*at), rotation)
    if actor:
        if name:
            actor.set_actor_label(name)
        setprop(actor, "tags", [unreal.Name(TAG)])
    return actor

def block(label, at, size, mat, shape="cube"):
    actor = spawn(unreal.StaticMeshActor, at, name=label)
    if not actor:
        return
    component = actor.static_mesh_component
    mesh = {"cube": CUBE, "cylinder": CYL, "sphere": SPHERE}.get(shape, CUBE)
    component.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(
        size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    if mat and mat in MATS and MATS[mat]:
        component.set_material(0, MATS[mat])
    return actor

def wall_x(label, x0, x1, y, mat="wall", z=210, h=420, t=24):
    if x1 <= x0: return
    return block(label, ((x0+x1)/2, y, z), (x1-x0,t,h), mat)

def wall_y(label, x, y0, y1, mat="wall", z=210, h=420, t=24):
    if y1 <= y0: return
    return block(label, (x,(y0+y1)/2,z), (t,y1-y0,h), mat)

def portal_x(label, x0, x1, y, mat="wall", h=420):
    # Door gap centered along a horizontal wall, from floor to 295 cm.
    gap0 = (x0+x1)/2-115
    gap1 = (x0+x1)/2+115
    wall_x(label+"_L",x0,gap0,y,mat,h=h)
    wall_x(label+"_R",gap1,x1,y,mat,h=h)
    block(label+"_Header",((gap0+gap1)/2,y,357),(230,30,126),"trim")
    block(label+"_DoorFrameL",(gap0,y,142),(17,38,284),"trim")
    block(label+"_DoorFrameR",(gap1,y,142),(17,38,284),"trim")
    block(label+"_Threshold",((gap0+gap1)/2,y,4),(230,48,8),"floor")
    return (gap0+gap1)/2

def portal_y(label, x, y0, y1, mat="wall", h=420):
    gap0 = (y0+y1)/2-115
    gap1 = (y0+y1)/2+115
    wall_y(label+"_L",x,y0,gap0,mat,h=h)
    wall_y(label+"_R",x,gap1,y1,mat,h=h)
    block(label+"_Header",(x,(gap0+gap1)/2,357),(30,230,126),"trim")
    for y in (gap0,gap1):
        block(label+"_Frame",(x,y,142),(38,17,284),"trim")

def desk(label,x,y,turn=False):
    if turn:
        block(label+"_Surface",(x,y,80),(90,220,10),"table")
        for dy in (-85,85):
            block(label+"_Stand",(x,y+dy,40),(75,12,80),"metal")
    else:
        block(label+"_Surface",(x,y,80),(220,90,10),"table")
        for dx in (-85,85):
            block(label+"_Stand",(x+dx,y,40),(12,75,80),"metal")

def terminal(label,x,y):
    block(label+"_ScreenCase",(x,y,124),(58,10,55),"metal")
    block(label+"_Display",(x,y-6,124),(48,3,44),"screen")
    block(label+"_Keyboard",(x,y-35,88),(72,23,4),"trim")
    block(label+"_Indicator",(x+19,y-7,147),(5,4,5),"indicator")

def crate(label,x,y,z=37):
    block(label+"_Case",(x,y,z),(72,72,74),"wood")
    block(label+"_Band1",(x,y,z+26),(74,74,8),"trim")
    block(label+"_Band2",(x,y,z-26),(74,74,8),"trim")

def cylinder(label,x,y,z,radius,height,material):
    return block(label,(x,y,z),(radius*2,radius*2,height),material,"cylinder")

def light(label,xyz,rgb=(1.0,.78,.55),intensity=350,reach=740):
    a=spawn(unreal.PointLight,xyz,name=label)
    if a:
        c=a.get_component_by_class(unreal.PointLightComponent)
        setprop(c,"intensity_units",unreal.LightUnits.CANDELAS)
        c.set_intensity(float(intensity))
        c.set_light_color(unreal.LinearColor(*rgb,1.0))
        c.set_attenuation_radius(float(reach))
        setprop(c,"cast_shadows",True)
    return a

def ceiling_light(label,x,y,tone="white"):
    block(label+"_Fixture",(x,y,396),(150,48,12),"metal")
    block(label+"_Panel",(x,y,389),(132,35,3),"ceilinglamp")
    if tone=="warning":
        light(label+"_Light",(x,y,367),(1,.2,.07),220,560)
    elif tone=="cyan":
        light(label+"_Light",(x,y,367),(.50,.8,1),220,620)
    else:
        light(label+"_Light",(x,y,367),(0.85,.93,1),250,650)

def strip(label,x,y,length,mat="warning"):
    block(label,(x,y,7),(length,7,2),mat)

def shelves(label,x,y):
    for z in (35,115,195):
        block(label+"_Shelf_%d"%z,(x,y,z),(180,50,8),"metal")
    for dx in (-82,82):
        block(label+"_Post_%d"%dx,(x+dx,y,108),(8,52,215),"trim")

def glass_case(label,x,y):
    block(label+"_Cabinet",(x,y,118),(140,80,235),"metal")
    block(label+"_Window",(x,y-42,124),(126,4,172),"glass")
    block(label+"_Containment",(x,y-45,140),(23,9,60),"indicator")

def forest_tree(i,x,y,height=420):
    # Cylinder trunk + three low-poly circular crowns. Leaves don't overlap walkway.
    trunkheight=height*0.60
    cylinder("Tree_%02d_Trunk"%i,x,y,trunkheight/2,19,trunkheight,"bark")
    for layer in range(3):
        dz=height*.18*layer
        broad=210-layer*45
        block("Tree_%02d_Crown_%d"%(i,layer),
              (x,y,trunkheight+dz),(broad,broad,195-layer*24),
              "pine","sphere")
    cylinder("Tree_%02d_Base"%i,x,y,14,46,28,"soil")

def ground_decor():
    block("Exterior_ForestGround",(-2700,0,-35),(4300,3200,70),"ground")
    block("GravelWalkway",(-2990,0,1),(2700,280,7),"path")
    # Fenced outdoor approach, with gate gap nearest entrance.
    for y in (-530,530):
        block("SiteFenceRail",(-2750,y,96),(2800,9,11),"metal")
        for x in range(-4050,-1250,230):
            block("SiteFencePost",(x,y,110),(12,12,220),"metal")
    for i in range(30):
        x=RNG.uniform(-4650,-1600)
        y=RNG.choice((-1,1))*RNG.uniform(770,1520)
        forest_tree(i,round(x),round(y),RNG.randint(320,590))
    for i in range(45):
        x=RNG.uniform(-4650,-1660)
        y=RNG.choice((-1,1))*RNG.uniform(360,1470)
        cylinder("GroundFern_%02d"%i,round(x),round(y),13,18,26,"pine")
    for i in range(25):
        x=RNG.uniform(-4200,-1630)
        y=RNG.choice((-1,1))*RNG.uniform(230,1100)
        block("Rock_%02d"%i,(x,y,12),(RNG.randint(24,78),RNG.randint(24,90),24),"rock","sphere")
    block("EntryCanopy",(-1540,0,400),(330,630,32),"roof")
    for y in (-270,270):
        block("CanopyColumn",(-1680,y,200),(20,20,400),"trim")
    for x in (-3600,-2600,-1725):
        cylinder("ExteriorBollard_%d"%x,x,220,60,17,120,"trim")
        light("ApproachLamp_%d"%x,(x,220,170),(0.65,.82,1),200,540)
    block("GateSign_Back",(-1830,0,210),(30,330,115),"metal")
    block("GateSign_Face",(-1850,0,210),(7,300,94),"warning")
    block("DrainGrate",(-1880,0,4),(24,300,5),"metal")
    for y in (-260,-180,-100,0,100,180,260):
        block("DrainSlot",( -1880,y,7),(22,9,3),"trim")

def facility_shell():
    block("ResearchFloor",(0,0,-18),(3200,2400,36),"floor")
    # roof is split: interior still visible if users hide ceiling group later.
    block("FacilityRoof",(0,0,443),(3280,2460,34),"roof")
    wall_x("NorthExterior",-1600,1600,1200)
    wall_x("SouthExterior",-1600,1600,-1200)
    wall_y("EastExterior",1600,-1200,1200)
    portal_y("WestExteriorEntrance",-1600,-1200,1200)
    # Main east-west hall is 420cm wide. Doors to each of the six rooms.
    for side in (-1,1):
        y=side*210
        portal_x(("North" if side==1 else "South")+"_SecurityDoor",-1570,-530,y)
        portal_x(("North" if side==1 else "South")+"_LaboratoryDoor",-515,420,y)
        portal_x(("North" if side==1 else "South")+"_UtilitiesDoor",435,1570,y)
    # Room separations from corridor wall to facility exterior wall.
    for x in (-520,425):
        wall_y("NorthPartition_%d"%x,x,225,1180)
        wall_y("SouthPartition_%d"%x,x,-1180,-225)
    # Mark directional wayfinding bands, no floating or unreadable lettering.
    for x in range(-1250,1451,400):
        strip("WalkwayStripe_%d"%x,x,-163,110,"warning")
    for x in (-1150,-200,930):
        ceiling_light("MainHallLuminaire_%d"%x,x,0)
    # Hall utility piping and wall conduits.
    for y in (-174,174):
        block("CeilingConduit_%.0f"%y,(0,y,371),(3020,11,12),"metal")
        for x in range(-1350,1450,350):
            block("ConduitClamp_%d_%d"%(x,y),(x,y,372),(9,22,22),"trim")
    for x in (-1330,-450,410,1370):
        block("HallFireSafetyBox_%d"%x,(x,185,132),(54,18,72),"warning")
    # Raised floor service / drainage details.
    for x in (-1240,-600,140,780,1380):
        block("HallFloorGrate_%d"%x,(x,0,3),(85,190,5),"trim")
        for dx in (-22,0,22):
            block("GrateSlot_%d_%d"%(x,dx),(x+dx,0,6),(7,170,2),"metal")

def furnish_north():
    # Northwest: reception and security. West corridor doorway is at -1050.
    desk("ReceptionDesk",-1050,600)
    terminal("ReceptionMonitor",-1050,580)
    desk("SecurityConsole",-680,870,turn=True)
    terminal("SecurityDisplay",-680,860)
    for x in (-1390,-1170):
        block("SecurityLocker_%d"%x,(x,1050,113),(110,88,225),"metal")
        block("LockerLabel_%d"%x,(x,1003,180),(88,4,20),"warning")
    block("SecurityNoticeboard",(-800,1177,200),(250,7,170),"wallpanel")
    ceiling_light("SecurityLighting",-1080,680,"warning")
    # North central: sample-analysis lab
    desk("SampleWorktable",0,610)
    glass_case("SampleIsolation",-270,955)
    glass_case("FieldSpecimens",270,955)
    terminal("SampleLabTerminal",10,590)
    for x in (-375,365):
        cylinder("DecontaminationCanister_%d"%x,x,530,60,35,120,"ceramic")
        cylinder("CanisterLid_%d"%x,x,530,121,37,13,"trim")
    ceiling_light("LabLighting",-50,785,"cyan")
    # NE: servers, electrical, and equipment with details.
    for x in (640,920,1200):
        for y in (710,1000):
            block("ServerRack_%d_%d"%(x,y),(x,y,142),(135,72,275),"metal")
            block("ServerPanel_%d_%d"%(x,y),(x,y-38,145),(110,5,232),"trim")
            for z in (60,112,164,214):
                block("ServerIndicator_%d_%d_%d"%(x,y,z),(x-30,y-42,z),(8,4,8),"indicator")
                block("ServerVent_%d_%d_%d"%(x,y,z),(x+22,y-43,z),(35,3,9),"metal")
    ceiling_light("ServerLight",950,590,"warning")

def furnish_south():
    # SW: archive and stores
    for x in (-1370,-1040,-690):
        shelves("SupplyRacking_%d"%x,x,-900)
        crate("SupplyCrate_%d"%x,x,-650)
        crate("SpareCrate_%d"%x,x,-520)
    ceiling_light("StoreLight",-1010,-735)
    # South lab: equipment and isolated chamber
    desk("DiagnosticsBench",-200,-695)
    terminal("DiagnosticsTerminal",-190,-715)
    glass_case("ContainmentCell",175,-910)
    for x in (-390,340):
        cylinder("TestColumn_%d"%x,x,-920,110,32,220,"ceramic")
        block("TestColumnAccent_%d"%x,(x,-920,222),(75,75,9),"warning")
    ceiling_light("DiagnosticsLight",25,-690,"cyan")
    # SE: generator/maintenance room
    for x in (650,1050,1350):
        block("GeneratorBase_%d"%x,(x,-810,69),(235,180,138),"metal")
        block("GeneratorTop_%d"%x,(x,-810,159),(215,165,46),"trim")
        for yy in (-890,-810,-730):
            block("GeneratorRib_%d_%d"%(x,yy),(x,yy,155),(13,20,75),"warning")
        cylinder("GeneratorGauge_%d"%x,x,-700,194,32,12,"ceramic")
    block("ServiceDuct",(970,-1100,342),(1050,105,75),"metal")
    for x in range(540,1450,140):
        block("DuctStripe_%d"%x,(x,-1100,304),(20,108,5),"trim")
    ceiling_light("MaintenanceLight",950,-600,"warning")

def scene_look():
    sky=spawn(unreal.SkyLight,(-2700,0,680),name="ExteriorAmbient")
    if sky:
        comp=sky.get_component_by_class(unreal.SkyLightComponent)
        setprop(comp,"intensity",0.38)
    directional=spawn(unreal.DirectionalLight,(-2500,-1250,1000),
                      rot=(-24,32,0),name="OvercastMoon")
    if directional:
        comp=directional.get_component_by_class(unreal.DirectionalLightComponent)
        setprop(comp,"intensity",0.75)
    fog=spawn(unreal.ExponentialHeightFog,(-2400,0,-65),name="ForestMist")
    if fog:
        comp=fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
        setprop(comp,"fog_density",0.018)
        setprop(comp,"fog_height_falloff",0.19)
    pp=spawn(unreal.PostProcessVolume,(-1000,0,220),name="ResearchOutpostPostProcess")
    if pp:
        setprop(pp,"unbound",True)
        settings=pp.get_editor_property("settings")
        for key,val in (
          ("override_vignette_intensity",True),("vignette_intensity",0.52),
          ("override_film_grain_intensity",True),("film_grain_intensity",0.18),
          ("override_bloom_intensity",True),("bloom_intensity",0.42),
          ("override_scene_fringe_intensity",True),("scene_fringe_intensity",0.35),
          ("override_motion_blur_amount",True),("motion_blur_amount",0.18)
        ):
            setprop(settings,key,val)
        setprop(pp,"settings",settings)

def spawn_visual_enemy_if_available():
    # Visual-only display. No enemy behaviour changes or AI actors spawned here.
    # If a rigged skeleton is imported later, add it in UE manually to the test bay.
    say("No AI enemies spawned. Enemy appearance/animation is imported separately.")

def make_map():
    global CUBE, CYL, SPHERE
    if EDITOR.does_asset_exist(MAP):
        say("Map already exists. Refusing to rebuild or overwrite: " + MAP)
        return
    CUBE = EDITOR.load_asset("/Engine/BasicShapes/Cube")
    CYL = EDITOR.load_asset("/Engine/BasicShapes/Cylinder")
    SPHERE = EDITOR.load_asset("/Engine/BasicShapes/Sphere")
    if not CUBE or not CYL or not SPHERE:
        raise RuntimeError("Missing standard Unreal Engine basic shape meshes")
    if not EDITOR.does_directory_exist(MATERIAL_FOLDER):
        EDITOR.make_directory(MATERIAL_FOLDER)

    palette={
       "wall":((.21,.24,.26),.90,.04),
       "floor":((.30,.36,.37),.86,.02),
       "trim":((.065,.077,.085),.74,.22),
       "metal":((.11,.14,.15),.58,.45),
       "table":((.16,.19,.19),.82,.10),
       "roof":((.07,.09,.10),.85,.03),
       "ceramic":((.58,.62,.58),.68,.0),
       "glass":((.09,.35,.40),.15,.05),
       "screen":((.02,.32,.38),.22,.01),
       "warning":((.56,.17,.05),.65,.05),
       "indicator":((.02,.82,.49),.32,.02),
       "ceilinglamp":((.50,.73,.80),.20,.0),
       "wood":((.23,.16,.11),.93,.0),
       "wallpanel":((.35,.39,.35),.94,.0),
       "bark":((.13,.10,.075),.95,.0),
       "pine":((.025,.080,.055),.94,.0),
       "ground":((.12,.15,.085),.97,.0),
       "soil":((.12,.095,.075),.95,.0),
       "path":((.24,.26,.23),.96,.0),
       "rock":((.23,.25,.24),.95,.0)
    }
    for name,(rgb,rough,metal) in palette.items():
        MATS[name]=color("M_AH_"+name,rgb,rough,metal)
    les=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not les.new_level(MAP):
        raise RuntimeError("UE failed to create " + MAP)
    scene_look()
    ground_decor()
    facility_shell()
    furnish_north()
    furnish_south()
    spawn_visual_enemy_if_available()
    spawn(unreal.PlayerStart,(-3700,0,100),rot=(0,0,0),name="ResearchApproachPlayerStart")
    if not les.save_current_level():
        unreal.log_warning("[AH Forest Research] Could not confirm level save. Save the level manually.")
    say("Level created: " + MAP)
    say("Saved existing TestArena and GameMode unchanged.")
    say("Open /Game/Maps/AH_ForestResearch_Small and inspect lighting, collision and performance.")

make_map()
