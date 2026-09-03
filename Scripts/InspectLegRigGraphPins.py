import json
import unreal


asset = unreal.load_asset(
    "/Game/Chimera/Character/Part/Leg/CR_CMLegLProcedural")
model = asset.get_default_model()
payload = {
    "links": [
        {
            "source": str(link.get_source_pin().get_pin_path()),
            "target": str(link.get_target_pin().get_pin_path()),
        }
        for link in model.get_links()
    ],
    "nodes": [],
}
for node in model.get_nodes():
    payload["nodes"].append({
        "name": str(node.get_name()),
        "pins": [
            {
                "path": str(pin.get_pin_path()),
                "default": str(pin.get_default_value()),
            }
            for pin in node.get_pins()
        ],
    })
with open(
    r"C:/Chimera/Saved/LegRigGraphPins.json",
    "w",
    encoding="utf-8",
) as output:
    json.dump(payload, output, indent=2, ensure_ascii=True)
