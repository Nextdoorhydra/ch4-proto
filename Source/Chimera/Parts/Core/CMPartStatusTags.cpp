#include "Parts/Core/CMPartStatusTags.h"

namespace CMPartStatusTags
{
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Electrified,
        "Chimera.State.Part.Electrified",
        "일정 시간 해당 파츠 행동 차단");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Slowed,
        "Chimera.State.Part.Slowed",
        "해당 파츠의 이동 Impulse 배율 감소");
}
