import traceback
from pathlib import Path
import unreal

LOG = Path(r"C:\Users\Thedo\Documents\MobaRepo\MobaProject\Saved\AuditBrawlerAbilities.log")
lines = []


def log(msg):
    lines.append(str(msg))
    unreal.log(str(msg))


def cls(path):
    try:
        return unreal.EditorAssetLibrary.load_blueprint_class(path)
    except Exception as exc:
        log("load class {} failed: {}".format(path, exc))
        return None


def dump_hero(path):
    log("==== {}".format(path))
    c = cls(path)
    if not c:
        return None
    cdo = unreal.get_default_object(c)
    log("class={}".format(cdo.get_class().get_name()))
    for name in (
        "input_mapping",
        "move_action",
        "look_action",
        "jump_action",
        "ability1",
        "ability2",
        "ability3",
        "ability4",
        "ability1_input",
        "ability2_input",
        "ability3_input",
        "ability4_input",
    ):
        try:
            val = cdo.get_editor_property(name)
        except Exception:
            val = "<missing>"
        log("  {}={}".format(name, val.get_path_name() if getattr(val, "get_path_name", None) else val))
    try:
        slots = cdo.get_editor_property("ability_slots")
    except Exception as exc:
        log("  ability_slots failed: {}".format(exc))
        slots = None
    log("  ability_slots num={}".format(len(slots) if slots is not None else "None"))
    if slots:
        for i, slot in enumerate(slots):
            ab = slot.get_editor_property("ability") if hasattr(slot, "get_editor_property") else None
            inp = slot.get_editor_property("input") if hasattr(slot, "get_editor_property") else None
            label = slot.get_editor_property("key_label") if hasattr(slot, "get_editor_property") else None
            log("    [{}] ability={} input={} label={}".format(
                i,
                ab.get_path_name() if ab else None,
                inp.get_path_name() if inp else None,
                label,
            ))
    return cdo


def copy_slots(src, dest_path):
    src_slots = src.get_editor_property("ability_slots")
    bp = unreal.EditorAssetLibrary.load_asset(dest_path)
    dest_cls = cls(dest_path)
    dest = unreal.get_default_object(dest_cls)
    copied = []
    for slot in src_slots:
        copied.append(slot)
    dest.set_editor_property("ability_slots", copied)
    log("copied {} slots onto {}".format(len(copied), dest_path))
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        log("compiled")
    except Exception as exc:
        log("compile: {}".format(exc))
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    log("saved")


def main():
    base = dump_hero("/Game/Moba/BP_MobaBaseChar")
    dump_hero("/Game/Moba/BP_Brawler")
    dump_hero("/Game/Moba/BP_Mage")
    if base:
        log("==== COPY BASE SLOTS TO BRAWLER ====")
        copy_slots(base, "/Game/Moba/BP_Brawler")
        dump_hero("/Game/Moba/BP_Brawler")
    log("Done")


if __name__ == "__main__":
    try:
        main()
    except Exception:
        log(traceback.format_exc())
        raise
    finally:
        LOG.write_text("\n".join(lines) + "\n", encoding="utf-8")
