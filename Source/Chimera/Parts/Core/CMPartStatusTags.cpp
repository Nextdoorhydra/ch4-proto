#include "Parts/Core/CMPartStatusTags.h"

namespace CMPartStatusTags
{
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Electrified,
        "State.Part.Electrified",
        "일정 시간 해당 파츠 행동 차단");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Staggered,
        "State.Part.Staggered",
        "충격으로 일정 시간 해당 파츠 행동 차단");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Slowed,
        "State.Part.Slowed",
        "해당 파츠의 이동 Impulse 배율 감소");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Confused,
        "State.Part.Confused",
        "해당 파츠의 혼란 상태");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Poisoned,
        "State.Part.Poisoned",
        "해당 파츠의 독 상태");
}
