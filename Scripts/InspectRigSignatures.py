import json
import unreal


asset = unreal.load_asset("/Game/Chimera/Character/Part/Leg/CR_CMLegLProcedural")
hierarchy_controller = asset.get_hierarchy_controller()
rig_controller = asset.get_controller()
names = [
    (hierarchy_controller, "add_bone"),
    (hierarchy_controller, "add_control"),
    (hierarchy_controller, "set_control_settings"),
    (rig_controller, "add_unit_node"),
    (rig_controller, "add_unit_node_with_defaults"),
    (rig_controller, "add_unit_node_from_struct_path"),
    (rig_controller, "set_pin_default_value"),
    (rig_controller, "add_link"),
    (rig_controller, "remove_nodes"),
    (asset, "get_or_create_controller"),
]
payload = {}
for owner, name in names:
    fn = getattr(owner, name)
    payload[name] = {
        "doc": getattr(fn, "__doc__", None),
        "repr": repr(fn),
    }
unreal.log("CM_RIG_SIGNATURES " + json.dumps(payload, ensure_ascii=True))
with open(r"C:/Chimera/Saved/RigSignatures.json", "w", encoding="utf-8") as output:
    json.dump(payload, output, indent=2, ensure_ascii=True)
