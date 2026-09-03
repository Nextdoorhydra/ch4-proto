import json
import unreal


asset = unreal.load_asset(
    "/Game/Chimera/Character/Part/Leg/CR_CMLegLProcedural")
hierarchy = asset.hierarchy


def transform_payload(transform):
    return {
        "translation": str(transform.translation),
        "rotation": str(transform.rotation),
        "scale": str(transform.scale3d),
    }


payload = {}
for key in hierarchy.get_controls():
    name = str(key.name)
    if not name.startswith("CTRL_CM_Leg"):
        continue
    entry = {"key": str(key), "parent": str(hierarchy.get_first_parent(key))}
    for space_name, getter in (
        ("current_global", hierarchy.get_global_transform),
        ("current_local", hierarchy.get_local_transform),
    ):
        try:
            entry[space_name] = transform_payload(getter(key, False))
        except TypeError:
            entry[space_name] = transform_payload(getter(key))
    for space_name, getter in (
        ("initial_global", hierarchy.get_global_transform),
        ("initial_local", hierarchy.get_local_transform),
    ):
        try:
            entry[space_name] = transform_payload(getter(key, True))
        except TypeError as exc:
            entry[space_name + "_error"] = repr(exc)
    payload[name] = entry

with open(
    r"C:/Chimera/Saved/LegRigHierarchy.json", "w", encoding="utf-8") as output:
    json.dump(payload, output, indent=2, ensure_ascii=True)
