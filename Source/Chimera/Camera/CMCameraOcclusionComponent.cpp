#include "Camera/CMCameraOcclusionComponent.h"

#include "Camera/CameraComponent.h"
#include "Camera/CMCameraOcclusionConfig.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Parts/Head/CMHeadPartActor.h"
#include "Parts/Head/CMVisionComponent.h"
#include "Player/CMChimera.h"
#include "Player/CMControlBody.h"
#include "Player/CMControlTypes.h"
#include "Player/CMPartSlotComponent.h"

namespace
{
const FName FadeParameterName(TEXT("CM_OcclusionFade"));
const FName ScreenCenterParameterName(TEXT("CM_OcclusionCenter"));
const FName AdditionalScreenCenterParameterNames[] = {
    TEXT("CM_OcclusionCenter1"),
    TEXT("CM_OcclusionCenter2"),
    TEXT("CM_OcclusionCenter3"),
    TEXT("CM_OcclusionCenter4"),
    TEXT("CM_OcclusionCenter5"),
    TEXT("CM_OcclusionCenter6"),
    TEXT("CM_OcclusionCenter7")
};
static_assert(
    UE_ARRAY_COUNT(AdditionalScreenCenterParameterNames)
        == CMControl::MaxPlayers - 1
);
const FName ScreenRadiusParameterName(TEXT("CM_OcclusionRadius"));
const FName MinimumOpacityParameterName(TEXT("CM_OcclusionMinOpacity"));
const FName EdgeSoftnessParameterName(TEXT("CM_OcclusionEdgeSoftness"));
}

UCMCameraOcclusionComponent::UCMCameraOcclusionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UCMCameraOcclusionComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction
)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    APlayerController* PlayerController = GetOcclusionConfig().bEnabled
        ? FindLocalViewer()
        : nullptr;
    if (!PlayerController)
    {
        RestoreAllFadeStates();
        return;
    }

    TMap<UPrimitiveComponent*, TArray<FVector2D>> CurrentOccluders;
    FindCurrentOccluders(
        *PlayerController,
        CurrentOccluders
    );

    for (FCMCameraOccluderFadeState& State : FadeStates)
    {
        const TArray<FVector2D>* ScreenCenters =
            CurrentOccluders.Find(State.Component.Get());
        State.bOccluding = ScreenCenters != nullptr;
        if (ScreenCenters)
        {
            State.ScreenCenters = *ScreenCenters;
        }
    }

    for (const TPair<UPrimitiveComponent*, TArray<FVector2D>>& Pair
        : CurrentOccluders)
    {
        if (FCMCameraOccluderFadeState* State =
            FindOrAddFadeState(*Pair.Key))
        {
            State->bOccluding = true;
            State->ScreenCenters = Pair.Value;
        }
    }

    UpdateFadeStates(DeltaTime);
}

const UCMCameraOcclusionConfig&
UCMCameraOcclusionComponent::GetOcclusionConfig() const
{
    return OcclusionConfig
        ? *OcclusionConfig
        : *GetDefault<UCMCameraOcclusionConfig>();
}

void UCMCameraOcclusionComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    RestoreAllFadeStates();
    Super::EndPlay(EndPlayReason);
}

APlayerController* UCMCameraOcclusionComponent::FindLocalViewer() const
{
    const UWorld* World = GetWorld();
    AActor* Owner = GetOwner();
    if (!World || !Owner)
    {
        return nullptr;
    }

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator();
        It;
        ++It)
    {
        APlayerController* PlayerController = It->Get();
        if (PlayerController
            && PlayerController->IsLocalController()
            && PlayerController->GetViewTarget() == Owner)
        {
            return PlayerController;
        }
    }
    return nullptr;
}

