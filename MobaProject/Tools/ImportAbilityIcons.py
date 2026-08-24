import traceback
from pathlib import Path
import shutil
import unreal

ART = "/Game/Moba/Art"
LOG = Path(r"C:\Users\Thedo\Documents\MobaRepo\MobaProject\Saved\ImportAbilityIcons.log")
STAGE = Path(r"C:\Users\Thedo\Documents\MobaRepo\MobaProject\Saved\IconImport")
SRC_ART = Path(r"C:\Users\Thedo\Documents\MobaRepo\MobaProject\Content\Moba\Art")
lines = []

ICONS = (
    ("T_Icon_Blink", "T_Icon_Blink.png", "/Game/Moba/Abilities/BP_GA_Blink"),
    ("T_Icon_BigShot", "T_Icon_BigShot.png", "/Game/Moba/Abilities/BP_GA_BigShot"),
    ("T_Icon_FireSlash", "T_Icon_FireSlash.png", "/Game/Moba/Abilities/BP_GA_Flamestrike"),
)


def log(msg):
    lines.append(str(msg))
    unreal.log(str(msg))


def import_png(src_path, dest_name):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(src_path))
    task.set_editor_property("destination_path", ART)
    task.set_editor_property("destination_name", dest_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = list(task.get_editor_property("imported_object_paths") or [])
    log("Imported {} -> {}".format(src_path.name, imported))
    if imported:
        return imported[0]
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    params = unreal.ImportAssetParameters()
    params.set_editor_property("destination_path", ART)
    params.set_editor_property("is_automated", True)
    params.set_editor_property("replace_existing", True)
    ok = mgr.import_asset(str(src_path), params)
    log("Interchange {} -> {}".format(src_path.name, ok))
    return ART + "/" + dest_name if ok else None


def tune_texture(path):
    tex = unreal.EditorAssetLibrary.load_asset(path)
    if not tex:
        log("Missing texture {}".format(path))
        return None
    tex.set_editor_property("srgb", True)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    try:
        tex.set_editor_property("never_stream", True)
    except Exception as exc:
        log("never_stream skipped: {}".format(exc))
    try:
        tex.set_editor_property(
            "compression_settings",
            unreal.TextureCompressionSettings.TC_DEFAULT,
        )
    except Exception as exc:
        log("compression skipped: {}".format(exc))
    unreal.EditorAssetLibrary.save_loaded_asset(tex)
    log("Tuned {}".format(path))
    return tex


def assign_icon(bp_path, tex):
    if not unreal.EditorAssetLibrary.does_asset_exist(bp_path):
        log("No ability BP {}".format(bp_path))
        return
    bp = unreal.EditorAssetLibrary.load_asset(bp_path)
    cls = unreal.EditorAssetLibrary.load_blueprint_class(bp_path)
    if not cls:
        log("No class {}".format(bp_path))
        return
    cdo = unreal.get_default_object(cls)
    try:
        cdo.set_editor_property("icon", tex)
        log("Assigned icon on {}".format(bp_path))
    except Exception as exc:
        log("Assign failed {}: {}".format(bp_path, exc))
        return
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        log("Compiled {}".format(bp_path))
    except Exception as exc:
        log("Compile skipped {}: {}".format(bp_path, exc))
    unreal.EditorAssetLibrary.save_asset(bp_path)


def main():
    STAGE.mkdir(parents=True, exist_ok=True)
    for dest_name, png_name, bp_path in ICONS:
        src = SRC_ART / png_name
        if not src.exists():
            log("Missing source {}".format(src))
            continue
        staged = STAGE / png_name
        shutil.copy2(src, staged)
        imported = import_png(staged, dest_name)
        tex_path = imported or (ART + "/" + dest_name)
        tex = tune_texture(tex_path)
        if tex:
            assign_icon(bp_path, tex)


if __name__ == "__main__":
    try:
        main()
    except Exception:
        log(traceback.format_exc())
        raise
    finally:
        LOG.write_text("\n".join(lines) + "\n", encoding="utf-8")
