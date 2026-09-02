import json
import unreal


hierarchy = unreal.load_asset("/Game/Chimera/Character/Part/Leg/CR_CMLegLProcedural").hierarchy


def key(name):
    result = unreal.RigElementKey()
    result.set_editor_property("name", name)
    result.set_editor_property("type", unreal.RigElementType.BONE)
    return result


payload = {}
for name in ("pelvis", "thigh_l", "calf_l", "foot_l", "ball_l", "ik_foot_l"):
    transform = hierarchy.get_global_transform(key(name))
    payload[name] = {
        "translation": str(transform.translation),
        "rotation": str(transform.rotation),
        "scale": str(transform.scale3d),
    }
unreal.log("CM_BONE_TRANSFORMS " + json.dumps(payload, ensure_ascii=True))
with open(r"C:/Chimera/Saved/BoneTransforms.json", "w", encoding="utf-8") as output:
    json.dump(payload, output, indent=2, ensure_ascii=True)
