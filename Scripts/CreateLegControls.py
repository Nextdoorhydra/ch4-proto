import json
import unreal


asset_path = "/Game/Chimera/Character/Part/Leg/CR_CMLegLProcedural"
asset = unreal.load_asset(asset_path)
if not asset:
    raise RuntimeError("Control Rig asset was not found: " + asset_path)

hierarchy = asset.hierarchy
controller = asset.get_hierarchy_controller()
existing_control_names = {
    str(key.name)
    for key in hierarchy.get_controls()
}


def rig_key(name, element_type):
    key = unreal.RigElementKey()
    key.set_editor_property("name", name)
    key.set_editor_property("type", element_type)
    return key


def bone_transform(name):
    return hierarchy.get_global_transform(rig_key(name, unreal.RigElementType.BONE))


def add_control(name, control_type, value):
    key = rig_key(name, unreal.RigElementType.CONTROL)
    if name in existing_control_names:
        return key, False
    settings = unreal.RigControlSettings()
    settings.set_editor_property("control_type", control_type)
    settings.set_editor_property(
        "animation_type",
        unreal.RigControlAnimationType.ANIMATION_CONTROL,
    )
    settings.set_editor_property("shape_visible", True)
    settings.set_editor_property("shape_name", "Circle_Thick")
    created = controller.add_control(
        name,
        unreal.RigElementKey(),
        settings,
        value,
        False,
        False,
    )
    if not created:
        raise RuntimeError("Failed to add Control Rig control: " + name)
    existing_control_names.add(name)
    return created, True


thigh_transform = bone_transform("thigh_l")
calf_transform = bone_transform("calf_l")
foot_transform = bone_transform("foot_l")
knee_position = calf_transform.translation + unreal.Vector(0.0, -70.0, 0.0)

controls = []
for name, control_type, value in (
    (
        "CTRL_CM_LegHip",
        unreal.RigControlType.EULER_TRANSFORM,
        hierarchy.make_control_value_from_transform(thigh_transform),
    ),
    (
        "CTRL_CM_LegFootIK",
        unreal.RigControlType.EULER_TRANSFORM,
        hierarchy.make_control_value_from_transform(foot_transform),
    ),
    (
        "CTRL_CM_LegKneePole",
        unreal.RigControlType.POSITION,
        hierarchy.make_control_value_from_vector(knee_position),
    ),
    (
        "CTRL_CM_LegPlantAlpha",
        unreal.RigControlType.FLOAT,
        hierarchy.make_control_value_from_float(1.0),
    ),
    (
        "CTRL_CM_LegBodyHeight",
        unreal.RigControlType.FLOAT,
        hierarchy.make_control_value_from_float(0.0),
    ),
):
    key, created = add_control(name, control_type, value)
    controls.append({"name": name, "created": created, "key": str(key)})

if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
    raise RuntimeError("Failed to save Control Rig asset: " + asset_path)

payload = {"asset": asset_path, "controls": controls}
unreal.log("CM_LEG_CONTROLS_CREATED " + json.dumps(payload, ensure_ascii=True))
with open(r"C:/Chimera/Saved/LegControlsCreated.json", "w", encoding="utf-8") as output:
    json.dump(payload, output, indent=2, ensure_ascii=True)
