import json
import unreal


asset = unreal.load_asset("/Game/Chimera/Character/Part/Leg/CR_CMLegLProcedural")
hierarchy_controller = asset.get_hierarchy_controller()
rig_controller = asset.get_controller()
model = asset.get_default_model()

payload = {
    "bones": [str(key) for key in asset.hierarchy.get_bones()],
    "controls": [str(key) for key in asset.hierarchy.get_controls()],
    "rig_nodes": [node.get_name() for node in model.get_nodes()],
    "rig_node_paths": [str(node.get_name()) for node in model.get_nodes()],
    "graph_name": model.get_graph_name(),
    "event_names": [str(name) for name in model.get_event_names()],
}
payload["get_keys_doc"] = asset.hierarchy.get_keys.__doc__
for owner, name in ((hierarchy_controller, "export_to_text"), (rig_controller, "export_nodes_to_text")):
    payload[name + "_doc"] = getattr(owner, name).__doc__
try:
    payload["hierarchy_text"] = hierarchy_controller.export_to_text()
except Exception as exc:
    payload["hierarchy_text_error"] = repr(exc)
try:
    payload["nodes_text"] = rig_controller.export_nodes_to_text(model.get_nodes())
except Exception as exc:
    payload["nodes_text_error"] = repr(exc)
unreal.log("CM_RIG_STATE " + json.dumps(payload, ensure_ascii=True))
with open(r"C:/Chimera/Saved/RigState.json", "w", encoding="utf-8") as output:
    json.dump(payload, output, indent=2, ensure_ascii=True)
