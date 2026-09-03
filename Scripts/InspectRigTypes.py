import json
import unreal


def public(value):
    return [name for name in dir(value) if not name.startswith("_")] if value else []


instances = {
    "RigControlSettings": unreal.RigControlSettings(),
    "RigControlValue": unreal.RigControlValue(),
    "RigElementKey": unreal.RigElementKey(),
    "RigUnit_TwoBoneIKSimple": unreal.RigUnit_TwoBoneIKSimple(),
    "RigUnit_GetTransform": unreal.RigUnit_GetTransform(),
    "RigUnit_SetBoneTransform": unreal.RigUnit_SetBoneTransform(),
    "RigUnit_LineTraceByTraceChannel": unreal.RigUnit_LineTraceByTraceChannel(),
    "RigUnit_GetControlTransform": unreal.RigUnit_GetControlTransform(),
    "RigUnit_GetControlVector": unreal.RigUnit_GetControlVector(),
    "RigUnit_GetControlFloat": unreal.RigUnit_GetControlFloat(),
    "Transform": unreal.Transform(),
}
payload = {name: {"methods": public(value)} for name, value in instances.items()}
payload["enums"] = {}
for name in ("RigControlType", "RigControlAnimationType", "RigControlVisibility", "RigControlAxis", "ControlRigVectorKind", "RigVectorKind", "RigControlValueType", "RigElementType", "RigBoneType", "RigVMTransformSpace", "RigVMPinDirection"):
    enum_type = getattr(unreal, name, None)
    if enum_type:
        payload["enums"][name] = public(enum_type)
for name, value in instances.items():
    if value:
        for method in ("get_editor_property", "set_editor_property"):
            if hasattr(value, method):
                payload[name][method + "_doc"] = getattr(value, method).__doc__
unreal.log("CM_RIG_TYPES " + json.dumps(payload, ensure_ascii=True))
with open(r"C:/Chimera/Saved/RigTypes.json", "w", encoding="utf-8") as output:
    json.dump(payload, output, indent=2, ensure_ascii=True)
