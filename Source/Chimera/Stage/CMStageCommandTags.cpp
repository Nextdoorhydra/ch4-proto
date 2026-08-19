#include "Stage/CMStageCommandTags.h"

namespace CMStageCommandTags
{
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Effect_Activate,
        "Chimera.Stage.Command.Effect.Activate",
        "등록된 지속형 나이아가라 이펙트 활성화");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Effect_Deactivate,
        "Chimera.Stage.Command.Effect.Deactivate",
        "등록된 나이아가라 이펙트 비활성화");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Effect_Restart,
        "Chimera.Stage.Command.Effect.Restart",
        "등록된 나이아가라 이펙트 초기화 후 다시 실행");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Effect_Burst,
        "Chimera.Stage.Command.Effect.Burst",
        "등록된 일회성 나이아가라 이펙트 처음부터 실행");
}
