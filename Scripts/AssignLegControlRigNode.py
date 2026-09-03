import json
import unreal


anim_path = "/Game/Chimera/ABP_CMLegLProcedural"
rig_path = "/Game/Chimera/Character/Part/Leg/CR_CMLegLProcedural"
anim_blueprint = unreal.load_asset(anim_path)
rig_blueprint = unreal.load_asset(rig_path)
if not anim_blueprint:
    raise RuntimeError("Animation Blueprint was not found: " + anim_path)
if not rig_blueprint:
    raise RuntimeError("Control Rig Blueprint was not found: " + rig_path)

rig_class = rig_blueprint.get_control_rig_class()
if not rig_class:
    raise RuntimeError("Control Rig generated class is unavailable: " + rig_path)

control_rig_nodes = []
for graph in anim_blueprint.get_animation_graphs():
    try:
        control_rig_nodes.extend(graph.get_graph_nodes_of_class(unreal.AnimGraphNode_ControlRig))
    except Exception:
        pass
if not control_rig_nodes:
    raise RuntimeError("AnimGraphNode_ControlRig was not found in " + anim_path)

target = next((node for node in control_rig_nodes if node.get_name() == "AnimGraphNode_ControlRig_0"), control_rig_nodes[0])
node_struct = target.get_editor_property("node")
node_struct.set_editor_property("control_rig_class", rig_class)
target.set_editor_property("node", node_struct)

if not unreal.EditorAssetLibrary.save_loaded_asset(anim_blueprint):
    raise RuntimeError("Failed to save Animation Blueprint: " + anim_path)

payload = {
    "anim_blueprint": anim_path,
    "control_rig": rig_path,
    "control_rig_class": str(rig_class),
    "node": target.get_name(),
    "assigned": str(node_struct.get_editor_property("control_rig_class")),
}
unreal.log("CM_LEG_CONTROLRIG_NODE_ASSIGNED " + json.dumps(payload, ensure_ascii=True))
with open(r"C:/Chimera/Saved/LegControlRigNodeAssigned.json", "w", encoding="utf-8") as output:
    json.dump(payload, output, indent=2, ensure_ascii=True)
