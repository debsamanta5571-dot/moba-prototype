import traceback
from pathlib import Path
import unreal

LOG = Path(r"C:\Users\Thedo\Documents\MobaRepo\MobaProject\Saved\AuditAbilityNotifyTags.log")
lines = []


def log(msg):
    lines.append(str(msg))
    unreal.log(str(msg))


def dump_ability(path):
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    cdo = unreal.get_default_object(cls) if cls else None
    if not cdo:
        log("missing {}".format(path))
        return
    notify = None
    cooldown = None
    activate = None
    montage = None
    try:
        notify = cdo.get_editor_property("anim_notify_tag")
    except Exception as exc:
        log("  anim_notify_tag: {}".format(exc))
    try:
        activate = cdo.get_editor_property("activate_event_tag")
    except Exception:
        pass
    for prop in ("melee_montage", "skillshot_montage"):
        try:
            montage = cdo.get_editor_property(prop)
            if montage:
                break
        except Exception:
            pass
    log("{} notify={} activate={} montage={}".format(
        path,
        notify,
        activate,
        montage.get_path_name() if montage else None,
    ))


def set_notify_tag(path, tag_name):
    bp = unreal.EditorAssetLibrary.load_asset(path)
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    cdo = unreal.get_default_object(cls)
    tag = unreal.GameplayTag()
    tag.set_editor_property("tag_name", tag_name)
    cdo.set_editor_property("event_tag", tag)
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    except Exception as exc:
        log("compile {}: {}".format(path, exc))
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    log("set {} event_tag={}".format(path, tag_name))


def main():
    for path in (
        "/Game/Moba/Abilities/BP_GA_Punch",
        "/Game/Moba/Abilities/BP_GA_Skillshot",
        "/Game/Moba/Abilities/BP_MageAttack",
        "/Game/Moba/Abilities/BP_GA_Dash",
        "/Game/Moba/Abilities/BP_GA_Explosion",
    ):
        dump_ability(path)

    log("==== FIX NOTIFY BPS ====")
    set_notify_tag("/Game/Moba/Montages/Anim_Notify_Melee", "Ability.1")
    set_notify_tag("/Game/Moba/Montages/Anim_Notify_Projectile", "Ability.2")

    for path in (
        "/Game/Moba/Montages/Anim_Notify_Melee",
        "/Game/Moba/Montages/Anim_Notify_Projectile",
    ):
        cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
        cdo = unreal.get_default_object(cls)
        log("after {} event_tag={}".format(path, cdo.get_editor_property("event_tag")))
    log("Done")


if __name__ == "__main__":
    try:
        main()
    except Exception:
        log(traceback.format_exc())
        raise
    finally:
        LOG.write_text("\n".join(lines) + "\n", encoding="utf-8")
