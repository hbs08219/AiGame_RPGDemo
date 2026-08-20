import math
import unreal

MAPS_ROOT = "/Game/Maps"
ASSET_ROOT = "/Game/RpgDemo/Materials"
TOWN_MAP = f"{MAPS_ROOT}/LV_MistportTown_08"
BATTLE_MAP = f"{MAPS_ROOT}/LV_CrystalArena_08"
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
CYLINDER = unreal.load_asset("/Engine/BasicShapes/Cylinder.Cylinder")
SPHERE = unreal.load_asset("/Engine/BasicShapes/Sphere.Sphere")
CONE = unreal.load_asset("/Engine/BasicShapes/Cone.Cone")
BASE_MATERIAL = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")

PALETTE = {
    "grass": (0.16, 0.42, 0.14, 1.0),
    "grass_light": (0.31, 0.62, 0.22, 1.0),
    "cobble": (0.54, 0.42, 0.27, 1.0),
    "stone": (0.46, 0.48, 0.47, 1.0),
    "wall": (0.78, 0.67, 0.48, 1.0),
    "timber": (0.31, 0.14, 0.06, 1.0),
    "roof": (0.05, 0.36, 0.48, 1.0),
    "roof_warm": (0.55, 0.16, 0.07, 1.0),
    "water": (0.07, 0.30, 0.45, 1.0),
    "tree": (0.06, 0.34, 0.10, 1.0),
    "trunk": (0.26, 0.12, 0.04, 1.0),
    "flower": (0.64, 0.16, 0.42, 1.0),
    "crystal": (0.43, 0.06, 0.85, 1.0),
    "banner": (0.12, 0.60, 0.36, 1.0),
}
MATERIALS = {}


def log(text):
    unreal.log("[RpgDemo][StaticScene] " + text)


def material(name, color):
    if name in MATERIALS:
        return MATERIALS[name]
    path = f"{ASSET_ROOT}/MI_{name}"
    asset = unreal.load_asset(path)
    if not asset:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        asset = tools.create_asset(f"MI_{name}", ASSET_ROOT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        asset.set_editor_property("parent", BASE_MATERIAL)
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(asset, "Color", unreal.LinearColor(*color))
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
    MATERIALS[name] = asset
    return asset


def set_label(actor, label):
    actor.set_actor_label(label)
    return actor


def mesh(asset, label, location, scale, palette_name, rotation=(0, 0, 0)):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location), unreal.Rotator(*rotation))
    actor.static_mesh_component.set_static_mesh(asset)
    actor.static_mesh_component.set_material(0, material(palette_name, PALETTE[palette_name]))
    actor.set_actor_scale3d(unreal.Vector(*scale))
    return set_label(actor, label)


def lighting():
    # 日间晴空基调：先建立天空与环境填充，再用暖色低角度太阳塑造体积。
    atmosphere = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
    set_label(atmosphere, "Mistport_Clear_Day_Sky")

    sun = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1800))
    sun.set_actor_rotation(unreal.Rotator(-50, -38, 0), False)
    component = sun.get_component_by_class(unreal.DirectionalLightComponent)
    component.set_editor_property("intensity", 8.5)
    component.set_editor_property("light_color", unreal.Color(255, 228, 182, 255))
    component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    component.set_atmosphere_sun_light(True)
    set_label(sun, "Mistport_Daylight_Sun")

    sky = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 0))
    component = sky.get_component_by_class(unreal.SkyLightComponent)
    component.set_editor_property("intensity", 1.25)
    component.set_editor_property("light_color", unreal.Color(183, 210, 255, 255))
    component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    set_label(sky, "Mistport_Blue_Sky_Fill")

    fog = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
    component = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    component.set_editor_property("fog_density", 0.0018)
    component.set_editor_property("fog_height_falloff", 0.12)
    set_label(fog, "Mistport_Soft_Daylight_Haze")


def tree(label, x, y, size=1.0):
    mesh(CYLINDER, label + "_Trunk", (x, y, 120 * size), (0.32 * size, 0.32 * size, 2.2 * size), "trunk")
    mesh(CONE, label + "_Canopy_A", (x, y, 330 * size), (2.2 * size, 2.2 * size, 3.3 * size), "tree")
    mesh(CONE, label + "_Canopy_B", (x, y, 510 * size), (1.55 * size, 1.55 * size, 2.5 * size), "tree")


def fence(label, x, y, length, along_x=True):
    for index in range(int(length / 150) + 1):
        px = x + index * 150 if along_x else x
        py = y if along_x else y + index * 150
        mesh(CYLINDER, f"{label}_post_{index}", (px, py, 85), (0.13, 0.13, 1.35), "timber")
    if along_x:
        mesh(CUBE, label + "_rail_top", (x + length / 2, y, 150), (length / 200, 0.10, 0.10), "timber")
        mesh(CUBE, label + "_rail_low", (x + length / 2, y, 80), (length / 200, 0.10, 0.10), "timber")
    else:
        mesh(CUBE, label + "_rail_top", (x, y + length / 2, 150), (0.10, length / 200, 0.10), "timber")
        mesh(CUBE, label + "_rail_low", (x, y + length / 2, 80), (0.10, length / 200, 0.10), "timber")


def cottage(label, x, y, roof="roof", width=5.2, depth=4.1, height=2.8):
    mesh(CUBE, label + "_Walls", (x, y, 145), (width, depth, height), "wall")
    mesh(CUBE, label + "_TimberBand", (x, y - depth * 105, 190), (width * 1.05, 0.14, 0.16), "timber")
    mesh(CUBE, label + "_Roof", (x, y, 430), (width * 1.18, depth * 1.22, 0.52), roof, (0, 0, 0))
    mesh(CUBE, label + "_Door", (x - width * 42, y - depth * 102, 105), (0.75, 0.12, 1.4), "timber")
    for offset in (-1, 1):
        mesh(CUBE, f"{label}_Window_{offset}", (x + offset * width * 42, y - depth * 102, 195), (0.56, 0.12, 0.55), "roof")


