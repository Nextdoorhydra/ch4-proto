#include "Player/CMChimeraTrailComponent.h"

#include "Components/AudioComponent.h"
#include "Components/DecalComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"
#include "Player/CMChimera.h"
#include "Sound/CMSoundPlayback.h"
#include "Sound/CMSoundTags.h"
#include "Engine/Texture.h"
#include "UObject/ConstructorHelpers.h"

CMChimeraTrail::FStampPlan CMChimeraTrail::BuildStampPlan(
    const float InCarriedDistance,
    const float MovementDistance,
    const float StampSpacing,
    const int32 MaxStamps)
{
    FStampPlan Plan;
    if (MovementDistance <= 0.0f
        || StampSpacing <= 0.0f
        || MaxStamps <= 0)
    {
        Plan.CarriedDistance = FMath::Max(0.0f, InCarriedDistance);
        return Plan;
    }

    const float Carried = FMath::Fmod(
        FMath::Max(0.0f, InCarriedDistance),
        StampSpacing);
    const float TotalDistance = Carried + MovementDistance;
    const int32 RequestedStampCount = FMath::FloorToInt(
        TotalDistance / StampSpacing);

    Plan.StampCount = FMath::Min(RequestedStampCount, MaxStamps);
    Plan.CarriedDistance = FMath::Fmod(TotalDistance, StampSpacing);
    if (Plan.StampCount > 0)
    {
        const int32 SkippedStampCount =
            RequestedStampCount - Plan.StampCount;
        Plan.FirstStampDistance = StampSpacing - Carried
            + SkippedStampCount * StampSpacing;
    }
    return Plan;
}

bool CMChimeraTrail::IsTeleport(
    const float MovementDistance,
    const float TeleportDistance)
{
    return TeleportDistance > 0.0f
        && MovementDistance > TeleportDistance;
}

FQuat CMChimeraTrail::BuildDecalRotation(
    const FVector& SurfaceNormal,
    const FVector& TangentDirection)
{
    const FVector SafeNormal = SurfaceNormal.GetSafeNormal(
        SMALL_NUMBER,
        FVector::UpVector);
    FVector SafeTangent = FVector::VectorPlaneProject(
        TangentDirection,
        SafeNormal).GetSafeNormal();
    if (SafeTangent.IsNearlyZero())
    {
        SafeTangent = FVector::CrossProduct(
            SafeNormal,
            FVector::RightVector).GetSafeNormal();
    }
    if (SafeTangent.IsNearlyZero())
    {
        SafeTangent = FVector::ForwardVector;
    }

    return FRotationMatrix::MakeFromXZ(
        SafeNormal,
        SafeTangent).ToQuat();
}

UCMChimeraTrailComponent::UCMChimeraTrailComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    SetIsReplicatedByDefault(false);

    static ConstructorHelpers::FObjectFinder<UMaterialInterface>
        DefaultTrailMaterial(
            TEXT("/Game/Chimera/Character/Chimera/Materials/"
                "M_CMChimeraTrailDecal.M_CMChimeraTrailDecal"));
    if (DefaultTrailMaterial.Succeeded())
    {
        TrailMaterial = DefaultTrailMaterial.Object;
    }

    static ConstructorHelpers::FObjectFinder<UTexture> DefaultBrushTexture(
        TEXT("/Game/TPBDMat/Textures/"
            "T_splat0_wall_v2.T_splat0_wall_v2"));
    if (DefaultBrushTexture.Succeeded())
    {
        BrushTexture = DefaultBrushTexture.Object;
    }
}

void UCMChimeraTrailComponent::BeginPlay()
{
    Super::BeginPlay();

    const UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_DedicatedServer)
    {
        SetComponentTickEnabled(false);
        return;
    }

    InitializePool();
    ResetMovementSamples();
}

void UCMChimeraTrailComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    StopDragLoopSound();

    for (UDecalComponent* Decal : DecalPool)
    {
        if (IsValid(Decal))
        {
            Decal->DestroyComponent();
        }
    }
    DecalPool.Reset();
    DecalMaterials.Reset();
    ExpirationTimes.Reset();
    SourceStates.Reset();

    Super::EndPlay(EndPlayReason);
}

