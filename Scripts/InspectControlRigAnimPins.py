import unreal
import json

ABP_PATH = '/Game/Chimera/ABP_CMLegLProcedural'
abp = unreal.load_asset(ABP_PATH)
if not abp:
    raise RuntimeError('Unable to load AnimBlueprint: ' + ABP_PATH)

graphs = abp.get_animation_graphs()
node = None
for graph in graphs:
    for candidate in graph.get_graph_nodes_of_class(unreal.AnimGraphNode_ControlRig):
        node = candidate
        break
    if node:
        break
if not node:
    raise RuntimeError('AnimGraphNode_ControlRig not found')

payload = {
    'node': node.get_name(),
    'node_dir': [name for name in dir(node) if 'pin' in name.lower() or 'reconstruct' in name.lower()],
}

try:
    custom_pins = node.get_editor_property('custom_pin_properties')
except Exception as exc:
    payload['custom_pin_error'] = repr(exc)
    custom_pins = []

payload['custom_pin_count'] = len(custom_pins)
payload['custom_pins'] = []
for pin in custom_pins:
    data = {'repr': str(pin), 'dir': [name for name in dir(pin) if not name.startswith('_')]}
    for name in ('property_name', 'property_friendly_name', 'b_show_pin', 'show_pin', 'b_can_toggle_visibility', 'b_property_is_customized'):
        try:
            data[name] = str(pin.get_editor_property(name))
        except Exception:
            pass
    payload['custom_pins'].append(data)

try:
    struct = node.get_editor_property('node')
    payload['struct_dir'] = [name for name in dir(struct) if 'mapping' in name.lower() or 'property' in name.lower()]
    for name in ('input_mapping', 'output_mapping', 'control_rig_class'):
        try:
            value = struct.get_editor_property(name)
            payload[name] = str(value)
        except Exception as exc:
            payload[name + '_error'] = repr(exc)
except Exception as exc:
    payload['struct_error'] = repr(exc)

with open('C:/Chimera/Saved/ControlRigAnimPins.json', 'w', encoding='utf-8') as handle:
    json.dump(payload, handle, indent=2, ensure_ascii=False)
print(json.dumps(payload, indent=2, ensure_ascii=False))
