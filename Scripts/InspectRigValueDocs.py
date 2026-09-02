import json
import unreal


hierarchy = unreal.load_asset("/Game/Chimera/Character/Part/Leg/CR_CMLegLProcedural").hierarchy
payload = {}
for name in (
    "make_control_value_from_transform",
    "make_control_value_from_vector",
    "make_control_value_from_float",
    "find_control",
    "find_bone",
    "set_control_value",
):
    payload[name] = getattr(hierarchy, name).__doc__
payload["RigElementKey_init"] = getattr(unreal.RigElementKey, "__doc__", None)
payload["RigControlSettings_init"] = getattr(unreal.RigControlSettings, "__doc__", None)
payload["RigControlValue_init"] = getattr(unreal.RigControlValue, "__doc__", None)
unreal.log("CM_RIG_VALUE_DOCS " + json.dumps(payload, ensure_ascii=True))
with open(r"C:/Chimera/Saved/RigValueDocs.json", "w", encoding="utf-8") as output:
    json.dump(payload, output, indent=2, ensure_ascii=True)
