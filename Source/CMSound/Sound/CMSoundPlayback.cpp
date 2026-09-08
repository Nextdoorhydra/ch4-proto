#include "Sound/CMSoundPlayback.h"

#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Sound/NKMSoundHelper.h"
#include "Sound/NKMSoundSubsystem.h"

void FCMSoundPlayback::PlaySFXAtActor(AActor* SourceActor, FGameplayTag SoundTag)
{
    PlaySFXAtLocation(
        SourceActor,
        IsValid(SourceActor) ? SourceActor->GetActorLocation() : FVector::ZeroVector,
        SoundTag);
}

void FCMSoundPlayback::PlaySFXAtLocation(
    AActor* SourceActor,
    const FVector Location,
    FGameplayTag SoundTag)
{
    if (!IsValid(SourceActor) || !SoundTag.IsValid()
        || Location.ContainsNaN()
        || SourceActor->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    if (UNKMSoundSubsystem* Sound = FNKMSoundHelper::Get(SourceActor))
    {
        Sound->PlaySFXForActor(
            SoundTag,
            Location,
            SourceActor);
    }
}

UAudioComponent* FCMSoundPlayback::PlayAttachedSFX(
    USceneComponent* AttachToComponent,
    FGameplayTag SoundTag)
{
    const AActor* SourceActor = IsValid(AttachToComponent)
        ? AttachToComponent->GetOwner()
        : nullptr;
    if (!IsValid(SourceActor) || !SoundTag.IsValid()
        || SourceActor->GetNetMode() == NM_DedicatedServer)
    {
        return nullptr;
    }

    if (UNKMSoundSubsystem* Sound = FNKMSoundHelper::Get(AttachToComponent))
    {
        return Sound->PlayAttachedSFX(SoundTag, AttachToComponent);
    }
    return nullptr;
}
