import json
import unreal


asset_path = "/Game/Chimera/Character/Part/Leg/CR_CMLegLProcedural"
asset = unreal.load_asset(asset_path)
factory = unreal.ControlRigBlueprintFactory
payload = {
    "asset_loaded": bool(asset),
    "asset_class": asset.get_class().get_name() if asset else None,
    "asset_methods": [
        name
        for name in dir(asset)
        if any(token in name.lower() for token in ("hierarchy", "rigvm", "controller", "graph", "model", "save"))
    ] if asset else [],
    "factory_methods": [name for name in dir(factory) if not name.startswith("_")],
}
if asset:
    for name in ("get_hierarchy", "get_hierarchy_controller", "get_vm_model", "get_rigvm_client", "get_controller", "get_preview_mesh"):
        if hasattr(asset, name):
            try:
                value = getattr(asset, name)()
                payload[name] = str(value)
            except Exception as exc:
                payload[name] = "ERROR: " + repr(exc)
unreal.log("CM_CONTROL_RIG_INSPECT " + json.dumps(payload, ensure_ascii=True))
with open(r"C:/Chimera/Saved/ControlRigInspect.json", "w", encoding="utf-8") as output:
    json.dump(payload, output, indent=2, ensure_ascii=True)
