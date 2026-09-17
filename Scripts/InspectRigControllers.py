import json
import unreal


asset = unreal.load_asset("/Game/Chimera/Character/Part/Leg/CR_CMLegLProcedural")
hierarchy_controller = asset.get_hierarchy_controller() if asset else None
rig_controller = asset.get_controller() if asset else None
hierarchy = asset.hierarchy if asset else None
model = asset.get_default_model() if asset else None


def public_methods(value):
    return [name for name in dir(value) if not name.startswith("_")] if value else []


payload = {
    "hierarchy_controller": public_methods(hierarchy_controller),
    "rig_controller": public_methods(rig_controller),
    "hierarchy": public_methods(hierarchy),
    "model": public_methods(model),
}
unreal.log("CM_RIG_CONTROLLERS " + json.dumps(payload, ensure_ascii=True))
with open(r"C:/Chimera/Saved/RigControllers.json", "w", encoding="utf-8") as output:
    json.dump(payload, output, indent=2, ensure_ascii=True)