void UCMChimeraTrailComponent::TickComponent(
    const float DeltaTime,
    const ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UWorld* World = GetWorld();
    AActor* Owner = GetOwner();
    if (!World || !Owner)
    {
        return;
    }

    const float WorldTime = World->GetTimeSeconds();
    UpdateExpiredStamps(WorldTime);

    if (!bEffectActive
        || !IsValid(TrailMaterial)
        || Owner->IsHidden())
    {
        StopDragLoopSound();
        ResetMovementSamples();
        return;
    }

    const int32 SourceCount = GetSourceCount();
    if (SourceStates.Num() != SourceCount)
    {
        ResetMovementSamples();
    }

    int32 RemainingStampBudget = FMath::Max(1, MaxStampsPerFrame);
    bool bAnySourceMoving = false;
    for (int32 SourceIndex = 0;
        SourceIndex < SourceCount;
        ++SourceIndex)
    {
        FVector SourceLocation;
        if (GetSourceLocation(SourceIndex, SourceLocation))
        {
            if (SourceStates.IsValidIndex(SourceIndex)
                && SourceStates[SourceIndex].bHasSample
                && DeltaTime > SMALL_NUMBER)
            {
                FVector PlanarDisplacement =
                    SourceLocation - SourceStates[SourceIndex].PreviousLocation;
                PlanarDisplacement.Z = 0.0f;
                const float MovementDistance = PlanarDisplacement.Size();
                bAnySourceMoving |=
                    MovementDistance > SMALL_NUMBER
                    && !CMChimeraTrail::IsTeleport(MovementDistance, TeleportDistance)
                    && MovementDistance / DeltaTime
                        >= FMath::Max(0.0f, DragSoundMinimumSpeed);
            }
            RemainingStampBudget -= UpdateSourceTrail(
                SourceIndex,
                SourceLocation,
                WorldTime,
                RemainingStampBudget);
        }
    }
    UpdateDragLoopSound(bAnySourceMoving, WorldTime);
}

void UCMChimeraTrailComponent::SetEffectActive(const bool bInActive)
{
    if (bEffectActive == bInActive)
    {
        return;
    }

    bEffectActive = bInActive;
    if (!bEffectActive)
    {
        StopDragLoopSound();
    }
    ResetMovementSamples();
}

void UCMChimeraTrailComponent::UpdateDragLoopSound(
    const bool bAnySourceMoving,
    const float WorldTime)
{
    if (bAnySourceMoving)
    {
        LastDragMovementTime = WorldTime;
        if (!IsValid(DragLoopSoundComponent)
            || !DragLoopSoundComponent->IsPlaying())
        {
            AActor* Owner = GetOwner();
            DragLoopSoundComponent = Owner
                ? FCMSoundPlayback::PlayAttachedSFX(
                    Owner->GetRootComponent(),
                    CMSoundTags::Body_BloodDragLoop)
                : nullptr;
        }
        return;
    }

    if (LastDragMovementTime >= 0.0f
        && WorldTime - LastDragMovementTime
            >= FMath::Max(0.0f, DragSoundStopDelay))
    {
        StopDragLoopSound();
    }
}

void UCMChimeraTrailComponent::StopDragLoopSound()
{
    if (IsValid(DragLoopSoundComponent))
    {
        DragLoopSoundComponent->Stop();
        DragLoopSoundComponent = nullptr;
    }
    LastDragMovementTime = -1.0f;
}

int32 UCMChimeraTrailComponent::GetActiveStampCount() const
{
    int32 ActiveCount = 0;
    for (const UDecalComponent* Decal : DecalPool)
    {
        ActiveCount += IsValid(Decal) && Decal->IsRegistered() ? 1 : 0;
    }
    return ActiveCount;
}

void UCMChimeraTrailComponent::InitializePool()
{
    AActor* Owner = GetOwner();
    if (!Owner || !DecalPool.IsEmpty())
    {
        return;
    }

    const int32 Capacity = FMath::Max(1, PoolCapacity);
    DecalPool.Reserve(Capacity);
    DecalMaterials.Init(nullptr, Capacity);
    ExpirationTimes.Init(-1.0f, Capacity);
    for (int32 Index = 0; Index < Capacity; ++Index)
    {
        UDecalComponent* Decal = NewObject<UDecalComponent>(
            Owner,
            *FString::Printf(TEXT("ChimeraTrailStamp_%d"), Index),
            RF_Transient);
        Owner->AddInstanceComponent(Decal);
        Decal->SetAutoActivate(false);
        Decal->SetHiddenInGame(true);
        Decal->SetVisibility(false);
        Decal->SetCanEverAffectNavigation(false);
        Decal->SetAbsolute(true, true, true);
        DecalPool.Add(Decal);
    }
}

