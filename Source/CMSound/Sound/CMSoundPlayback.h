#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class AActor;

// 게임 코드는 이 창구로 프로젝트 사운드 태그의 로컬 재생만 요청한다.
class CMSOUND_API FCMSoundPlayback
{
public:
    static void PlaySFXAtActor(AActor* SourceActor, FGameplayTag SoundTag);
};
