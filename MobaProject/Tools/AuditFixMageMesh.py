import traceback
from pathlib import Path

import unreal

LOG_PATH = Path(r"C:\Users\Thedo\Documents\MobaRepo\MobaProject\Saved\AuditFixMageMesh.log")
lines = []

ART = "/Game/Moba/Art"
PARENT_MESH = "/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"
HAT_MESH = "/Game/Moba/Art/WizardHat"
HAT_MAT = "/Game/Moba/Art/M_WizardHat"
MAGE_BP = "/Game/Moba/BP_Mage"
BASE_BP = "/Game/Moba/BP_MobaBaseChar"


def log(msg):
    text = str(msg)
    lines.append(text)
    unreal.log(text)


def write_log():
    try:
        LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
        LOG_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")
    except Exception as exc:
        unreal.log_error("Failed to write audit log: {}".format(exc))


def prop(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def asset_name(obj):
    if not obj:
        return "None"
    try:
        return obj.get_path_name()
    except Exception:
        return str(obj)


def load_class(path):
    try:
        return unreal.EditorAssetLibrary.load_blueprint_class(path)
    except Exception as exc:
        log("load_blueprint_class {} failed: {}".format(path, exc))
        return None


def dump_comp(prefix, comp):
    if not comp:
        log("{} None".format(prefix))
        return
    log("{} class={} name={}".format(prefix, comp.get_class().get_name(), comp.get_name()))
    log("  loc={} rot={} scale={}".format(
        prop(comp, "relative_location"),
        prop(comp, "relative_rotation"),
        prop(comp, "relative_scale3d"),
    ))
    parent = None
    try:
        parent = comp.get_attach_parent()
    except Exception:
        parent = prop(comp, "attach_parent")
    log("  attach parent={} socket={}".format(
        parent.get_name() if parent else "None",
        prop(comp, "attach_socket_name", ""),
    ))
    if isinstance(comp, unreal.SkeletalMeshComponent):
        log("  skeletal_mesh={}".format(asset_name(prop(comp, "skeletal_mesh_asset") or prop(comp, "skeletal_mesh"))))
        log("  anim_class={}".format(asset_name(prop(comp, "anim_class"))))
        log("  override_materials={}".format(prop(comp, "override_materials")))
    if isinstance(comp, unreal.StaticMeshComponent):
        log("  static_mesh={}".format(asset_name(prop(comp, "static_mesh"))))
        log("  override_materials={}".format(prop(comp, "override_materials")))


def dump_subobjects(bp, label):
    log("--- subobjects {} ---".format(label))
    try:
        sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        handles = sds.k2_gather_subobject_data_for_blueprint(bp)
    except Exception as exc:
        log("subobject gather failed: {}".format(exc))
        return []
    objects = []
    for handle in handles or []:
        obj = None
        try:
            obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(handle)
        except Exception:
            try:
                data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
                obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
            except Exception as exc:
                log("  handle decode failed: {}".format(exc))
        if obj:
            objects.append(obj)
            dump_comp("  sub", obj)
    return objects


def dump_hero(path):
    log("==== {}".format(path))
    bp = unreal.EditorAssetLibrary.load_asset(path)
    cls = load_class(path)
    cdo = unreal.get_default_object(cls) if cls else None
    if not cdo:
        log("no CDO")
        return bp, None
    log("class={}".format(cdo.get_class().get_name()))
    try:
        comps = cdo.get_components_by_class(unreal.SceneComponent)
    except Exception as exc:
        log("get_components_by_class failed: {}".format(exc))
        comps = []
    for comp in comps:
        dump_comp("CDO", comp)
    dump_subobjects(bp, path)
    return bp, cdo


def list_art():
    log("--- Art assets ---")
    try:
        assets = unreal.EditorAssetLibrary.list_assets(ART, True, False)
    except Exception as exc:
        log("list_assets failed: {}".format(exc))
        return
    for a in assets:
        log("  {}".format(a))


def ensure_hat_material():
    color = unreal.EditorAssetLibrary.load_asset(ART + "/T_WizardHat_Color")
    normal = unreal.EditorAssetLibrary.load_asset(ART + "/T_WizardHat_Normal")
    roughness = unreal.EditorAssetLibrary.load_asset(ART + "/T_WizardHat_Roughness")
    if not color:
        log("missing hat color texture")
        return None
    if unreal.EditorAssetLibrary.does_asset_exist(HAT_MAT):
        unreal.EditorAssetLibrary.delete_asset(HAT_MAT)
    factory = unreal.MaterialFactoryNew()
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_WizardHat", ART, unreal.Material, factory
    )
    color_samp = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionTextureSampleParameter2D, -380, -180
    )
    color_samp.set_editor_property("parameter_name", "Color")
    color_samp.set_editor_property("texture", color)
    unreal.MaterialEditingLibrary.connect_material_property(
        color_samp, "RGB", unreal.MaterialProperty.MP_BASE_COLOR
    )
    if roughness:
        rough_samp = unreal.MaterialEditingLibrary.create_material_expression(
            mat, unreal.MaterialExpressionTextureSampleParameter2D, -380, 40
        )
        rough_samp.set_editor_property("parameter_name", "Roughness")
        rough_samp.set_editor_property("texture", roughness)
        unreal.MaterialEditingLibrary.connect_material_property(
            rough_samp, "R", unreal.MaterialProperty.MP_ROUGHNESS
        )
    if normal:
        nrm_samp = unreal.MaterialEditingLibrary.create_material_expression(
            mat, unreal.MaterialExpressionTextureSampleParameter2D, -380, 240
        )
        nrm_samp.set_editor_property("parameter_name", "Normal")
        nrm_samp.set_editor_property("texture", normal)
        unreal.MaterialEditingLibrary.connect_material_property(
            nrm_samp, "RGB", unreal.MaterialProperty.MP_NORMAL
        )
    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    log("created {}".format(mat.get_path_name()))
    return mat


