#include "Sound/CMSoundPlayback.h"

#include "GameFramework/Actor.h"
#include "Sound/NKMSoundHelper.h"
#include "Sound/NKMSoundSubsystem.h"

void FCMSoundPlayback::PlaySFXAtActor(AActor* SourceActor, FGameplayTag SoundTag)
{
    if (!IsValid(SourceActor) || !SoundTag.IsValid()
        || SourceActor->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    if (UNKMSoundSubsystem* Sound = FNKMSoundHelper::Get(SourceActor))
    {
        Sound->PlaySFXForActor(
            SoundTag,
            SourceActor->GetActorLocation(),
            SourceActor);
    }
}
