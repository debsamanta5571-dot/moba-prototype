import traceback
from pathlib import Path
import unreal

LOG = Path(r"C:\Users\Thedo\Documents\MobaRepo\MobaProject\Saved\EnableFlamestrikeRing.log")
BP = "/Game/Moba/Abilities/BP_GA_Flamestrike"
lines = []


def log(msg):
    lines.append(str(msg))
    unreal.log(str(msg))


def main():
    if not unreal.EditorAssetLibrary.does_asset_exist(BP):
        log("missing " + BP)
        return
    bp = unreal.EditorAssetLibrary.load_asset(BP)
    cls = unreal.EditorAssetLibrary.load_blueprint_class(BP)
    cdo = unreal.get_default_object(cls)
    cdo.set_editor_property("show_range_ring", True)
    try:
        cdo.set_editor_property("range_ring_lifetime", 0.7)
    except Exception as exc:
        log("lifetime skipped: {}".format(exc))
    log("show_range_ring={}".format(cdo.get_editor_property("show_range_ring")))
    log("range={}".format(cdo.get_editor_property("range")))
    log("radius={}".format(cdo.get_editor_property("radius")))
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        log("compiled")
    except Exception as exc:
        log("compile skipped: {}".format(exc))
    unreal.EditorAssetLibrary.save_asset(BP)
    log("saved")


if __name__ == "__main__":
    try:
        main()
    except Exception:
        log(traceback.format_exc())
        raise
    finally:
        LOG.write_text("\n".join(lines) + "\n", encoding="utf-8")
