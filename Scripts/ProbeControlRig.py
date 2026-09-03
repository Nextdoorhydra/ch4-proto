import json
import unreal


symbols = [
    name
    for name in dir(unreal)
    if "ControlRig" in name or "RigVM" in name or "RigUnit" in name
]
payload = {
    "control_rig_symbols": symbols,
    "has_control_rig_blueprint_factory": hasattr(unreal, "ControlRigBlueprintFactory"),
    "has_control_rig_blueprint_editor_library": hasattr(
        unreal, "ControlRigBlueprintEditorLibrary"
    ),
}
unreal.log("CM_CONTROL_RIG_PYTHON_PROBE " + json.dumps(payload, ensure_ascii=True))
with open(r"C:/Chimera/Saved/ControlRigPythonProbe.json", "w", encoding="utf-8") as output:
    json.dump(payload, output, indent=2, ensure_ascii=True)
