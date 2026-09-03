import json
import unreal


asset_path = "/Game/Chimera/Character/Part/Leg/CR_CMLegLProcedural"
asset = unreal.load_asset(asset_path)
if not asset:
    raise RuntimeError("Control Rig asset was not found: " + asset_path)

controller = asset.get_or_create_controller()
model = asset.get_default_model()
node_names = {node.get_name() for node in model.get_nodes()}
required_nodes = {
    "ForwardsSolve",
    "GetLegHip",
    "SetLegHip",
    "GetLegFootIK",
    "GetLegKneePole",
    "GetLegPlantAlpha",
    "LegTwoBoneIK",
}
missing = sorted(required_nodes - node_names)
if missing:
    raise RuntimeError("Control Rig nodes are missing: " + ", ".join(missing))


def set_default(pin_path, value):
    if not controller.set_pin_default_value(pin_path, value, True, False, False, False, True):
        raise RuntimeError("Failed to set RigVM pin default: " + pin_path)


defaults = {
    "GetLegHip.Control": '(Type=Control,Name="CTRL_CM_LegHip")',
    "GetLegHip.Space": "GlobalSpace",
    "GetLegFootIK.Control": '(Type=Control,Name="CTRL_CM_LegFootIK")',
    "GetLegFootIK.Space": "GlobalSpace",
    "GetLegKneePole.Control": '(Type=Control,Name="CTRL_CM_LegKneePole")',
    "GetLegKneePole.Space": "GlobalSpace",
    "GetLegPlantAlpha.Control": '(Type=Control,Name="CTRL_CM_LegPlantAlpha")',
    "SetLegHip.Bone": '(Type=Bone,Name="thigh_l")',
    "SetLegHip.Space": "GlobalSpace",
    "SetLegHip.Weight": "1.000000",
    "SetLegHip.bPropagateToChildren": "True",
    "LegTwoBoneIK.BoneA": '(Type=Bone,Name="thigh_l")',
    "LegTwoBoneIK.BoneB": '(Type=Bone,Name="calf_l")',
    "LegTwoBoneIK.EffectorBone": '(Type=Bone,Name="foot_l")',
    "LegTwoBoneIK.PoleVectorKind": "Location",
    "LegTwoBoneIK.PoleVectorSpace": "None",
    "LegTwoBoneIK.bEnableStretch": "False",
    "LegTwoBoneIK.bPropagateToChildren": "True",
}
for path, value in defaults.items():
    set_default(path, value)


links = [
    ("ForwardsSolve.ExecutePin", "SetLegHip.ExecutePin"),
    ("SetLegHip.ExecutePin", "LegTwoBoneIK.ExecutePin"),
    ("GetLegHip.Transform", "SetLegHip.Transform"),
    ("GetLegFootIK.Transform", "LegTwoBoneIK.Effector"),
    ("GetLegKneePole.Vector", "LegTwoBoneIK.PoleVector"),
    ("GetLegPlantAlpha.FloatValue", "LegTwoBoneIK.Weight"),
]
link_results = []
for source, target in links:
    link_results.append({"source": source, "target": target, "linked": controller.add_link(source, target, False, False)})

if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
    raise RuntimeError("Failed to save Control Rig asset: " + asset_path)

payload = {
    "asset": asset_path,
    "defaults": defaults,
    "links": link_results,
    "nodes": sorted(node_names),
}
unreal.log("CM_LEG_RIG_GRAPH_FINALIZED " + json.dumps(payload, ensure_ascii=True))
with open(r"C:/Chimera/Saved/LegRigGraphFinalized.json", "w", encoding="utf-8") as output:
    json.dump(payload, output, indent=2, ensure_ascii=True)
