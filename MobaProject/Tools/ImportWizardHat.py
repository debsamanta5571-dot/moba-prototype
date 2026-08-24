import traceback
from pathlib import Path

import unreal

ART = "/Game/Moba/Art"
# Import from Downloads so Interchange does not skip files already sitting in Content.
SRC_FBX = Path(r"C:\Users\Thedo\Downloads\black-wizard-hat\source\Wizard Hat.fbx")
SRC_TEX = Path(r"C:\Users\Thedo\Downloads\black-wizard-hat\textures")
LOG_PATH = Path(r"C:\Users\Thedo\Documents\MobaRepo\MobaProject\Saved\ImportWizardHat.log")
lines = []


def log(msg):
    text = str(msg)
    lines.append(text)
    unreal.log(text)


def write_log():
    try:
        LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
        LOG_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")
    except Exception as exc:
        unreal.log_error("Failed to write import log: {}".format(exc))


def import_file_interchange(src_path):
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    params = unreal.ImportAssetParameters()
    params.set_editor_property("destination_path", ART)
    params.set_editor_property("is_automated", True)
    params.set_editor_property("replace_existing", True)
    ok = mgr.import_asset(str(src_path), params)
    log("Interchange {} -> {}".format(src_path.name, ok))
    return ok


def import_file(src_path, dest_name, options=None):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(src_path))
    task.set_editor_property("destination_path", ART)
    task.set_editor_property("destination_name", dest_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", True)
    if options is not None:
        task.set_editor_property("options", options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = list(task.get_editor_property("imported_object_paths") or [])
    log("Imported {} -> {}".format(src_path.name, imported))
    if not imported:
        import_file_interchange(src_path)
    return imported


def make_fbx_options():
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_animations", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.set_editor_property("original_import_type", unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.set_editor_property("automated_import_should_detect_type", False)
    static_data = options.get_editor_property("static_mesh_import_data")
    static_data.set_editor_property("combine_meshes", True)
    static_data.set_editor_property("generate_lightmap_u_vs", True)
    static_data.set_editor_property("auto_generate_collision", True)
    static_data.set_editor_property("convert_scene", True)
    static_data.set_editor_property("force_front_x_axis", False)
    static_data.set_editor_property("convert_scene_unit", True)
    return options


def set_texture(path, compression, srgb):
    tex = unreal.EditorAssetLibrary.load_asset(path)
    if not tex:
        log("Missing texture {}".format(path))
        return None
    tex.set_editor_property("compression_settings", compression)
    tex.set_editor_property("srgb", srgb)
    unreal.EditorAssetLibrary.save_loaded_asset(tex)
    log("Texture {} compression={} srgb={}".format(path, compression, srgb))
    return tex


def create_material(color, normal, roughness):
    mat_path = ART + "/M_WizardHat"
    if unreal.EditorAssetLibrary.does_asset_exist(mat_path):
        unreal.EditorAssetLibrary.delete_asset(mat_path)
    factory = unreal.MaterialFactoryNew()
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_WizardHat", ART, unreal.Material, factory
    )
    if not mat:
        log("Failed to create M_WizardHat")
        return None

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
    log("Created {}".format(mat.get_path_name()))
    return mat


def assign_material(mesh_path, mat):
    mesh = unreal.EditorAssetLibrary.load_asset(mesh_path)
    if not mesh or not mat:
        log("Cannot assign material mesh={} mat={}".format(mesh_path, mat))
        return
    mesh.set_material(0, mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    log("Assigned {} to {}".format(mat.get_name(), mesh_path))


def main():
    log("Importing wizard hat into {}".format(ART))
    unreal.EditorAssetLibrary.make_directory(ART)

    pngs = [
        (SRC_TEX / "Wizard_Hat_ColorMap.png", "T_WizardHat_Color"),
        (SRC_TEX / "Wizard_Hat_NormalMap.png", "T_WizardHat_Normal"),
        (SRC_TEX / "Wizard_Hat_RoughnessMap.png", "T_WizardHat_Roughness"),
    ]
    for path, dest_name in pngs:
        if path.is_file():
            import_file(path, dest_name)
        else:
            log("Skip missing {}".format(path))

    if SRC_FBX.is_file():
        import_file(SRC_FBX, "SM_WizardHat", make_fbx_options())
    else:
        log("Missing {}".format(SRC_FBX))

    color = set_texture(
        ART + "/T_WizardHat_Color",
        unreal.TextureCompressionSettings.TC_DEFAULT,
        True,
    )
    normal = set_texture(
        ART + "/T_WizardHat_Normal",
        unreal.TextureCompressionSettings.TC_NORMALMAP,
        False,
    )
    roughness = set_texture(
        ART + "/T_WizardHat_Roughness",
        unreal.TextureCompressionSettings.TC_MASKS,
        False,
    )
    mat = create_material(color, normal, roughness)

    mesh_path = ART + "/SM_WizardHat"
    if not unreal.EditorAssetLibrary.does_asset_exist(mesh_path):
        # Interchange may keep the source filename.
        alt = ART + "/WizardHat"
        if unreal.EditorAssetLibrary.does_asset_exist(alt):
            mesh_path = alt
    assign_material(mesh_path, mat)
    log("Done")


if __name__ == "__main__":
    try:
        main()
    except Exception:
        log(traceback.format_exc())
        raise
    finally:
        write_log()
