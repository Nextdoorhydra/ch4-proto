import json
import unreal


asset_path = "/Game/Chimera/Character/Part/Leg/CR_CMLegLProcedural"
asset = unreal.load_asset(asset_path)
if not asset:
    raise RuntimeError("Control Rig asset was not found: " + asset_path)

controller = asset.get_or_create_controller()
model = asset.get_default_model()
existing = {node.get_name(): node for node in model.get_nodes()}


def add_node(name, script_struct, x, y):
    if name in existing:
        return existing[name], False
    node = controller.add_unit_node(
        script_struct.static_struct(),
        "Execute",
        unreal.Vector2D(float(x), float(y)),
        name,
        False,
        False,
    )
    if not node:
        raise RuntimeError("Failed to add RigVM node: " + name)
    existing[name] = node
    return node, True


nodes = {}
created = []
for name, struct, x, y in (
    ("ForwardsSolve", unreal.RigUnit_BeginExecution, 0, 0),
    ("GetLegHip", unreal.RigUnit_GetControlTransform, 260, -240),
    ("SetLegHip", unreal.RigUnit_SetBoneTransform, 520, -240),
    ("GetLegFootIK", unreal.RigUnit_GetControlTransform, 260, 0),
    ("GetLegKneePole", unreal.RigUnit_GetControlVector, 260, 220),
    ("GetLegPlantAlpha", unreal.RigUnit_GetControlFloat, 260, 420),
    ("LegTwoBoneIK", unreal.RigUnit_TwoBoneIKSimple, 820, 80),
):
    node, was_created = add_node(name, struct, x, y)
    nodes[name] = node
    if was_created:
        created.append(name)


def pin_info(node):
    result = []
    for pin in node.get_pins():
        result.append({
            "name": pin.get_name(),
            "path": pin.get_pin_path(),
            "direction": str(pin.get_direction()),
            "default": pin.get_default_value(),
            "links": [str(link) for link in pin.get_links()],
        })
    return result


payload = {
    "asset": asset_path,
    "created": created,
    "nodes": {name: pin_info(node) for name, node in nodes.items()},
}
try:
    first_node = next(iter(nodes.values()))
    first_pin = next(iter(first_node.get_pins()))
    payload["pin_methods"] = [name for name in dir(first_pin) if not name.startswith("_")]
except Exception as exc:
    payload["pin_methods_error"] = repr(exc)
unreal.log("CM_LEG_RIG_NODES_READY " + json.dumps(payload, ensure_ascii=True))
with open(r"C:/Chimera/Saved/LegRigNodesReady.json", "w", encoding="utf-8") as output:
    json.dump(payload, output, indent=2, ensure_ascii=True)
