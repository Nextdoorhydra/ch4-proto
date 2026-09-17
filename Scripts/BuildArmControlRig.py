import json
import unreal


RIG_PATH = "/Game/Chimera/Character/Part/Arm/CR_CMArmLProcedural"
MESH_PATH = "/Game/CoreC/02BaseBody/SKM/male_Arm_L"
OUTPUT_PATH = r"C:/Chimera/Saved/ArmControlRigBuild.json"


def rig_key(name, element_type):
    key = unreal.RigElementKey()
    key.set_editor_property("name", name)
    key.set_editor_property("type", element_type)
    return key


asset = unreal.load_asset(RIG_PATH)
created_asset = False
if not asset:
    mesh = unreal.load_asset(MESH_PATH)
    if not mesh:
        raise RuntimeError("Arm skeletal mesh was not found: " + MESH_PATH)
    asset = unreal.ControlRigBlueprintFactory.create_control_rig_from_skeletal_mesh_or_skeleton(
        mesh,
        False,
    )
    if not asset:
        raise RuntimeError("Failed to create arm Control Rig from: " + MESH_PATH)
    source_path = asset.get_path_name().split(".")[0]
    if source_path != RIG_PATH:
        if not unreal.EditorAssetLibrary.rename_asset(source_path, RIG_PATH):
            raise RuntimeError(
                "Failed to move Control Rig from {} to {}".format(
                    source_path,
                    RIG_PATH,
                )
            )
        asset = unreal.load_asset(RIG_PATH)
    created_asset = True

if not asset:
    raise RuntimeError("Arm Control Rig could not be loaded: " + RIG_PATH)

hierarchy = asset.hierarchy
hierarchy_controller = asset.get_hierarchy_controller()
existing_controls = {str(key.name) for key in hierarchy.get_controls()}


def bone_transform(name):
    return hierarchy.get_global_transform(rig_key(name, unreal.RigElementType.BONE))


def add_control(name, control_type, value, shape_name):
    key = rig_key(name, unreal.RigElementType.CONTROL)
    if name in existing_controls:
        return key, False
    settings = unreal.RigControlSettings()
    settings.set_editor_property("control_type", control_type)
    settings.set_editor_property(
        "animation_type",
        unreal.RigControlAnimationType.ANIMATION_CONTROL,
    )
    settings.set_editor_property("shape_visible", True)
    settings.set_editor_property("shape_name", shape_name)
    result = hierarchy_controller.add_control(
        name,
        unreal.RigElementKey(),
        settings,
        value,
        False,
        False,
    )
    if not result:
        raise RuntimeError("Failed to create arm control: " + name)
    existing_controls.add(name)
    return result, True


elbow_transform = bone_transform("lowerarm_l")
hand_transform = bone_transform("hand_l")
elbow_pole = elbow_transform.translation + unreal.Vector(45.0, 0.0, 0.0)
controls = []
for name, control_type, value, shape_name in (
    (
        "CTRL_CM_ArmHandIK",
        unreal.RigControlType.EULER_TRANSFORM,
        hierarchy.make_control_value_from_transform(hand_transform),
        "Box_Thick",
    ),
    (
        "CTRL_CM_ArmElbowPole",
        unreal.RigControlType.POSITION,
        hierarchy.make_control_value_from_vector(elbow_pole),
        "Sphere_Thick",
    ),
    (
        "CTRL_CM_ArmIKAlpha",
        unreal.RigControlType.FLOAT,
        hierarchy.make_control_value_from_float(1.0),
        "Circle_Thick",
    ),
):
    key, was_created = add_control(name, control_type, value, shape_name)
    controls.append({"name": name, "created": was_created, "key": str(key)})

controller = asset.get_or_create_controller()
model = asset.get_default_model()
existing_nodes = {node.get_name(): node for node in model.get_nodes()}


def add_node(name, script_struct, x, y):
    if name in existing_nodes:
        return existing_nodes[name], False
    node = controller.add_unit_node(
        script_struct.static_struct(),
        "Execute",
        unreal.Vector2D(float(x), float(y)),
        name,
        False,
        False,
    )
    if not node:
        raise RuntimeError("Failed to add arm RigVM node: " + name)
    existing_nodes[name] = node
    return node, True


nodes = {}
created_nodes = []
for name, script_struct, x, y in (
    ("ForwardsSolve", unreal.RigUnit_BeginExecution, 0, 0),
    ("GetArmHandIK", unreal.RigUnit_GetControlTransform, 260, -120),
    ("GetArmElbowPole", unreal.RigUnit_GetControlVector, 260, 100),
    ("GetArmIKAlpha", unreal.RigUnit_GetControlFloat, 260, 320),
    ("ArmTwoBoneIK", unreal.RigUnit_TwoBoneIKSimple, 760, 80),
):
    node, was_created = add_node(name, script_struct, x, y)
    nodes[name] = node
    if was_created:
        created_nodes.append(name)


def set_default(pin_path, value):
    if not controller.set_pin_default_value(
        pin_path,
        value,
        True,
        False,
        False,
        False,
        True,
    ):
        raise RuntimeError("Failed to set arm RigVM pin: " + pin_path)


defaults = {
    "GetArmHandIK.Control": '(Type=Control,Name="CTRL_CM_ArmHandIK")',
    "GetArmHandIK.Space": "GlobalSpace",
    "GetArmElbowPole.Control": '(Type=Control,Name="CTRL_CM_ArmElbowPole")',
    "GetArmElbowPole.Space": "GlobalSpace",
    "GetArmIKAlpha.Control": '(Type=Control,Name="CTRL_CM_ArmIKAlpha")',
    "ArmTwoBoneIK.BoneA": '(Type=Bone,Name="upperarm_l")',
    "ArmTwoBoneIK.BoneB": '(Type=Bone,Name="lowerarm_l")',
    "ArmTwoBoneIK.EffectorBone": '(Type=Bone,Name="hand_l")',
    "ArmTwoBoneIK.PoleVectorKind": "Location",
    "ArmTwoBoneIK.PoleVectorSpace": "None",
    "ArmTwoBoneIK.bEnableStretch": "False",
    "ArmTwoBoneIK.bPropagateToChildren": "True",
}
for path, value in defaults.items():
    set_default(path, value)

links = (
    ("ForwardsSolve.ExecutePin", "ArmTwoBoneIK.ExecutePin"),
    ("GetArmHandIK.Transform", "ArmTwoBoneIK.Effector"),
    ("GetArmElbowPole.Vector", "ArmTwoBoneIK.PoleVector"),
    ("GetArmIKAlpha.FloatValue", "ArmTwoBoneIK.Weight"),
)
link_results = []
for source, target in links:
    linked = controller.add_link(source, target, False, False)
    link_results.append({"source": source, "target": target, "linked": linked})

if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
    raise RuntimeError("Failed to save arm Control Rig: " + RIG_PATH)

payload = {
    "asset": RIG_PATH,
    "created_asset": created_asset,
    "controls": controls,
    "created_nodes": created_nodes,
    "defaults": defaults,
    "links": link_results,
}
with open(OUTPUT_PATH, "w", encoding="utf-8") as output:
    json.dump(payload, output, indent=2, ensure_ascii=True)
unreal.log("CM_ARM_CONTROL_RIG_BUILT " + json.dumps(payload, ensure_ascii=True))
