import traceback
from pathlib import Path
import unreal

LOG = Path(r"C:\Users\Thedo\Documents\MobaRepo\MobaProject\Saved\RestoreMageHatScale.log")
lines = []


def log(msg):
    lines.append(str(msg))
    unreal.log(str(msg))


bp = unreal.EditorAssetLibrary.load_asset("/Game/Moba/BP_Mage")
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
handles = sds.k2_gather_subobject_data_for_blueprint(bp)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
hat = None
mesh = None
hat_handle = None
mesh_handle = None
for handle in handles or []:
    obj = None
    try:
        obj = lib.get_associated_object(handle)
    except Exception:
        try:
            obj = lib.get_object(handle)
        except Exception:
            obj = None
    if not obj:
        continue
    name = obj.get_name()
    log("obj {}".format(name))
    if isinstance(obj, unreal.StaticMeshComponent) or "StaticMesh" in name:
        hat = obj
        hat_handle = handle
    if name == "CharacterMesh0":
        mesh = obj
        mesh_handle = handle

if hat:
    hat.set_editor_property("relative_scale3d", unreal.Vector(0.205844, 0.205844, 0.205844))
    log("restored scale 0.205844")
    try:
        hat.set_editor_property("collision_enabled", unreal.CollisionEnabled.NO_COLLISION)
    except Exception:
        pass

attach_ok = False
if hat_handle and mesh_handle:
    for name in ("k2_attach_subobject", "attach_subobject", "attach"):
        fn = getattr(sds, name, None)
        if not fn:
            continue
        try:
            fn(mesh_handle, hat_handle)
            attach_ok = True
            log("attached via {}".format(name))
            break
        except Exception as exc:
            log("{} failed: {}".format(name, exc))

mesh_asset = unreal.EditorAssetLibrary.load_asset("/Game/Moba/Art/WizardHat")
mat = unreal.EditorAssetLibrary.load_asset("/Game/Moba/Art/M_WizardHat")
if mesh_asset and mat:
    mesh_asset.set_material(0, mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh_asset)
    log("hat material still M_WizardHat")

try:
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
except Exception as exc:
    log("compile: {}".format(exc))
unreal.EditorAssetLibrary.save_loaded_asset(bp)
log("attach_ok={} saved".format(attach_ok))
LOG.write_text("\n".join(lines) + "\n", encoding="utf-8")