def hat_scale(mesh):
    try:
        bounds = mesh.get_bounds()
        height = max(float(bounds.box_extent.z) * 2.0, 1.0)
    except Exception as exc:
        log("bounds failed: {}".format(exc))
        return unreal.Vector(1.0, 1.0, 1.0)
    scale = max(0.01, min(28.0 / height, 4.0))
    log("hat height={} fitted scale={}".format(height, scale))
    return unreal.Vector(scale, scale, scale)


def assign_mesh_material(mesh, mat):
    if not mesh or not mat:
        return
    try:
        mesh.set_material(0, mat)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        log("assigned {} to {}".format(mat.get_name(), mesh.get_path_name()))
    except Exception as exc:
        log("set_material failed: {}".format(exc))


def find_hat_objects(bp, cdo):
    found = []
    try:
        for comp in cdo.get_components_by_class(unreal.StaticMeshComponent) or []:
            found.append(comp)
    except Exception:
        pass
    for obj in dump_subobjects(bp, "fix-search"):
        name = obj.get_name().lower()
        if isinstance(obj, unreal.StaticMeshComponent) or "hat" in name or "wizard" in name or "staticmesh" in name:
            found.append(obj)
    return found


def find_skel(cdo):
    try:
        for comp in cdo.get_components_by_class(unreal.SkeletalMeshComponent) or []:
            if comp.get_name() == "CharacterMesh0":
                return comp
        comps = cdo.get_components_by_class(unreal.SkeletalMeshComponent) or []
        return comps[0] if comps else None
    except Exception:
        return None


def pick_socket(skel):
    names = []
    try:
        names = [str(n) for n in skel.get_all_socket_names()]
    except Exception:
        names = []
    log("sockets={}".format(names[:40]))
    for cand in ("head", "Head"):
        if cand in names:
            return cand
    for n in names:
        if "head" in n.lower():
            return n
    return "head"


def compile_bp(bp):
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        log("compiled {}".format(bp.get_name()))
    except Exception as exc:
        log("compile failed: {}".format(exc))


def fix_mage(bp, cdo, hat, mat):
    skel = find_skel(cdo)
    if skel:
        parent_mesh = unreal.EditorAssetLibrary.load_asset(PARENT_MESH)
        current = prop(skel, "skeletal_mesh_asset") or prop(skel, "skeletal_mesh")
        log("character mesh {}".format(asset_name(current)))
        if (not current) and parent_mesh:
            try:
                skel.set_editor_property("skeletal_mesh_asset", parent_mesh)
            except Exception:
                skel.set_editor_property("skeletal_mesh", parent_mesh)
            log("restored SKM_Quinn_Simple")
        try:
            skel.set_editor_property("override_materials", [])
            log("cleared character override materials")
        except Exception as exc:
            log("clear overrides failed: {}".format(exc))

    hats = find_hat_objects(bp, cdo)
    log("hat objects count={}".format(len(hats)))
    socket = pick_socket(skel) if skel else "head"
    for tmpl in hats:
        if not isinstance(tmpl, unreal.StaticMeshComponent):
            continue
        log("fixing {}".format(tmpl.get_name()))
        tmpl.set_editor_property("static_mesh", hat)
        try:
            tmpl.set_editor_property("override_materials", [])
        except Exception:
            pass
        try:
            tmpl.set_editor_property("collision_enabled", unreal.CollisionEnabled.NO_COLLISION)
        except Exception:
            pass
        tmpl.set_editor_property("relative_scale3d", hat_scale(hat))
        loc = prop(tmpl, "relative_location") or unreal.Vector(0, 0, 0)
        if abs(loc.x) < 0.01 and abs(loc.y) < 0.01 and abs(loc.z) < 0.01:
            tmpl.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, 8.0))
        try:
            tmpl.set_editor_property("attach_socket_name", socket)
            log("socket {}".format(socket))
        except Exception as exc:
            log("socket failed: {}".format(exc))

    compile_bp(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    log("saved mage")


def main():
    list_art()
    dump_hero(BASE_BP)
    bp, cdo = dump_hero(MAGE_BP)
    hat = unreal.EditorAssetLibrary.load_asset(HAT_MESH)
    log("hat mesh load={}".format(asset_name(hat)))
    if hat:
        try:
            mats = hat.get_editor_property("static_materials")
            log("hat static_materials={}".format(mats))
        except Exception as exc:
            log("hat materials dump failed: {}".format(exc))
    mat = ensure_hat_material()
    assign_mesh_material(hat, mat)
    if not bp or not cdo or not hat:
        log("cannot fix, missing bp/cdo/hat")
        return
    log("==== FIX ====")
    fix_mage(bp, cdo, hat, mat)
    log("==== AFTER ====")
    dump_hero(MAGE_BP)
    log("Done")


if __name__ == "__main__":
    try:
        main()
    except Exception:
        log(traceback.format_exc())
        raise
    finally:
        write_log()
