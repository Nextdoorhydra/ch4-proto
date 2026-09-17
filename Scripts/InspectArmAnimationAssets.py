import json
import unreal


ANIM_PATH = "/Game/Chimera/ABP_CMArmLProcedural"
MESH_PATH = "/Game/CoreC/02BaseBody/SKM/male_Arm_L"
OUTPUT_PATH = r"C:/Chimera/Saved/ArmAnimationAssetsInspect.json"


def rig_key(name):
    key = unreal.RigElementKey()
    key.set_editor_property("name", name)
    key.set_editor_property("type", unreal.RigElementType.BONE)
    return key


payload = {}
anim_blueprint = unreal.load_asset(ANIM_PATH)
mesh = unreal.load_asset(MESH_PATH)
payload["anim_blueprint"] = str(anim_blueprint)
payload["skeletal_mesh"] = str(mesh)
if anim_blueprint:
    graphs = []
    for graph in anim_blueprint.get_animation_graphs():
        graph_data = {"name": graph.get_name(), "nodes": []}
        try:
            nodes = graph.get_graph_nodes_of_class(unreal.AnimGraphNode_Base)
        except Exception:
            nodes = []
        for node in nodes:
            node_data = {
                "name": node.get_name(),
                "class": node.get_class().get_name(),
            }
            if isinstance(node, unreal.AnimGraphNode_ControlRig):
                node_struct = node.get_editor_property("node")
                node_data["control_rig_class"] = str(
                    node_struct.get_editor_property("control_rig_class"))
            graph_data["nodes"].append(node_data)
        graphs.append(graph_data)
    payload["graphs"] = graphs

if mesh:
    skeleton = mesh.get_editor_property("skeleton")
    payload["skeleton"] = str(skeleton)

with open(OUTPUT_PATH, "w", encoding="utf-8") as output:
    json.dump(payload, output, indent=2, ensure_ascii=True)
unreal.log("CM_ARM_ANIMATION_INSPECT " + json.dumps(payload, ensure_ascii=True))
