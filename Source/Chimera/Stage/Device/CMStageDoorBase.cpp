#include "Stage/Device/CMStageDoorBase.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/CMSoundPlayback.h"
#include "Sound/CMSoundTags.h"

ACMStageDoorBase::ACMStageDoorBase()
{
    bStartActive = false;
    MovementSoundTag = CMSoundTags::Stage_Door_Move;
}

// 문 표현과 블로킹 충돌이 목표 상태에 도달했음을 대기 중인 룸 트리거에 전달
void ACMStageDoorBase::NotifyDoorTransitionFinished(bool bIsOpen)
{
    OnDoorTransitionFinished.Broadcast(bIsOpen);
}

void ACMStageDoorBase::TestToggleDoor()
{
    if (const UWorld* World = GetWorld();
        World && World->IsGameWorld() && HasAuthority())
    {
        ToggleElement();
    }
}

// Mechanism 활성 상태를 문의 열림 상태로 전달
void ACMStageDoorBase::HandleElementActiveChanged_Implementation(bool bIsActive)
{
    Super::HandleElementActiveChanged_Implementation(bIsActive);

    const bool bStateChanged = bHasObservedOpenState
        && bLastObservedOpenState != bIsActive;
    bLastObservedOpenState = bIsActive;
    bHasObservedOpenState = true;

    if (bStateChanged)
    {
        FCMSoundPlayback::PlaySFXAtActor(this, MovementSoundTag);
        if (bIsActive && OpeningCameraShake)
        {
            UGameplayStatics::PlayWorldCameraShake(
                this,
                OpeningCameraShake,
                GetActorLocation(),
                CameraShakeInnerRadius,
                FMath::Max(CameraShakeOuterRadius, CameraShakeInnerRadius));
        }
    }

    OnDoorOpenStateChanged(bIsActive);
}

// 문 전용 초기화 표현을 하위 구현에 전달
void ACMStageDoorBase::HandleElementReset_Implementation()
{
    Super::HandleElementReset_Implementation();
    OnDoorReset();
}
