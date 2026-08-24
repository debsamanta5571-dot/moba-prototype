import traceback
from pathlib import Path
import shutil
import unreal

ART = "/Game/Moba/Art"
LOG = Path(r"C:\Users\Thedo\Documents\MobaRepo\MobaProject\Saved\ImportSlowShotIcon.log")
STAGE = Path(r"C:\Users\Thedo\Documents\MobaRepo\MobaProject\Saved\IconImport")
SRC = Path(r"C:\Users\Thedo\Documents\MobaRepo\MobaProject\Content\Moba\Art\T_Icon_SlowShot.png")
DEST_NAME = "T_Icon_SlowShot"
BP_PATH = "/Game/Moba/Abilities/BP_MageAttack"
lines = []


def log(msg):
    lines.append(str(msg))
    unreal.log(str(msg))


def main():
    STAGE.mkdir(parents=True, exist_ok=True)
    staged = STAGE / SRC.name
    shutil.copy2(SRC, staged)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(staged))
    task.set_editor_property("destination_path", ART)
    task.set_editor_property("destination_name", DEST_NAME)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = list(task.get_editor_property("imported_object_paths") or [])
    log("Imported {} -> {}".format(SRC.name, imported))
    tex_path = imported[0] if imported else (ART + "/" + DEST_NAME)

    tex = unreal.EditorAssetLibrary.load_asset(tex_path)
    if not tex:
        log("Missing texture {}".format(tex_path))
        return
    tex.set_editor_property("srgb", True)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    try:
        tex.set_editor_property("never_stream", True)
    except Exception as exc:
        log("never_stream skipped: {}".format(exc))
    unreal.EditorAssetLibrary.save_loaded_asset(tex)
    log("Tuned {}".format(tex_path))

    if not unreal.EditorAssetLibrary.does_asset_exist(BP_PATH):
        log("No ability BP {}".format(BP_PATH))
        return
    bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)
    cls = unreal.EditorAssetLibrary.load_blueprint_class(BP_PATH)
    cdo = unreal.get_default_object(cls)
    cdo.set_editor_property("icon", tex)
    log("Assigned icon on {}".format(BP_PATH))
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        log("Compiled {}".format(BP_PATH))
    except Exception as exc:
        log("Compile skipped: {}".format(exc))
    unreal.EditorAssetLibrary.save_asset(BP_PATH)


if __name__ == "__main__":
    try:
        main()
    except Exception:
        log(traceback.format_exc())
        raise
    finally:
        LOG.write_text("\n".join(lines) + "\n", encoding="utf-8")