UMaterialInstanceDynamic*
UCMChimeraTrailComponent::GetOrCreateStampMaterial(const int32 StampIndex)
{
    if (!DecalMaterials.IsValidIndex(StampIndex)
        || !IsValid(TrailMaterial))
    {
        return nullptr;
    }

    TObjectPtr<UMaterialInstanceDynamic>& StampMaterial =
        DecalMaterials[StampIndex];
    if (!IsValid(StampMaterial))
    {
        StampMaterial = UMaterialInstanceDynamic::Create(
            TrailMaterial,
            this);
        if (StampMaterial)
        {
            StampMaterial->SetTextureParameterValue(
                TEXT("BrushTexture"), BrushTexture);
        }
    }
    return StampMaterial;
}

void UCMChimeraTrailComponent::DeactivateStamp(const int32 StampIndex)
{
    if (!DecalPool.IsValidIndex(StampIndex))
    {
        return;
    }

    UDecalComponent* Decal = DecalPool[StampIndex];
    if (IsValid(Decal))
    {
        if (Decal->IsRegistered())
        {
            Decal->UnregisterComponent();
        }
        Decal->SetFadeOut(0.0f, 0.0f, false);
        Decal->SetLifeSpan(0.0f);
        Decal->SetHiddenInGame(true);
        Decal->SetVisibility(false);
        Decal->SetDecalMaterial(nullptr);
    }
    ExpirationTimes[StampIndex] = -1.0f;
}

void UCMChimeraTrailComponent::UpdateExpiredStamps(const float WorldTime)
{
    for (int32 Index = 0; Index < ExpirationTimes.Num(); ++Index)
    {
        if (ExpirationTimes[Index] >= 0.0f
            && WorldTime >= ExpirationTimes[Index])
        {
            DeactivateStamp(Index);
        }
        else if (ExpirationTimes[Index] >= 0.0f
            && DecalPool.IsValidIndex(Index)
            && DecalPool[Index]->IsRegistered()
            && DecalMaterials.IsValidIndex(Index)
            && IsValid(DecalMaterials[Index]))
        {
            const float SafeFadeDuration = FMath::Max(
                0.0f,
                FadeDuration);
            const float RemainingLifetime =
                ExpirationTimes[Index] - WorldTime;
            const float FadeAlpha = SafeFadeDuration > 0.0f
                ? FMath::Clamp(
                    RemainingLifetime / SafeFadeDuration,
                    0.0f,
                    1.0f)
                : 1.0f;
            DecalMaterials[Index]->SetScalarParameterValue(
                TEXT("Opacity"),
                FMath::Max(0.0f, TrailOpacity) * FadeAlpha);
        }
    }
}

void UCMChimeraTrailComponent::ResetMovementSamples()
{
    const int32 SourceCount = GetSourceCount();
    SourceStates.SetNum(SourceCount);
    for (int32 SourceIndex = 0;
        SourceIndex < SourceCount;
        ++SourceIndex)
    {
        FSourceState& State = SourceStates[SourceIndex];
        State.CarriedDistance = 0.0f;
        State.bHasSample = GetSourceLocation(
            SourceIndex,
            State.PreviousLocation);
    }
}

int32 UCMChimeraTrailComponent::GetSourceCount() const
{
    const ACMChimera* Chimera = Cast<ACMChimera>(GetOwner());
    return Chimera
        ? FMath::Max(0, Chimera->GetActiveSegmentCount())
        : (GetOwner() ? 1 : 0);
}

bool UCMChimeraTrailComponent::GetSourceLocation(
    const int32 SourceIndex,
    FVector& OutLocation) const
{
    const ACMChimera* Chimera = Cast<ACMChimera>(GetOwner());
    if (Chimera)
    {
        const UBoxComponent* Segment =
            Chimera->GetBodySegmentComponent(SourceIndex);
        if (!Segment)
        {
            return false;
        }
        OutLocation = Segment->GetComponentLocation();
        return true;
    }

    const AActor* Owner = GetOwner();
    if (Owner && SourceIndex == 0)
    {
        OutLocation = Owner->GetActorLocation();
        return true;
    }
    return false;
}

