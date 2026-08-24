import traceback
from pathlib import Path
import unreal

LOG = Path(r"C:\Users\Thedo\Documents\MobaRepo\MobaProject\Saved\MakeProjectileVariants.log")
ART = "/Game/Moba/Art"
ABIL = "/Game/Moba/Abilities"
SRC_BP = ABIL + "/BP_Moba_Projectile"
MAT = ART + "/M_MobaBolt"
lines = []

VARIANTS = (
    {
        "name": "BP_Proj_Skillshot",
        "color": (0.15, 0.78, 1.0, 1.0),
        "scale": 0.40,
        "radius": 16.0,
        "assign": ((ABIL + "/BP_GA_Skillshot", "projectile_class"),),
    },
    {
        "name": "BP_Proj_SlowShot",
        "color": (0.28, 0.82, 0.62, 1.0),
        "scale": 0.85,
        "radius": 28.0,
        "assign": ((ABIL + "/BP_MageAttack", "projectile_class"),),
    },
    {
        "name": "BP_Proj_BigShot",
        "color": (1.0, 0.42, 0.06, 1.0),
        "scale": 1.70,
        "radius": 48.0,
        "assign": ((ABIL + "/BP_GA_BigShot", "projectile_class"),),
    },
    {
        "name": "BP_Proj_GroundStrike",
        "color": (1.0, 0.82, 0.18, 1.0),
        "scale": 1.10,
        "radius": 32.0,
        "assign": ((ABIL + "/BP_GA_GroundStrike", "projectile_class"),),
    },
    {
        "name": "BP_Proj_Tower",
        "color": (1.0, 0.12, 0.05, 1.0),
        "scale": 0.55,
        "radius": 18.0,
        "assign": (("/Game/Moba/Actors/BP_Tower", "projectile_class"),),
    },
)


def log(msg):
    lines.append(str(msg))
    unreal.log(str(msg))


def ensure_bolt_material():
    if unreal.EditorAssetLibrary.does_asset_exist(MAT):
        mat = unreal.EditorAssetLibrary.load_asset(MAT)
        log("existing " + MAT)
        return mat

    factory = unreal.MaterialFactoryNew()
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_MobaBolt", ART, unreal.Material, factory
    )
    if not mat:
        log("failed to create M_MobaBolt")
        return None

    try:
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    except Exception as exc:
        log("shading_model skipped: {}".format(exc))

    color = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionVectorParameter, -480, -40
    )
    color.set_editor_property("parameter_name", "BoltColor")
    color.set_editor_property("default_value", unreal.LinearColor(0.15, 0.78, 1.0, 1.0))

    strength = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionConstant, -480, 80
    )
    strength.set_editor_property("r", 12.0)

    mul = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionMultiply, -220, 0
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(color, "", mul, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(strength, "", mul, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )
    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    log("created " + mat.get_path_name())
    return mat


def duplicate_variant(spec):
    dest = ABIL + "/" + spec["name"]
    if not unreal.EditorAssetLibrary.does_asset_exist(dest):
        if not unreal.EditorAssetLibrary.duplicate_asset(SRC_BP, dest):
            log("duplicate failed " + spec["name"])
            return None
        log("duplicated " + spec["name"])
    else:
        log("existing " + dest)

    bp = unreal.EditorAssetLibrary.load_asset(dest)
    cls = unreal.EditorAssetLibrary.load_blueprint_class(dest)
    if not cls:
        log("no class " + dest)
        return None
    cdo = unreal.get_default_object(cls)
    mat = unreal.EditorAssetLibrary.load_asset(MAT)
    if mat:
        cdo.set_editor_property("bolt_material", mat)
    sphere = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Sphere")
    if sphere:
        cdo.set_editor_property("bolt_mesh", sphere)
    cdo.set_editor_property("bolt_color", unreal.LinearColor(*spec["color"]))
    cdo.set_editor_property("visual_scale", spec["scale"])
    cdo.set_editor_property("collision_radius", spec["radius"])
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    except Exception as exc:
        log("compile skipped {}: {}".format(dest, exc))
    unreal.EditorAssetLibrary.save_asset(dest)
    log("look {} scale={} radius={}".format(spec["name"], spec["scale"], spec["radius"]))
    return cls


def assign(bp_path, prop, cls):
    if not unreal.EditorAssetLibrary.does_asset_exist(bp_path):
        log("missing assign target " + bp_path)
        return
    bp = unreal.EditorAssetLibrary.load_asset(bp_path)
    dest_cls = unreal.EditorAssetLibrary.load_blueprint_class(bp_path)
    cdo = unreal.get_default_object(dest_cls)
    cdo.set_editor_property(prop, cls)
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    except Exception as exc:
        log("assign compile skipped {}: {}".format(bp_path, exc))
    unreal.EditorAssetLibrary.save_asset(bp_path)
    log("assigned {} -> {}".format(cls.get_name(), bp_path))


def main():
    if not unreal.EditorAssetLibrary.does_asset_exist(SRC_BP):
        log("missing " + SRC_BP)
        return
    ensure_bolt_material()
    for spec in VARIANTS:
        cls = duplicate_variant(spec)
        if not cls:
            continue
        for bp_path, prop in spec["assign"]:
            assign(bp_path, prop, cls)


if __name__ == "__main__":
    try:
        main()
    except Exception:
        log(traceback.format_exc())
        raise
    finally:
        LOG.write_text("\n".join(lines) + "\n", encoding="utf-8")