void UCMCameraOcclusionComponent::FindTargetLocations(
    const UCMCameraOcclusionConfig& Config,
    TArray<FVector>& OutTargetLocations
) const
{
    OutTargetLocations.Reset();
    const ACMChimera* Chimera = Cast<ACMChimera>(GetOwner());
    UWorld* World = GetWorld();
    if (Chimera && World)
    {
        TSet<const ACMHeadPartActor*> AddedHeads;
        for (TActorIterator<ACMControlBody> It(World); It; ++It)
        {
            const ACMControlBody* ControlBody = *It;
            if (!ControlBody
                || ControlBody->GetSharedChimera() != Chimera)
            {
                continue;
            }

            for (const FCMPartSlotAddress& SlotAddress
                : ControlBody->GetControlSlots())
            {
                const UCMPartSlotComponent* PartSlot =
                    Chimera->GetPartSlotComponent(SlotAddress);
                const ACMHeadPartActor* HeadPart = PartSlot
                    ? Cast<ACMHeadPartActor>(PartSlot->GetAttachedPart())
                    : nullptr;
                if (!HeadPart || AddedHeads.Contains(HeadPart))
                {
                    continue;
                }

                AddedHeads.Add(HeadPart);
                const UCMVisionComponent* VisionComponent =
                    HeadPart->GetVisionComponent();
                OutTargetLocations.Add(VisionComponent
                    ? VisionComponent->GetVisionOrigin()
                    : HeadPart->GetActorLocation());
                break;
            }
        }
    }

    const AActor* Owner = GetOwner();
    if (OutTargetLocations.IsEmpty() && Owner)
    {
        OutTargetLocations.Add(
            Owner->GetActorLocation()
                + FVector(0.0f, 0.0f, Config.TargetHeightOffset)
        );
    }
}

void UCMCameraOcclusionComponent::FindCurrentOccluders(
    APlayerController& PlayerController,
    TMap<UPrimitiveComponent*, TArray<FVector2D>>& OutOccluders
) const
{
    const AActor* Owner = GetOwner();
    const UCameraComponent* Camera = Owner
        ? Owner->FindComponentByClass<UCameraComponent>()
        : nullptr;
    UWorld* World = GetWorld();
    if (!Owner || !Camera || !World)
    {
        return;
    }

    const UCMCameraOcclusionConfig& Config = GetOcclusionConfig();
    TArray<FVector> TargetLocations;
    FindTargetLocations(Config, TargetLocations);
    int32 ViewportWidth = 0;
    int32 ViewportHeight = 0;
    PlayerController.GetViewportSize(ViewportWidth, ViewportHeight);
    if (ViewportWidth <= 0 || ViewportHeight <= 0)
    {
        return;
    }

    FCollisionObjectQueryParams ObjectQueryParams;
    for (const ECollisionChannel ObjectType : Config.OccluderObjectTypes)
    {
        ObjectQueryParams.AddObjectTypesToQuery(ObjectType);
    }
    if (!ObjectQueryParams.IsValid())
    {
        return;
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CameraOcclusion), false);
    QueryParams.AddIgnoredActor(Owner);
    TArray<AActor*> AttachedActors;
    Owner->GetAttachedActors(AttachedActors, true, true);
    QueryParams.AddIgnoredActors(AttachedActors);

    for (const FVector& TargetLocation : TargetLocations)
    {
        FVector2D ScreenPosition;
        if (!PlayerController.ProjectWorldLocationToScreen(
            TargetLocation,
            ScreenPosition,
            true))
        {
            continue;
        }
        const FVector2D ScreenCenter = FVector2D(
            ScreenPosition.X / ViewportWidth,
            ScreenPosition.Y / ViewportHeight
        ) + Config.ScreenCenterOffset;

        TArray<FHitResult> Hits;
        World->SweepMultiByObjectType(
            Hits,
            Camera->GetComponentLocation(),
            TargetLocation,
            FQuat::Identity,
            ObjectQueryParams,
            FCollisionShape::MakeSphere(Config.TraceRadius),
            QueryParams
        );

        for (const FHitResult& Hit : Hits)
        {
            if (UPrimitiveComponent* Component = Hit.GetComponent())
            {
                OutOccluders.FindOrAdd(Component).AddUnique(ScreenCenter);
            }
        }
    }
}

