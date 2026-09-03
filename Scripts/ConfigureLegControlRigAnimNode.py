import unreal

editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor.get_editor_world() if editor else None
if not world:
    raise RuntimeError('Editor world is not available')

unreal.SystemLibrary.execute_console_command(
    world,
    'Chimera.ConfigureLegControlRigAnimNode /Game/Chimera/ABP_CMLegLProcedural')
print('Control Rig AnimGraph mapping command dispatched')
