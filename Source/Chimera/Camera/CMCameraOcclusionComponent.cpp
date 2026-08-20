#include "Camera/CMCameraOcclusionComponent.h"

#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

UCMCameraOcclusionComponent::UCMCameraOcclusionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
    OccluderObjectTypes.Add(ECC_WorldStatic);
}

void UCMCameraOcclusionComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction
)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    APlayerController* PlayerController = bEnabled
        ? FindLocalViewer()
        : nullptr;
    if (!PlayerController)
    {
        RestoreAllFadeStates();
        return;
    }

    TSet<UPrimitiveComponent*> CurrentOccluders;
    FVector2D ScreenCenter = FVector2D(0.5f, 0.5f);
    FindCurrentOccluders(
        *PlayerController,
        CurrentOccluders,
        ScreenCenter
    );

    for (FCMCameraOccluderFadeState& State : FadeStates)
    {
        State.bOccluding = CurrentOccluders.Contains(State.Component.Get());
    }

    for (UPrimitiveComponent* Component : CurrentOccluders)
    {
        if (FCMCameraOccluderFadeState* State =
            FindOrAddFadeState(*Component))
        {
            State->bOccluding = true;
        }
    }

    UpdateFadeStates(DeltaTime, ScreenCenter);
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

void UCMCameraOcclusionComponent::FindCurrentOccluders(
    APlayerController& PlayerController,
    TSet<UPrimitiveComponent*>& OutOccluders,
    FVector2D& OutScreenCenter
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

    const FVector TargetLocation = Owner->GetActorLocation()
        + FVector(0.0f, 0.0f, TargetHeightOffset);
    FVector2D ScreenPosition;
    int32 ViewportWidth = 0;
    int32 ViewportHeight = 0;
    PlayerController.GetViewportSize(ViewportWidth, ViewportHeight);
    if (ViewportWidth > 0
        && ViewportHeight > 0
        && PlayerController.ProjectWorldLocationToScreen(
            TargetLocation,
            ScreenPosition,
            true))
    {
        OutScreenCenter = FVector2D(
            ScreenPosition.X / ViewportWidth,
            ScreenPosition.Y / ViewportHeight
        );
    }

    FCollisionObjectQueryParams ObjectQueryParams;
    for (const ECollisionChannel ObjectType : OccluderObjectTypes)
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

    TArray<FHitResult> Hits;
    World->SweepMultiByObjectType(
        Hits,
        Camera->GetComponentLocation(),
        TargetLocation,
        FQuat::Identity,
        ObjectQueryParams,
        FCollisionShape::MakeSphere(TraceRadius),
        QueryParams
    );

    for (const FHitResult& Hit : Hits)
    {
        if (UPrimitiveComponent* Component = Hit.GetComponent())
        {
            OutOccluders.Add(Component);
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

void UCMCameraOcclusionComponent::UpdateFadeStates(
    float DeltaTime,
    const FVector2D& ScreenCenter
)
{
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
            ? FadeOutSpeed
            : FadeInSpeed;
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
                FLinearColor(ScreenCenter.X, ScreenCenter.Y, 0.0f, 0.0f)
            );
            Material->SetScalarParameterValue(
                ScreenRadiusParameterName,
                ScreenFadeRadius
            );
            Material->SetScalarParameterValue(
                MinimumOpacityParameterName,
                MinimumOpacity
            );
            Material->SetScalarParameterValue(
                EdgeSoftnessParameterName,
                EdgeSoftness
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
