import traceback
from pathlib import Path

import unreal

MAP_PATH = "/Game/Moba/Maps/MobaMenu"
TEMPLATE_PATH = "/Engine/Maps/Templates/Template_Default"
LOG_PATH = Path(r"C:\Users\Thedo\Documents\MobaRepo\MobaProject\Saved\RebuildMobaMenu.log")
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
        unreal.log_error("Failed to write rebuild log: {}".format(exc))


def load_menu_game_mode():
    cls = unreal.load_class(None, "/Script/MobaProject.MobaMenuGameMode")
    if cls:
        return cls
    if hasattr(unreal, "MobaMenuGameMode"):
        return unreal.MobaMenuGameMode.static_class()
    return None


def main():
    log("Rebuilding MobaMenu from {}".format(TEMPLATE_PATH))
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        raise RuntimeError("MobaMenu still exists; delete the umap on disk first")

    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level_editor:
        raise RuntimeError("LevelEditorSubsystem missing")

    created = level_editor.new_level_from_template(MAP_PATH, TEMPLATE_PATH)
    log("new_level_from_template: {}".format(created))
    if not created:
        raise RuntimeError("Failed to create MobaMenu from Template_Default")

    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = ues.get_editor_world()
    log("World: {}".format(world.get_path_name() if world else None))
    if not world:
        raise RuntimeError("No editor world after creating MobaMenu")

    gm = load_menu_game_mode()
    log("Menu GameMode class: {}".format(gm))
    if not gm:
        raise RuntimeError("Could not load AMobaMenuGameMode")

    world.get_world_settings().set_editor_property("default_game_mode", gm)
    log("World Settings DefaultGameMode set")

    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_sub.get_all_level_actors()
    labels = sorted(a.get_actor_label() for a in actors)
    log("Actors: {}".format(", ".join(labels)))

    has_start = any(isinstance(a, unreal.PlayerStart) for a in actors)
    if not has_start:
        actor_sub.spawn_actor_from_class(
            unreal.PlayerStart,
            unreal.Vector(0.0, 0.0, 96.0),
            unreal.Rotator(0.0, 0.0, 0.0),
        )
        log("Spawned PlayerStart")

    cam = actor_sub.spawn_actor_from_class(
        unreal.CameraActor,
        unreal.Vector(0.0, -620.0, 220.0),
        unreal.Rotator(-8.0, 90.0, 0.0),
    )
    if cam:
        cam.set_actor_label("MenuCamera")
        log("Spawned MenuCamera")

    saved = level_editor.save_current_level()
    log("save_current_level: {}".format(saved))
    log("MobaMenu rebuild complete")


if __name__ == "__main__":
    try:
        main()
    except Exception:
        log(traceback.format_exc())
        write_log()
        raise
    write_log()