def town():
    unreal.EditorLevelLibrary.new_level(TOWN_MAP)
    lighting()

    # 前景河岸、低台阶和中景广场：仿 HD-2D 的分层阅读顺序。
    mesh(CUBE, "River_Foreground", (800, -2050, -110), (72, 10, 0.22), "water")
    mesh(CUBE, "Lower_Grass_Terrace", (800, -1300, -65), (66, 8, 0.26), "grass")
    mesh(CUBE, "Town_Main_Terrace", (900, 0, -45), (67, 48, 0.30), "grass_light")
    mesh(CUBE, "Cobble_Main_Road", (940, 0, 0), (32, 3.3, 0.12), "cobble")
    mesh(CUBE, "Cobble_South_Path", (900, -560, 0), (3.0, 9.3, 0.13), "cobble")
    mesh(CUBE, "Cobble_North_Path", (1050, 535, 0), (3.1, 8.3, 0.13), "cobble")
    mesh(CYLINDER, "Mistport_Plaza", (625, 0, 20), (9.5, 9.5, 0.16), "stone")
    mesh(CYLINDER, "Mistport_Well", (625, 0, 65), (1.65, 1.65, 0.62), "stone")
    mesh(CYLINDER, "Mistport_Well_Water", (625, 0, 135), (1.15, 1.15, 0.09), "water")

    cottage("Mayor_Residence", 440, 390, "roof_warm", 5.6, 4.4, 3.0)
    cottage("Apothecary", 1050, -540, "roof", 5.0, 3.8, 2.7)
    cottage("Blacksmith", 1130, 520, "roof_warm", 5.3, 4.0, 2.8)
    cottage("Inn", 210, -390, "roof", 6.4, 4.5, 3.15)
    cottage("Market_House", 1480, 360, "roof", 4.8, 3.8, 2.7)

    # 石墙、花坛和木栅栏增强景深层次。
    mesh(CUBE, "Upper_Terrace_Wall", (880, 1030, 170), (47, 0.65, 2.0), "stone")
    mesh(CUBE, "Upper_Terrace", (880, 1370, 55), (47, 4.2, 0.22), "grass")
    for index, x in enumerate(range(120, 1780, 230)):
        tree(f"NorthPine_{index}", x, 1470, 1.0 + (index % 3) * 0.12)
        tree(f"SouthPine_{index}", x + 70, -1040, 0.82 + (index % 2) * 0.15)
    for index, pos in enumerate([(260, 850), (1220, 840), (1670, -760), (-40, -660)]):
        tree(f"FeatureTree_{index}", pos[0], pos[1], 1.15)

    fence("PlazaFence_A", 250, -270, 500)
    fence("PlazaFence_B", 1110, -230, 460)
    fence("ApothecaryFence", 800, -800, 520)
    fence("BlacksmithFence", 820, 820, 560)
    for index, pos in enumerate([(370, -180), (500, -280), (800, 270), (1180, -310), (1330, 180)]):
        mesh(SPHERE, f"FlowerBed_{index}", (pos[0], pos[1], 65), (1.3, 1.3, 0.45), "flower")

    # 矿洞作为主线终点，紫色晶体和冷光与城镇暖色形成对比。
    mesh(CUBE, "Mine_Gate_Left", (1970, -235, 230), (2.6, 1.3, 4.3), "stone")
    mesh(CUBE, "Mine_Gate_Right", (1970, 235, 230), (2.6, 1.3, 4.3), "stone")
    mesh(CUBE, "Mine_Gate_Top", (1970, 0, 505), (2.7, 4.6, 1.0), "stone")
    mesh(CUBE, "Mine_Interior", (2110, 0, 95), (4.6, 3.7, 1.5), "timber")
    for index, pos in enumerate([(2015, -170, 95), (2080, 115, 115), (1935, 185, 80)]):
        mesh(SPHERE, f"Mine_Crystal_{index}", pos, (1.4, 1.4, 2.65), "crystal")
    # 所有小镇与矿洞点光源已移除；日光、天光和大气环境承担整体照明。

    unreal.EditorLevelLibrary.save_current_level()
    log("Saved static HD-2D inspired Mistport town.")


def arena():
    unreal.EditorLevelLibrary.new_level(BATTLE_MAP)
    lighting()
    mesh(CYLINDER, "Arena_Outer_Ring", (0, 0, -70), (29, 29, 0.35), "stone")
    mesh(CYLINDER, "Arena_Inner_Ring", (0, 0, -25), (20, 20, 0.18), "cobble")
    mesh(CYLINDER, "Arena_Battle_Mark", (0, 0, 20), (11, 11, 0.08), "crystal")
    for index, pos in enumerate([(-1580, -1580, 260), (-1580, 1580, 260), (1580, -1580, 260), (1580, 1580, 260)]):
        mesh(SPHERE, f"Arena_Crystal_{index}", pos, (2.0, 2.0, 4.8), "crystal")
    for index in range(16):
        angle = math.radians(index * 22.5)
        mesh(CUBE, f"Arena_Ruin_{index}", (math.cos(angle) * 2450, math.sin(angle) * 2450, 190), (0.65, 1.25, 3.1), "stone")
    unreal.EditorLevelLibrary.save_current_level()
    log("Saved static Crystal Arena.")


town()
arena()