FCMCameraOccluderFadeState*
UCMCameraOcclusionComponent::FindOrAddFadeState(
    UPrimitiveComponent& Component
)
{
    if (FCMCameraOccluderFadeState* Existing = FadeStates.FindByPredicate(
        [&Component](const FCMCameraOccluderFadeState& State)
        {
            return State.Component.Get() == &Component;
        }))
    {
        return Existing;
    }

    FCMCameraOccluderFadeState NewState;
    NewState.Component = &Component;
    const int32 MaterialCount = Component.GetNumMaterials();
    NewState.OriginalMaterials.SetNum(MaterialCount);
    NewState.DynamicMaterials.SetNum(MaterialCount);

    bool bHasMaterial = false;
    for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
    {
        UMaterialInterface* OriginalMaterial =
            Component.GetMaterial(MaterialIndex);
        NewState.OriginalMaterials[MaterialIndex] = OriginalMaterial;
        if (!OriginalMaterial)
        {
            continue;
        }

        NewState.DynamicMaterials[MaterialIndex] =
            Component.CreateDynamicMaterialInstance(
                MaterialIndex,
                OriginalMaterial
            );
        bHasMaterial |= NewState.DynamicMaterials[MaterialIndex] != nullptr;
    }

    if (!bHasMaterial)
    {
        return nullptr;
    }

    return &FadeStates.Add_GetRef(MoveTemp(NewState));
}

void UCMCameraOcclusionComponent::UpdateFadeStates(float DeltaTime)
{
    const UCMCameraOcclusionConfig& Config = GetOcclusionConfig();
    for (int32 Index = FadeStates.Num() - 1; Index >= 0; --Index)
    {
        FCMCameraOccluderFadeState& State = FadeStates[Index];
        if (!State.Component.IsValid())
        {
            FadeStates.RemoveAtSwap(Index);
            continue;
        }

        const float TargetFade = State.bOccluding ? 1.0f : 0.0f;
        const float FadeSpeed = State.bOccluding
            ? Config.FadeOutSpeed
            : Config.FadeInSpeed;
        State.Fade = FMath::FInterpConstantTo(
            State.Fade,
            TargetFade,
            DeltaTime,
            FadeSpeed
        );

        for (UMaterialInstanceDynamic* Material : State.DynamicMaterials)
        {
            if (!Material)
            {
                continue;
            }
            Material->SetScalarParameterValue(FadeParameterName, State.Fade);
            Material->SetVectorParameterValue(
                ScreenCenterParameterName,
                State.ScreenCenters.IsValidIndex(0)
                    ? FLinearColor(
                        State.ScreenCenters[0].X,
                        State.ScreenCenters[0].Y,
                        0.0f,
                        0.0f)
                    : FLinearColor(10.0f, 10.0f, 0.0f, 0.0f)
            );
            for (int32 CenterIndex = 1;
                CenterIndex < CMControl::MaxPlayers;
                ++CenterIndex)
            {
                const FVector2D Center =
                    State.ScreenCenters.IsValidIndex(CenterIndex)
                    ? State.ScreenCenters[CenterIndex]
                    : FVector2D(10.0f, 10.0f);
                Material->SetVectorParameterValue(
                    AdditionalScreenCenterParameterNames[CenterIndex - 1],
                    FLinearColor(Center.X, Center.Y, 0.0f, 0.0f)
                );
            }
            Material->SetScalarParameterValue(
                ScreenRadiusParameterName,
                Config.ScreenFadeRadius
            );
            Material->SetScalarParameterValue(
                MinimumOpacityParameterName,
                Config.MinimumOpacity
            );
            Material->SetScalarParameterValue(
                EdgeSoftnessParameterName,
                Config.EdgeSoftness
            );
        }

        if (!State.bOccluding && State.Fade <= KINDA_SMALL_NUMBER)
        {
            RestoreFadeState(State);
            FadeStates.RemoveAtSwap(Index);
        }
    }
}

void UCMCameraOcclusionComponent::RestoreFadeState(
    FCMCameraOccluderFadeState& State
)
{
    UPrimitiveComponent* Component = State.Component.Get();
    if (!Component)
    {
        return;
    }

    for (int32 MaterialIndex = 0;
        MaterialIndex < State.OriginalMaterials.Num();
        ++MaterialIndex)
    {
        Component->SetMaterial(
            MaterialIndex,
            State.OriginalMaterials[MaterialIndex]
        );
    }
}

void UCMCameraOcclusionComponent::RestoreAllFadeStates()
{
    for (FCMCameraOccluderFadeState& State : FadeStates)
    {
        RestoreFadeState(State);
    }
    FadeStates.Reset();
}
