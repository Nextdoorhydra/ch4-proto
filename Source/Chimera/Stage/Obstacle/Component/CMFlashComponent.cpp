#include "Stage/Obstacle/Component/CMFlashComponent.h"

#include "EngineUtils.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Parts/Head/CMVisionComponent.h"
#include "Player/CMChimera.h"
#include "Player/CMPartSlotComponent.h"
#include "TimerManager.h"

UCMFlashComponent::UCMFlashComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UCMFlashComponent::TriggerFlash()
{
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!Owner || !Owner->HasAuthority() || !World || !bFlashEnabled
        || bFlashPending)
    {
        return;
    }

    const float WarningDuration = FMath::Max(BlindActivationDelay, 0.0f);
    if (WarningDuration <= UE_KINDA_SMALL_NUMBER)
    {
        DetonateFlash();
        return;
    }

    bFlashPending = true;
    World->GetTimerManager().SetTimer(
        DetonationTimerHandle,
        this,
        &ThisClass::DetonateFlash,
        WarningDuration,
        false);
}

void UCMFlashComponent::SetFlashEnabled(bool bEnabled)
{
    bFlashEnabled = bEnabled;
    if (!bFlashEnabled && GetWorld())
    {
        bFlashPending = false;
        GetWorld()->GetTimerManager().ClearTimer(DetonationTimerHandle);
    }
}

void UCMFlashComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(DetonationTimerHandle);
    }
    Super::EndPlay(EndPlayReason);
}

void UCMFlashComponent::DetonateFlash()
{
    bFlashPending = false;
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!Owner || !Owner->HasAuthority() || !World || !bFlashEnabled)
    {
        return;
    }

    const FVector FlashLocation = Owner->GetActorLocation();
    const float RangeSquared = FMath::Square(FMath::Max(FlashRange, 0.0f));
    for (TActorIterator<ACMChimera> It(World); It; ++It)
    {
        ACMChimera* Chimera = *It;
        TArray<UCMVisionComponent*> ActiveVisionSources;
        Chimera->GetActiveHeadVisionSources(ActiveVisionSources);

        int32 UnblindedSourceCount = 0;
        TArray<UCMVisionComponent*> ExposedSources;
        for (UCMVisionComponent* VisionSource : ActiveVisionSources)
        {
            if (!VisionSource || VisionSource->IsBlinded())
            {
                continue;
            }
            ++UnblindedSourceCount;
            if (FVector::DistSquared(
                    VisionSource->GetVisionOrigin(), FlashLocation)
                    <= RangeSquared
                && VisionSource->IsLocationInsideVisionCone(FlashLocation)
                && HasLineOfSight(*VisionSource))
            {
                ExposedSources.Add(VisionSource);
            }
        }

        if (ExposedSources.Num() >= UnblindedSourceCount
            && UnblindedSourceCount > 0)
        {
            ExposedSources.RemoveAtSwap(
                FMath::RandHelper(ExposedSources.Num()));
        }
        for (UCMVisionComponent* VisionSource : ExposedSources)
        {
            VisionSource->ApplyBlindness(
                BlindDuration,
                0.0f,
                BlindRecoveryDuration);
        }
    }
}

bool UCMFlashComponent::HasLineOfSight(
    const UCMVisionComponent& VisionSource) const
{
    if (!bRequireLineOfSight || !GetWorld() || !GetOwner())
    {
        return true;
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CMFlash), false);
    QueryParams.AddIgnoredActor(GetOwner());
    QueryParams.AddIgnoredActor(VisionSource.GetOwner());
    const ACMPartActorBase* HeadPart = Cast<ACMPartActorBase>(
        VisionSource.GetOwner());
    if (const UCMPartSlotComponent* PartSlot = HeadPart
        ? HeadPart->GetAttachedPartSlot() : nullptr)
    {
        QueryParams.AddIgnoredActor(PartSlot->GetOwner());
    }

    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(
        Hit,
        VisionSource.GetVisionOrigin(),
        GetOwner()->GetActorLocation(),
        LineOfSightTraceChannel,
        QueryParams);
}
