#include "Player/CMChimera.h"

#include "Player/CMControlBody.h"
#include "Player/CMPlayerState.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void ACMChimera::UpdateControlAssignmentMarkers(float DeltaTime)
{
    if (GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    TArray<const ACMPlayerState*> ControlOwners;
    ControlOwners.Init(nullptr, CMControl::MaxPartSlots);
    TArray<int32> ControlSlotIndices;
    ControlSlotIndices.Init(INDEX_NONE, CMControl::MaxPartSlots);

    if (GetWorld())
    {
        for (TActorIterator<ACMControlBody> It(GetWorld()); It; ++It)
        {
            const ACMControlBody* ControlBody = *It;
            const ACMPlayerState* CMPlayerState = ControlBody
                ? ControlBody->GetPlayerState<ACMPlayerState>()
                : nullptr;
            if (!CMPlayerState
                || CMPlayerState->IsOnlyASpectator()
                || !ControlBody->IsControlInputEnabled())
            {
                continue;
            }

            for (int32 SlotIndex = 0;
                SlotIndex < ControlBody->GetControlSlots().Num();
                ++SlotIndex)
            {
                const FCMPartSlotAddress PartSlotAddress =
                    ControlBody->GetControlSlots()[SlotIndex];
                if (CMControl::IsValidPartSlot(
                    PartSlotAddress,
                    ActiveSegmentCount))
                {
                    const int32 PartSlotFlatIndex =
                        CMControl::ToFlatPartSlotIndex(
                            PartSlotAddress
                        );
                    ControlOwners[PartSlotFlatIndex] = CMPlayerState;
                    ControlSlotIndices[PartSlotFlatIndex] = SlotIndex;
                }
            }
        }
    }

    if (AppliedMarkerColorIndices.Num()
        != ControlAssignmentMarkers.Num())
    {
        AppliedMarkerColorIndices.Init(
            INDEX_NONE,
            ControlAssignmentMarkers.Num()
        );
    }
    if (AppliedMarkerSlotIndices.Num()
        != ControlAssignmentMarkers.Num())
    {
        AppliedMarkerSlotIndices.Init(
            INDEX_NONE,
            ControlAssignmentMarkers.Num()
        );
    }
    if (AppliedMarkerPressedStates.Num()
        != ControlAssignmentMarkers.Num())
    {
        AppliedMarkerPressedStates.Init(
            false,
            ControlAssignmentMarkers.Num()
        );
    }

    APlayerCameraManager* CameraManager = nullptr;
    if (const APlayerController* LocalPlayerController = GetWorld()
        ? GetWorld()->GetFirstPlayerController()
        : nullptr)
    {
        CameraManager = LocalPlayerController->PlayerCameraManager;
    }
    static const TCHAR* KeyLabels[] =
    {
        TEXT("Q"),
        TEXT("W"),
        TEXT("E"),
        TEXT("R")
    };

    for (int32 MarkerIndex = 0;
        MarkerIndex < ControlAssignmentMarkers.Num();
        ++MarkerIndex)
    {
        UStaticMeshComponent* Marker =
            ControlAssignmentMarkers[MarkerIndex];
        UTextRenderComponent* MarkerText =
            ControlAssignmentMarkerTexts.IsValidIndex(MarkerIndex)
                ? ControlAssignmentMarkerTexts[MarkerIndex]
                : nullptr;
        const ACMPlayerState* ControlOwner =
            ControlOwners.IsValidIndex(MarkerIndex)
                ? ControlOwners[MarkerIndex]
                : nullptr;
        const int32 SegmentIndex =
            MarkerIndex / CMControl::PartSlotsPerSegment;
        const bool bShouldShow = Marker
            && ControlOwner
            && SegmentIndex < ActiveSegmentCount;

        if (!Marker)
        {
            continue;
        }

        Marker->SetVisibility(bShouldShow);
        Marker->SetHiddenInGame(!bShouldShow);
        if (MarkerText)
        {
            MarkerText->SetVisibility(bShouldShow);
            MarkerText->SetHiddenInGame(!bShouldShow);
        }
        if (!bShouldShow)
        {
            AppliedMarkerColorIndices[MarkerIndex] = INDEX_NONE;
            AppliedMarkerSlotIndices[MarkerIndex] = INDEX_NONE;
            AppliedMarkerPressedStates[MarkerIndex] = false;
            continue;
        }

        const int32 OwnerColorIndex =
            ControlOwner->GetPlayerColorIndex();
        const bool bIsPressed = (PressedPartSlotMask
            & (1u << MarkerIndex)) != 0;
        const float TargetScaleMultiplier = bIsPressed
            ? FMath::Max(PressedMarkerScale, 1.0f)
            : 1.0f;
        const FVector BaseMarkerScale(
            ControlMarkerRadius / 50.0f,
            ControlMarkerRadius / 50.0f,
            ControlMarkerThickness / 100.0f
        );
        Marker->SetRelativeScale3D(FMath::VInterpTo(
            Marker->GetRelativeScale3D(),
            BaseMarkerScale * TargetScaleMultiplier,
            DeltaTime,
            MarkerScaleAnimationSpeed
        ));
        if (MarkerText)
        {
            MarkerText->SetWorldSize(FMath::FInterpTo(
                MarkerText->WorldSize,
                ControlMarkerTextWorldSize * TargetScaleMultiplier,
                DeltaTime,
                MarkerScaleAnimationSpeed
            ));
        }
        if ((AppliedMarkerColorIndices[MarkerIndex] != OwnerColorIndex
                || AppliedMarkerPressedStates[MarkerIndex] != bIsPressed)
            && ControlMarkerMaterials.IsValidIndex(MarkerIndex)
            && ControlMarkerMaterials[MarkerIndex])
        {
            FLinearColor MarkerColor = ControlOwner->GetPlayerColor();
            if (bIsPressed)
            {
                const float Brightness = FMath::Clamp(
                    PressedMarkerBrightness,
                    0.0f,
                    1.0f
                );
                MarkerColor.R *= Brightness;
                MarkerColor.G *= Brightness;
                MarkerColor.B *= Brightness;
            }
            ControlMarkerMaterials[MarkerIndex]->SetVectorParameterValue(
                ControlMarkerColorParameter,
                MarkerColor
            );
            AppliedMarkerColorIndices[MarkerIndex] = OwnerColorIndex;
            AppliedMarkerPressedStates[MarkerIndex] = bIsPressed;
        }

        const int32 SlotIndex = ControlSlotIndices.IsValidIndex(MarkerIndex)
            ? ControlSlotIndices[MarkerIndex]
            : INDEX_NONE;
        if (MarkerText
            && SlotIndex >= 0
            && SlotIndex < UE_ARRAY_COUNT(KeyLabels))
        {
            if (AppliedMarkerSlotIndices[MarkerIndex] != SlotIndex)
            {
                MarkerText->SetText(FText::FromString(KeyLabels[SlotIndex]));
                AppliedMarkerSlotIndices[MarkerIndex] = SlotIndex;
            }

            if (CameraManager)
            {
                const FVector ToCamera =
                    CameraManager->GetCameraLocation()
                    - MarkerText->GetComponentLocation();
                if (!ToCamera.IsNearlyZero())
                {
                    const FVector CameraUp =
                        CameraManager->GetCameraRotation()
                            .RotateVector(FVector::UpVector);
                    MarkerText->SetWorldRotation(
                        FRotationMatrix::MakeFromXZ(
                            ToCamera.GetSafeNormal(),
                            CameraUp
                        ).Rotator()
                    );
                }
            }
        }
    }
}