int32 UCMChimeraTrailComponent::UpdateSourceTrail(
    const int32 SourceIndex,
    const FVector& CurrentLocation,
    const float WorldTime,
    const int32 StampBudget)
{
    if (!SourceStates.IsValidIndex(SourceIndex))
    {
        return 0;
    }

    FSourceState& State = SourceStates[SourceIndex];
    if (!State.bHasSample)
    {
        State.PreviousLocation = CurrentLocation;
        State.bHasSample = true;
        return 0;
    }

    FVector PlanarDisplacement = CurrentLocation - State.PreviousLocation;
    PlanarDisplacement.Z = 0.0f;
    const float MovementDistance = PlanarDisplacement.Size();
    if (CMChimeraTrail::IsTeleport(MovementDistance, TeleportDistance))
    {
        State.PreviousLocation = CurrentLocation;
        State.CarriedDistance = 0.0f;
        return 0;
    }

    const float SafeSpacing = FMath::Max(1.0f, StampSpacing);
    if (StampBudget <= 0)
    {
        State.CarriedDistance = FMath::Fmod(
            State.CarriedDistance + MovementDistance,
            SafeSpacing);
        State.PreviousLocation = CurrentLocation;
        return 0;
    }

    const CMChimeraTrail::FStampPlan Plan =
        CMChimeraTrail::BuildStampPlan(
            State.CarriedDistance,
            MovementDistance,
            SafeSpacing,
            StampBudget);
    State.CarriedDistance = Plan.CarriedDistance;

    if (Plan.StampCount > 0 && MovementDistance > SMALL_NUMBER)
    {
        const FVector MovementDirection =
            PlanarDisplacement / MovementDistance;
        for (int32 StampIndex = 0;
            StampIndex < Plan.StampCount;
            ++StampIndex)
        {
            const float DistanceAlongMovement = Plan.FirstStampDistance
                + StampIndex * SafeSpacing;
            const float Alpha = FMath::Clamp(
                DistanceAlongMovement / MovementDistance,
                0.0f,
                1.0f);
            TryPlaceStamp(
                FMath::Lerp(
                    State.PreviousLocation,
                    CurrentLocation,
                    Alpha),
                MovementDirection,
                WorldTime);
        }
    }

    State.PreviousLocation = CurrentLocation;
    return Plan.StampCount;
}

bool UCMChimeraTrailComponent::TryPlaceStamp(
    const FVector& SampleLocation,
    const FVector& MovementDirection,
    const float WorldTime)
{
    UWorld* World = GetWorld();
    AActor* Owner = GetOwner();
    if (!World
        || !Owner
        || DecalPool.IsEmpty()
        || !IsValid(TrailMaterial))
    {
        return false;
    }

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(CMChimeraTrailGroundTrace),
        false,
        Owner);
    FHitResult SurfaceHit;
    const FVector TraceStart = SampleLocation
        + FVector::UpVector * FMath::Max(0.0f, TraceHeight);
    const FVector TraceEnd = SampleLocation
        - FVector::UpVector * FMath::Max(0.0f, TraceDepth);
    if (!World->LineTraceSingleByChannel(
        SurfaceHit,
        TraceStart,
        TraceEnd,
        TraceChannel,
        QueryParams))
    {
        return false;
    }

    const int32 StampIndex = NextPoolIndex % DecalPool.Num();
    NextPoolIndex = (StampIndex + 1) % DecalPool.Num();
    DeactivateStamp(StampIndex);

    UDecalComponent* Decal = DecalPool[StampIndex];
    UMaterialInstanceDynamic* StampMaterial =
        GetOrCreateStampMaterial(StampIndex);
    if (!StampMaterial)
    {
        return false;
    }
    const FVector SurfaceNormal = SurfaceHit.ImpactNormal.GetSafeNormal(
        SMALL_NUMBER,
        FVector::UpVector);
    FVector Tangent = FVector::VectorPlaneProject(
        MovementDirection,
        SurfaceNormal).GetSafeNormal();
    if (Tangent.IsNearlyZero())
    {
        Tangent = FVector::ForwardVector;
    }

    StampMaterial->SetTextureParameterValue(
        TEXT("BrushTexture"), BrushTexture);
    StampMaterial->SetScalarParameterValue(
        TEXT("Opacity"),
        FMath::Max(0.0f, TrailOpacity));
    Decal->SetDecalMaterial(StampMaterial);
    Decal->DecalSize = FVector(
        FMath::Max(0.1f, DecalDepth),
        FMath::Max(1.0f, TrailWidth),
        FMath::Max(1.0f, StampLength));
    Decal->SetWorldLocationAndRotation(
        SurfaceHit.ImpactPoint
            + SurfaceNormal * FMath::Max(0.0f, SurfaceOffset),
        CMChimeraTrail::BuildDecalRotation(
            SurfaceNormal,
            Tangent));
    Decal->RegisterComponent();

    const float SafeLifetime = FMath::Max(0.0f, Lifetime);
    // The source material does not expose Unreal's decal-lifetime fade in a
    // reliable way. Per-stamp Opacity is updated explicitly instead.
    Decal->SetFadeOut(0.0f, 0.0f, false);
    Decal->SetLifeSpan(0.0f);
    Decal->SetHiddenInGame(false);
    Decal->SetVisibility(true);
    Decal->MarkRenderStateDirty();

    ExpirationTimes[StampIndex] = SafeLifetime > 0.0f
        ? WorldTime + SafeLifetime
        : -1.0f;
    return true;
}
