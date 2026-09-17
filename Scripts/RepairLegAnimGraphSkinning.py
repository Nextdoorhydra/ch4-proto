import unreal


ANIM_BLUEPRINT_PATH = "/Game/Chimera/ABP_CMLegLProcedural"

editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor.get_editor_world() if editor else None
if not world:
    raise RuntimeError("Editor world is unavailable")

unreal.SystemLibrary.execute_console_command(
    world,
    f"Chimera.RepairLegAnimGraphSkinning {ANIM_BLUEPRINT_PATH}",
)

if not unreal.EditorAssetLibrary.save_asset(ANIM_BLUEPRINT_PATH, only_if_is_dirty=False):
    raise RuntimeError(f"Failed to save {ANIM_BLUEPRINT_PATH}")

unreal.log(f"[Chimera] Saved repaired leg AnimBlueprint: {ANIM_BLUEPRINT_PATH}")
