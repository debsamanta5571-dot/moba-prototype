import traceback
from pathlib import Path
import unreal

LOG = Path(r"C:\Users\Thedo\Documents\MobaRepo\MobaProject\Saved\RemovePlacedHeroes.log")
lines = []
MAP = "/Game/Moba/Maps/MobaTestMap"


def log(msg):
    lines.append(str(msg))
    unreal.log(str(msg))


def main():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not les.load_level(MAP):
        log("failed to load {}".format(MAP))
        return
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    removed = 0
    for actor in list(actors.get_all_level_actors() or []):
        cls_name = actor.get_class().get_name()
        label = actor.get_actor_label()
        is_hero = False
        try:
            is_hero = isinstance(actor, unreal.MobaBaseCharacter)
        except Exception:
            is_hero = False
        if (not is_hero) and ("BP_Mage" in label or "BP_Brawler" in label or cls_name in ("BP_Mage_C", "BP_Brawler_C")):
            is_hero = True
        log("actor {} class={}".format(label, cls_name))
        if is_hero:
            log("DESTROY {}".format(label))
            actors.destroy_actor(actor)
            removed += 1
    if removed:
        les.save_current_level()
    log("removed {} placed heroes, saved={}".format(removed, removed > 0))


if __name__ == "__main__":
    try:
        main()
    except Exception:
        log(traceback.format_exc())
        raise
    finally:
        LOG.write_text("\n".join(lines) + "\n", encoding="utf-8")
