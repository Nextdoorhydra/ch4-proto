import json
import unreal


asset_path = "/Game/Chimera/ABP_CMLegLProcedural"
asset = unreal.load_asset(asset_path)
if not asset:
    raise RuntimeError("Animation Blueprint was not found: " + asset_path)

payload = {
    "asset_class": asset.get_class().get_name(),
    "asset_dir": [name for name in dir(asset) if "graph" in name.lower() or "node" in name.lower() or "blueprint" in name.lower()],
}
payload["unreal_load_symbols"] = [name for name in dir(unreal) if "blueprint" in name.lower() and ("class" in name.lower() or "generated" in name.lower()) or "controlrig" in name.lower()]
try:
    cr_asset = unreal.load_asset("/Game/Chimera/Character/Part/Leg/CR_CMLegLProcedural")
    payload["cr_asset"] = str(cr_asset)
    payload["cr_asset_dir"] = [name for name in dir(cr_asset) if "class" in name.lower() or "generated" in name.lower() or "blueprint" in name.lower()]
    try:
        payload["cr_get_control_rig_class"] = str(cr_asset.get_control_rig_class())
    except Exception as exc:
        payload["cr_get_control_rig_class_error"] = repr(exc)
    for prop in ("generated_class", "parent_class", "blueprint_generated_class"):
        try:
            payload["cr_asset_" + prop] = str(cr_asset.get_editor_property(prop))
        except Exception:
            pass
except Exception as exc:
    payload["cr_asset_error"] = repr(exc)
for prop in ("generated_class", "generated_class_path", "parent_class"):
    try:
        payload[prop] = str(asset.get_editor_property(prop))
    except Exception:
        pass
for path in ("/Game/Chimera/Character/Part/Leg/CR_CMLegLProcedural_C", "/Game/Chimera/Character/Part/Leg/CR_CMLegLProcedural"):
    try:
        payload["load_class_" + path.rsplit("/", 1)[-1]] = str(unreal.load_class(None, path))
    except Exception as exc:
        payload["load_class_error_" + path.rsplit("/", 1)[-1]] = repr(exc)

try:
    graphs = asset.get_animation_graphs()
    payload["graphs"] = []
    for graph in graphs:
        graph_info = {
            "name": graph.get_name(),
            "class": graph.get_class().get_name(),
            "dir": [name for name in dir(graph) if "node" in name.lower() or "schema" in name.lower() or "graph" in name.lower()],
        }
        try:
            nodes = graph.get_graph_nodes_of_class(unreal.AnimGraphNode_ControlRig)
            graph_info["nodes"] = []
            for node in nodes:
                node_info = {
                    "name": node.get_name(),
                    "class": node.get_class().get_name(),
                    "dir": [name for name in dir(node) if "rig" in name.lower() or "control" in name.lower() or "property" in name.lower()],
                }
                try:
                    node_struct = node.get_editor_property("node")
                    node_info["node_struct_class"] = str(node_struct)
                    node_info["node_struct_dir"] = [name for name in dir(node_struct) if "rig" in name.lower() or "class" in name.lower() or "control" in name.lower() or "property" in name.lower()]
                    for prop in ("control_rig_class", "control_rig", "class", "control_rig_asset", "sequencer_control_rig"):
                        try:
                            node_info["node_struct_" + prop] = str(node_struct.get_editor_property(prop))
                        except Exception:
                            pass
                except Exception as exc:
                    node_info["node_struct_error"] = repr(exc)
                for prop in ("control_rig_class", "control_rig", "class", "node", "node_instance"):
                    try:
                        node_info[prop] = str(node.get_editor_property(prop))
                    except Exception:
                        pass
                graph_info["nodes"].append(node_info)
        except Exception as exc:
            graph_info["nodes_error"] = repr(exc)
        payload["graphs"].append(graph_info)
except Exception as exc:
    payload["graphs_error"] = repr(exc)

unreal.log("CM_ANIMGRAPH_CONTROLRIG_INSPECT " + json.dumps(payload, ensure_ascii=True))
with open(r"C:/Chimera/Saved/AnimGraphControlRigInspect.json", "w", encoding="utf-8") as output:
    json.dump(payload, output, indent=2, ensure_ascii=True)
