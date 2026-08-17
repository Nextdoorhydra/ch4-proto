#include "CMPawn.h"

#include "GameMode/CMGameState.h"
#include "Player/CMPlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/SpringArmComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"

ACMPawn::ACMPawn()
{
    constexpr int32 MaxSegmentCount = 4;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> MarkerMeshAsset(
        TEXT("/Engine/BasicShapes/Cylinder.Cylinder")
    );
    static ConstructorHelpers::FObjectFinder<UMaterialInterface>
        MarkerMaterialAsset(
            TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")
        );
    static ConstructorHelpers::FObjectFinder<UMaterialInterface>
        MarkerTextMaterialAsset(
            TEXT("/Engine/EngineMaterials/UnlitText.UnlitText")
        );

    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);
    SetNetUpdateFrequency(30.0f);
    SetMinNetUpdateFrequency(10.0f);

    BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("BodyMesh")
    );
    SetRootComponent(BodyMesh);

    BodyMesh->SetSimulatePhysics(true);
    BodyMesh->SetEnableGravity(bEnableBodyGravity);
    BodyMesh->SetLinearDamping(BodyLinearDamping);
    BodyMesh->SetAngularDamping(BodyAngularDamping);
    BodyMesh->SetCollisionProfileName(BodyCollisionProfile);
    BodyMesh->SetRelativeScale3D(FVector(SegmentScale));
    BodyMesh->SetIsReplicated(true);

    LeftFootPoint = CreateDefaultSubobject<USceneComponent>(
        TEXT("LeftFootPoint")
    );
    LeftFootPoint->SetupAttachment(BodyMesh);
    LeftFootPoint->SetRelativeLocation(LeftFootOffset);

    RightFootPoint = CreateDefaultSubobject<USceneComponent>(
        TEXT("RightFootPoint")
    );
    RightFootPoint->SetupAttachment(BodyMesh);
    RightFootPoint->SetRelativeLocation(RightFootOffset);

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(
        TEXT("CameraBoom")
    );
    CameraBoom->SetupAttachment(BodyMesh);
    CameraBoom->SetUsingAbsoluteRotation(true);
    CameraBoom->TargetArmLength = DefaultCameraDistance;
    CameraBoom->SetRelativeRotation(
        FRotator(-90.0f, 0.0f, 0.0f)
    );
    CameraBoom->SocketOffset = CameraSocketOffset;
    CameraBoom->TargetOffset = FVector(-CameraFollowBackOffset, 0.0f, 0.0f);
    CameraBoom->bUsePawnControlRotation = false;
    CameraBoom->bInheritPitch = false;
    CameraBoom->bInheritYaw = false;
    CameraBoom->bInheritRoll = false;
    CameraBoom->bEnableCameraLag = bEnableCameraLag;
    CameraBoom->CameraLagSpeed = CameraLagSpeed;
    CameraBoom->CameraLagMaxDistance = CameraLagMaxDistance;
    CameraBoom->bEnableCameraRotationLag = bEnableCameraRotationLag;
    CameraBoom->CameraRotationLagSpeed = CameraRotationLagSpeed;
    CameraBoom->bDoCollisionTest = false;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(
        TEXT("FollowCamera")
    );
    FollowCamera->SetupAttachment(
        CameraBoom,
        USpringArmComponent::SocketName
    );
    FollowCamera->bUsePawnControlRotation = false;

    BodySegments.Add(BodyMesh);
    LeftFootPoints.Add(LeftFootPoint);
    RightFootPoints.Add(RightFootPoint);

    for (int32 Index = 1; Index < MaxSegmentCount; ++Index)
    {
        UStaticMeshComponent* SegmentBody =
            CreateDefaultSubobject<UStaticMeshComponent>(
                *FString::Printf(TEXT("BodyMesh_%d"), Index + 1)
            );
        SegmentBody->SetupAttachment(BodyMesh);
        SegmentBody->SetRelativeLocation(
            FVector(-SegmentSpacing * SegmentScale * Index, 0.0f, 0.0f)
        );
        SegmentBody->SetRelativeScale3D(FVector(SegmentScale));
        SegmentBody->SetSimulatePhysics(false);
        SegmentBody->SetEnableGravity(bEnableBodyGravity);
        SegmentBody->SetLinearDamping(BodyLinearDamping);
        SegmentBody->SetAngularDamping(BodyAngularDamping);
        SegmentBody->SetCollisionProfileName(BodyCollisionProfile);

        USceneComponent* SegmentLeftFoot =
            CreateDefaultSubobject<USceneComponent>(
                *FString::Printf(TEXT("LeftFootPoint_%d"), Index + 1)
            );
        SegmentLeftFoot->SetupAttachment(SegmentBody);
        SegmentLeftFoot->SetRelativeLocation(LeftFootOffset);

        USceneComponent* SegmentRightFoot =
            CreateDefaultSubobject<USceneComponent>(
                *FString::Printf(TEXT("RightFootPoint_%d"), Index + 1)
            );
        SegmentRightFoot->SetupAttachment(SegmentBody);
        SegmentRightFoot->SetRelativeLocation(RightFootOffset);

        UPhysicsConstraintComponent* Constraint =
            CreateDefaultSubobject<UPhysicsConstraintComponent>(
                *FString::Printf(TEXT("SegmentConstraint_%d"), Index)
            );
        Constraint->SetupAttachment(BodyMesh);
        Constraint->SetDisableCollision(bDisableCollisionBetweenSegments);

        BodySegments.Add(SegmentBody);
        LeftFootPoints.Add(SegmentLeftFoot);
        RightFootPoints.Add(SegmentRightFoot);
        SegmentConstraints.Add(Constraint);
    }

    for (int32 ControlIndex = 0;
        ControlIndex < CMControl::MaxControlParts;
        ++ControlIndex)
    {
        const ECMControlPart ControlPart =
            static_cast<ECMControlPart>(ControlIndex);
        const int32 SegmentIndex =
            CMControl::GetSegmentIndex(ControlPart);
        USceneComponent* FootPoint = CMControl::IsRightLeg(ControlPart)
            ? RightFootPoints[SegmentIndex]
            : LeftFootPoints[SegmentIndex];

        UStaticMeshComponent* Marker =
            CreateDefaultSubobject<UStaticMeshComponent>(
                *FString::Printf(
                    TEXT("ControlAssignmentMarker_%d"),
                    ControlIndex + 1
                )
            );
        Marker->SetupAttachment(FootPoint);
        Marker->SetAbsolute(false, false, true);
        Marker->SetRelativeLocation(
            FVector(0.0f, 0.0f, ControlMarkerHeightOffset)
        );
        Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Marker->SetGenerateOverlapEvents(false);
        Marker->SetCanEverAffectNavigation(false);
        Marker->SetCastShadow(false);
        Marker->SetHiddenInGame(true);
        Marker->SetVisibility(false);

        if (MarkerMeshAsset.Succeeded())
        {
            Marker->SetStaticMesh(MarkerMeshAsset.Object);
        }
        if (MarkerMaterialAsset.Succeeded())
        {
            Marker->SetMaterial(0, MarkerMaterialAsset.Object);
        }

        ControlAssignmentMarkers.Add(Marker);

        UTextRenderComponent* MarkerText =
            CreateDefaultSubobject<UTextRenderComponent>(
                *FString::Printf(
                    TEXT("ControlAssignmentMarkerText_%d"),
                    ControlIndex + 1
                )
            );
        MarkerText->SetupAttachment(FootPoint);
        MarkerText->SetAbsolute(false, true, true);
        MarkerText->SetRelativeLocation(FVector(
            0.0f,
            0.0f,
            ControlMarkerHeightOffset + ControlMarkerTextHeightOffset
        ));
        MarkerText->SetWorldSize(ControlMarkerTextWorldSize);
        MarkerText->SetHorizontalAlignment(EHTA_Center);
        MarkerText->SetVerticalAlignment(EVRTA_TextCenter);
        MarkerText->SetTextRenderColor(FColor::Black);
        if (MarkerTextMaterialAsset.Succeeded())
        {
            MarkerText->SetTextMaterial(MarkerTextMaterialAsset.Object);
        }
        MarkerText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        MarkerText->SetGenerateOverlapEvents(false);
        MarkerText->SetCanEverAffectNavigation(false);
        MarkerText->SetCastShadow(false);
        MarkerText->SetTranslucentSortPriority(10);
        MarkerText->SetHiddenInGame(true);
        MarkerText->SetVisibility(false);
        ControlAssignmentMarkerTexts.Add(MarkerText);
    }

}

void ACMPawn::BeginPlay()
{
    Super::BeginPlay();

    ConfigureSegments();
    ConfigureNetworkPhysics();

    ControlMarkerMaterials.SetNum(ControlAssignmentMarkers.Num());
    AppliedMarkerColorIndices.Init(
        INDEX_NONE,
        ControlAssignmentMarkers.Num()
    );
    AppliedMarkerSlotIndices.Init(
        INDEX_NONE,
        ControlAssignmentMarkers.Num()
    );
    AppliedMarkerPressedStates.Init(
        false,
        ControlAssignmentMarkers.Num()
    );
    for (int32 MarkerIndex = 0;
        MarkerIndex < ControlAssignmentMarkers.Num();
        ++MarkerIndex)
    {
        if (ControlAssignmentMarkers[MarkerIndex])
        {
            ControlMarkerMaterials[MarkerIndex] =
                ControlAssignmentMarkers[MarkerIndex]
                    ->CreateDynamicMaterialInstance(0);
        }
    }
    UpdateControlAssignmentMarkers(0.0f);
}

void ACMPawn::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplyBlueprintSettings();
}

void ACMPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UpdateCameraFollowOffset();
    UpdateControlAssignmentMarkers(DeltaTime);

    if (!HasAuthority())
    {
        ApplyReplicatedSegmentStates(DeltaTime);
        return;
    }

    if (!PendingCooperationContributions.IsEmpty())
    {
        CooperationWindowRemaining -= DeltaTime;
        if (CooperationWindowRemaining <= 0.0f)
        {
            FlushCooperativeInput();
        }
    }

    FVector Velocity = BodyMesh->GetPhysicsLinearVelocity();

    const FVector HorizontalVelocity(
        Velocity.X,
        Velocity.Y,
        0.0f
    );

    const float EffectiveMaxSpeed =
        MaxSpeed * GetPlayerCountSpeedMultiplier();
    if (HorizontalVelocity.Size() > EffectiveMaxSpeed)
    {
        const FVector LimitedHorizontalVelocity =
            HorizontalVelocity.GetSafeNormal() * EffectiveMaxSpeed;

        Velocity.X = LimitedHorizontalVelocity.X;
        Velocity.Y = LimitedHorizontalVelocity.Y;

        BodyMesh->SetPhysicsLinearVelocity(Velocity);
    }

    UpdateReplicatedSegmentStates();
}

void ACMPawn::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACMPawn, ReplicatedSegmentStates);
    DOREPLIFETIME(ACMPawn, PressedControlPartMask);
}

void ACMPawn::ActivateControlPart(
    ECMControlPart ControlPart,
    ACMPlayerState* ContributingPlayerState
)
{
    if (!HasAuthority() || !CMControl::IsValidPart(ControlPart))
    {
        return;
    }

    TryApplyLegImpulse(
        CMControl::GetSegmentIndex(ControlPart),
        CMControl::IsRightLeg(ControlPart),
        ContributingPlayerState
    );
}

void ACMPawn::SetControlPartPressed(
    ECMControlPart ControlPart,
    bool bPressed
)
{
    if (!HasAuthority() || !CMControl::IsValidPart(ControlPart))
    {
        return;
    }

    const uint8 ControlBit =
        static_cast<uint8>(1u << static_cast<uint8>(ControlPart));
    if (bPressed)
    {
        PressedControlPartMask |= ControlBit;
    }
    else
    {
        PressedControlPartMask &= ~ControlBit;
    }

    ForceNetUpdate();
}

void ACMPawn::ClearPressedControlParts()
{
    if (!HasAuthority() || PressedControlPartMask == 0)
    {
        return;
    }

    PressedControlPartMask = 0;
    ForceNetUpdate();
}

FRotator ACMPawn::GetInitialCameraRotation() const
{
    return FRotator(-90.0f, 0.0f, 0.0f);
}

float ACMPawn::GetMouseLookSensitivity() const
{
    return MouseLookSensitivity;
}

bool ACMPawn::IsMousePitchInverted() const
{
    return bInvertMousePitch;
}

void ACMPawn::SetLocalCameraRotation(
    const FRotator& NewCameraRotation
)
{
    if (CameraBoom && GetNetMode() != NM_DedicatedServer)
    {
        CameraBoom->SetWorldRotation(FRotator(-90.0f, 0.0f, 0.0f));
    }
}

void ACMPawn::AdjustLocalCameraZoom(float AxisValue)
{
    if (!CameraBoom || FMath::IsNearlyZero(AxisValue))
    {
        return;
    }

    CameraBoom->TargetArmLength = FMath::Clamp(
        CameraBoom->TargetArmLength - AxisValue * ZoomStep,
        MinimumCameraDistance,
        MaximumCameraDistance
    );
}

void ACMPawn::TryApplyLegImpulse(
    int32 SegmentIndex,
    bool bRightLeg,
    ACMPlayerState* ContributingPlayerState
)
{
    if (SegmentIndex < 0
        || SegmentIndex >= ActiveSegmentCount
        || !BodySegments.IsValidIndex(SegmentIndex))
    {
        return;
    }

    USceneComponent* FootPoint = bRightLeg
        ? RightFootPoints[SegmentIndex]
        : LeftFootPoints[SegmentIndex];

    ApplyLegImpulse(
        BodySegments[SegmentIndex],
        FootPoint,
        ContributingPlayerState
    );
}

void ACMPawn::ApplyLegImpulse(
    UStaticMeshComponent* SegmentBody,
    USceneComponent* FootPoint,
    ACMPlayerState* ContributingPlayerState
)
{
    if (!SegmentBody || !FootPoint)
    {
        return;
    }

    FHitResult GroundHit;

    if (!TraceGround(FootPoint, GroundHit))
    {
        return;
    }

    FVector ForwardDirection = SegmentBody->GetForwardVector();
    ForwardDirection.Z = 0.0f;
    ForwardDirection.Normalize();

    const FVector Impulse =
        ForwardDirection
        * LegImpulse
        * GetPerControlImpulseMultiplier();

    const float TranslationFraction = FMath::Clamp(
        IndividualPlanarTranslationFraction,
        0.0f,
        1.0f
    );
    const float RotationFraction = FMath::Clamp(
        IndividualYawRotationFraction,
        0.0f,
        1.0f
    );
    const FVector RotationalImpulse = Impulse * RotationFraction;
    SegmentBody->AddImpulseAtLocation(
        RotationalImpulse,
        GroundHit.ImpactPoint
    );
    SegmentBody->AddImpulse(
        Impulse * (TranslationFraction - RotationFraction)
    );

    const FVector LeverArm =
        GroundHit.ImpactPoint - SegmentBody->GetCenterOfMass();
    const float IntendedYawAngularImpulse =
        FVector::CrossProduct(LeverArm, Impulse).Z;
    RegisterCooperativeInput(
        ContributingPlayerState,
        Impulse,
        IntendedYawAngularImpulse
    );
}

void ACMPawn::RegisterCooperativeInput(
    ACMPlayerState* ContributingPlayerState,
    const FVector& PlanarImpulse,
    float YawAngularImpulse
)
{
    if (!HasAuthority()
        || !IsValid(ContributingPlayerState)
        || PlanarImpulse.IsNearlyZero())
    {
        return;
    }

    if (PendingCooperationContributions.IsEmpty())
    {
        CooperationWindowRemaining = FMath::Max(
            CooperationInputWindow,
            0.01f
        );
    }

    const int32 ContributorId = ContributingPlayerState->GetUniqueID();
    if (!PendingCooperationContributions.Contains(ContributorId))
    {
        FVector HorizontalImpulse = PlanarImpulse;
        HorizontalImpulse.Z = 0.0f;
        PendingCooperationContributions.Add(
            ContributorId,
            HorizontalImpulse
        );
        PendingCooperationYawImpulses.Add(
            ContributorId,
            YawAngularImpulse
        );
    }
}

void ACMPawn::FlushCooperativeInput()
{
    const int32 ContributorCount =
        PendingCooperationContributions.Num();
    const ACMGameState* GameState = GetWorld()
        ? GetWorld()->GetGameState<ACMGameState>()
        : nullptr;
    const int32 ActivePlayerCount = FMath::Clamp(
        GameState ? GameState->GetLobbyPlayerCount() : 1,
        1,
        CMControl::MaxPlayers
    );
    if (!HasAuthority()
        || !BodyMesh
        || ContributorCount <= 0)
    {
        PendingCooperationContributions.Reset();
        PendingCooperationYawImpulses.Reset();
        CooperationWindowRemaining = 0.0f;
        return;
    }

    const float ParticipationRatio = FMath::Clamp(
        static_cast<float>(ContributorCount) / ActivePlayerCount,
        0.0f,
        1.0f
    );

    FVector DirectionSum = FVector::ZeroVector;
    for (const TPair<int32, FVector>& Contribution
        : PendingCooperationContributions)
    {
        DirectionSum += Contribution.Value.GetSafeNormal2D();
    }

    const float DirectionCoherence =
        DirectionSum.Size2D() / ContributorCount;
    if (DirectionCoherence >= CooperationDirectionThreshold)
    {
        const FVector CoherentDirection =
            DirectionSum.GetSafeNormal2D();
        float AlignedImpulseMagnitude = 0.0f;
        for (const TPair<int32, FVector>& Contribution
            : PendingCooperationContributions)
        {
            AlignedImpulseMagnitude += FMath::Max(
                0.0f,
                FVector::DotProduct(
                    Contribution.Value,
                    CoherentDirection
                )
            );
        }

        const float TranslationFraction = FMath::Clamp(
            IndividualPlanarTranslationFraction,
            0.0f,
            1.0f
        );
        const float RetainedIndividualImpulse =
            AlignedImpulseMagnitude * TranslationFraction;
        const float MaximumFinalImpulse = FMath::Max(
            MaximumCooperativePlanarImpulse,
            0.0f
        );
        const float TargetFinalImpulse =
            MaximumFinalImpulse
            * ParticipationRatio
            * DirectionCoherence;
        const float BonusImpulseMagnitude = FMath::Max(
            TargetFinalImpulse - RetainedIndividualImpulse,
            0.0f
        );
        const FVector CooperationBonusImpulse =
            CoherentDirection * BonusImpulseMagnitude;
        BodyMesh->AddImpulse(CooperationBonusImpulse);
    }

    float YawDirectionSum = 0.0f;
    for (const TPair<int32, float>& Contribution
        : PendingCooperationYawImpulses)
    {
        if (!FMath::IsNearlyZero(Contribution.Value))
        {
            YawDirectionSum += FMath::Sign(Contribution.Value);
        }
    }

    const float YawDirectionCoherence =
        FMath::Abs(YawDirectionSum) / ContributorCount;
    if (YawDirectionCoherence >= CooperationDirectionThreshold)
    {
        const float YawDirection = FMath::Sign(YawDirectionSum);
        float AlignedYawAngularImpulse = 0.0f;
        for (const TPair<int32, float>& Contribution
            : PendingCooperationYawImpulses)
        {
            if (FMath::Sign(Contribution.Value) == YawDirection)
            {
                AlignedYawAngularImpulse += FMath::Abs(
                    Contribution.Value
                );
            }
        }

        const float RotationFraction = FMath::Clamp(
            IndividualYawRotationFraction,
            0.0f,
            1.0f
        );
        const float RetainedIndividualYawImpulse =
            AlignedYawAngularImpulse * RotationFraction;
        const float MaximumFinalYawImpulse = FMath::Max(
            MaximumCooperativeYawAngularImpulse,
            0.0f
        );
        const float TargetFinalYawImpulse =
            MaximumFinalYawImpulse
            * ParticipationRatio
            * YawDirectionCoherence;
        const float YawBonusImpulseMagnitude = FMath::Max(
            TargetFinalYawImpulse - RetainedIndividualYawImpulse,
            0.0f
        );
        BodyMesh->AddAngularImpulseInRadians(FVector(
            0.0f,
            0.0f,
            YawDirection * YawBonusImpulseMagnitude
        ));
    }

    PendingCooperationContributions.Reset();
    PendingCooperationYawImpulses.Reset();
    CooperationWindowRemaining = 0.0f;
}

void ACMPawn::ApplyBlueprintSettings()
{
    ActiveSegmentCount = FMath::Clamp(
        ActiveSegmentCount,
        1,
        BodySegments.Num()
    );

    const float SafeSegmentScale = FMath::Max(SegmentScale, 0.1f);
    const float EffectiveSpacing = SegmentSpacing * SafeSegmentScale;

    for (int32 Index = 0; Index < BodySegments.Num(); ++Index)
    {
        UStaticMeshComponent* SegmentBody = BodySegments[Index];
        if (!SegmentBody)
        {
            continue;
        }

        SegmentBody->SetEnableGravity(bEnableBodyGravity);
        SegmentBody->SetLinearDamping(BodyLinearDamping);
        SegmentBody->SetAngularDamping(BodyAngularDamping);
        SegmentBody->SetCollisionProfileName(BodyCollisionProfile);
        SegmentBody->SetWorldScale3D(FVector(SafeSegmentScale));
        ConfigureBodyRotationLock(SegmentBody);

        if (Index > 0 && !SegmentBody->IsSimulatingPhysics())
        {
            const FVector SegmentLocation =
                BodyMesh->GetComponentLocation()
                - BodyMesh->GetForwardVector()
                    * EffectiveSpacing * Index;
            SegmentBody->SetWorldLocationAndRotation(
                SegmentLocation,
                BodyMesh->GetComponentQuat()
            );
        }

        const bool bIsActive = Index < ActiveSegmentCount;
        SegmentBody->SetVisibility(bIsActive, true);
    }

    for (USceneComponent* FootPoint : LeftFootPoints)
    {
        if (FootPoint)
        {
            FootPoint->SetRelativeLocation(LeftFootOffset);
        }
    }

    for (USceneComponent* FootPoint : RightFootPoints)
    {
        if (FootPoint)
        {
            FootPoint->SetRelativeLocation(RightFootOffset);
        }
    }

    for (UPhysicsConstraintComponent* Constraint : SegmentConstraints)
    {
        if (Constraint)
        {
            Constraint->SetDisableCollision(
                bDisableCollisionBetweenSegments
            );
        }
    }

    const FVector MarkerScale(
        ControlMarkerRadius / 50.0f,
        ControlMarkerRadius / 50.0f,
        ControlMarkerThickness / 100.0f
    );
    for (UStaticMeshComponent* Marker : ControlAssignmentMarkers)
    {
        if (Marker)
        {
            Marker->SetRelativeLocation(
                FVector(0.0f, 0.0f, ControlMarkerHeightOffset)
            );
            Marker->SetRelativeScale3D(MarkerScale);
        }
    }

    for (UTextRenderComponent* MarkerText : ControlAssignmentMarkerTexts)
    {
        if (MarkerText)
        {
            MarkerText->SetRelativeLocation(FVector(
                0.0f,
                0.0f,
                ControlMarkerHeightOffset + ControlMarkerTextHeightOffset
            ));
            MarkerText->SetWorldSize(ControlMarkerTextWorldSize);
        }
    }

    if (CameraBoom)
    {
        CameraBoom->SetUsingAbsoluteRotation(true);
        CameraBoom->bUsePawnControlRotation = false;
        CameraBoom->bInheritPitch = false;
        CameraBoom->bInheritYaw = false;
        CameraBoom->bInheritRoll = false;

        const float SafeMinimumDistance = FMath::Max(
            MinimumCameraDistance,
            0.0f
        );
        const float SafeMaximumDistance = FMath::Max(
            MaximumCameraDistance,
            SafeMinimumDistance
        );

        CameraBoom->TargetArmLength = FMath::Clamp(
            DefaultCameraDistance,
            SafeMinimumDistance,
            SafeMaximumDistance
        );
        CameraBoom->SetRelativeRotation(
            FRotator(-90.0f, 0.0f, 0.0f)
        );
        CameraBoom->SocketOffset = CameraSocketOffset;
        UpdateCameraFollowOffset();
        CameraBoom->bEnableCameraLag = bEnableCameraLag;
        CameraBoom->CameraLagSpeed = CameraLagSpeed;
        CameraBoom->CameraLagMaxDistance = CameraLagMaxDistance;
        CameraBoom->bEnableCameraRotationLag =
            bEnableCameraRotationLag;
        CameraBoom->CameraRotationLagSpeed =
            CameraRotationLagSpeed;
        CameraBoom->bDoCollisionTest = false;
    }

}

void ACMPawn::UpdateCameraFollowOffset()
{
    if (!CameraBoom || !BodyMesh)
    {
        return;
    }

    CameraBoom->TargetOffset = FVector(
        -CameraFollowBackOffset,
        0.0f,
        0.0f
    );
}

void ACMPawn::UpdateControlAssignmentMarkers(float DeltaTime)
{
    if (GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    TArray<const ACMPlayerState*> ControlOwners;
    ControlOwners.Init(nullptr, CMControl::MaxControlParts);
    TArray<int32> ControlSlotIndices;
    ControlSlotIndices.Init(INDEX_NONE, CMControl::MaxControlParts);

    const ACMGameState* GameState = GetWorld()
        ? GetWorld()->GetGameState<ACMGameState>()
        : nullptr;
    if (GameState)
    {
        for (APlayerState* RosterPlayerState
            : GameState->PlayerArray)
        {
            const ACMPlayerState* CMPlayerState =
                Cast<ACMPlayerState>(RosterPlayerState);
            if (!CMPlayerState
                || CMPlayerState->IsOnlyASpectator())
            {
                continue;
            }

            for (int32 SlotIndex = 0;
                SlotIndex < CMPlayerState->AssignedControlParts.Num();
                ++SlotIndex)
            {
                const ECMControlPart ControlPart =
                    CMPlayerState->AssignedControlParts[SlotIndex];
                if (CMControl::IsValidPart(ControlPart))
                {
                    const int32 ControlIndex =
                        static_cast<uint8>(ControlPart);
                    ControlOwners[ControlIndex] = CMPlayerState;
                    ControlSlotIndices[ControlIndex] = SlotIndex;
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
        const int32 SegmentIndex = MarkerIndex / 2;
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
        const bool bIsPressed = (PressedControlPartMask
            & static_cast<uint8>(1u << MarkerIndex)) != 0;
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

float ACMPawn::GetPlayerCountSpeedMultiplier() const
{
    const ACMGameState* GameState = GetWorld()
        ? GetWorld()->GetGameState<ACMGameState>()
        : nullptr;
    const int32 PlayerCount = FMath::Clamp(
        GameState ? GameState->GetLobbyPlayerCount() : 1,
        1,
        CMControl::MaxPlayers
    );

    float TargetSeconds = OnePlayerTurnTargetSeconds;
    if (PlayerCount == 2)
    {
        TargetSeconds = TwoPlayerTurnTargetSeconds;
    }
    else if (PlayerCount == 3)
    {
        TargetSeconds = ThreePlayerTurnTargetSeconds;
    }
    else if (PlayerCount >= 4)
    {
        TargetSeconds = FourPlayerTurnTargetSeconds;
    }

    return FMath::Max(OnePlayerTurnTargetSeconds, 0.1f)
        / FMath::Max(TargetSeconds, 0.1f);
}

float ACMPawn::GetPerControlImpulseMultiplier() const
{
    const ACMGameState* GameState = GetWorld()
        ? GetWorld()->GetGameState<ACMGameState>()
        : nullptr;
    int32 AssignedControlCount = 0;
    if (GameState)
    {
        for (APlayerState* RosterPlayerState
            : GameState->PlayerArray)
        {
            const ACMPlayerState* CMPlayerState =
                Cast<ACMPlayerState>(RosterPlayerState);
            if (CMPlayerState
                && !CMPlayerState->IsOnlyASpectator())
            {
                AssignedControlCount +=
                    CMPlayerState->GetAssignedControlCount();
            }
        }
    }

    constexpr float SinglePlayerControlCount = 4.0f;
    return GetPlayerCountSpeedMultiplier()
        * SinglePlayerControlCount
        / FMath::Max(static_cast<float>(AssignedControlCount), 1.0f);
}

void ACMPawn::ConfigureSegments()
{
    ApplyBlueprintSettings();

    for (int32 Index = 0; Index < BodySegments.Num(); ++Index)
    {
        UStaticMeshComponent* SegmentBody = BodySegments[Index];
        const bool bIsActive = Index < ActiveSegmentCount;

        if (!SegmentBody)
        {
            continue;
        }

        if (Index > 0 && !SegmentBody->GetStaticMesh())
        {
            SegmentBody->SetStaticMesh(BodyMesh->GetStaticMesh());
        }

        SegmentBody->SetWorldScale3D(FVector(SegmentScale));

        SegmentBody->SetHiddenInGame(!bIsActive);
        SegmentBody->SetCollisionEnabled(
            bIsActive
                ? ECollisionEnabled::QueryAndPhysics
                : ECollisionEnabled::NoCollision
        );
        SegmentBody->SetSimulatePhysics(bIsActive);
        ConfigureBodyRotationLock(SegmentBody);
    }

    for (int32 Index = 0; Index < SegmentConstraints.Num(); ++Index)
    {
        UPhysicsConstraintComponent* Constraint =
            SegmentConstraints[Index];
        const bool bIsActive = Index < ActiveSegmentCount - 1;

        if (!Constraint)
        {
            continue;
        }

        Constraint->SetActive(bIsActive);
        Constraint->SetComponentTickEnabled(bIsActive);
        Constraint->SetDisableCollision(
            bDisableCollisionBetweenSegments
        );

        if (!bIsActive)
        {
            Constraint->BreakConstraint();
            continue;
        }

        UStaticMeshComponent* FrontBody = BodySegments[Index];
        UStaticMeshComponent* RearBody = BodySegments[Index + 1];

        const FVector RearLocation =
            FrontBody->GetComponentLocation()
            - FrontBody->GetForwardVector()
                * SegmentSpacing * SegmentScale;
        RearBody->SetWorldLocationAndRotation(
            RearLocation,
            FrontBody->GetComponentQuat()
        );

        Constraint->SetWorldLocation(
            (FrontBody->GetComponentLocation()
                + RearBody->GetComponentLocation()) * 0.5f
        );
        Constraint->SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
        Constraint->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
        Constraint->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
        Constraint->SetAngularSwing1Limit(
            EAngularConstraintMotion::ACM_Limited,
            SwingLimitDegrees
        );
        Constraint->SetAngularSwing2Limit(
            EAngularConstraintMotion::ACM_Limited,
            SwingLimitDegrees
        );
        Constraint->SetAngularTwistLimit(
            EAngularConstraintMotion::ACM_Limited,
            TwistLimitDegrees
        );
        Constraint->SetConstrainedComponents(
            FrontBody,
            NAME_None,
            RearBody,
            NAME_None
        );
    }
}

void ACMPawn::ConfigureBodyRotationLock(
    UStaticMeshComponent* SegmentBody
)
{
    if (!SegmentBody)
    {
        return;
    }

    FBodyInstance& BodyInstance = SegmentBody->BodyInstance;
    BodyInstance.bLockXRotation = bLockBodyUpright;
    BodyInstance.bLockYRotation = bLockBodyUpright;
    BodyInstance.bLockZRotation = false;
    BodyInstance.SetDOFLock(EDOFMode::SixDOF);
}

void ACMPawn::ConfigureNetworkPhysics()
{
    if (HasAuthority())
    {
        UpdateReplicatedSegmentStates();
        return;
    }

    for (UPhysicsConstraintComponent* Constraint : SegmentConstraints)
    {
        if (Constraint)
        {
            Constraint->SetActive(false);
            Constraint->SetComponentTickEnabled(false);
        }
    }

    for (int32 Index = 1; Index < BodySegments.Num(); ++Index)
    {
        UStaticMeshComponent* SegmentBody = BodySegments[Index];
        if (SegmentBody)
        {
            SegmentBody->SetSimulatePhysics(false);
            SegmentBody->SetCollisionEnabled(
                Index < ActiveSegmentCount
                    ? ECollisionEnabled::QueryOnly
                    : ECollisionEnabled::NoCollision
            );
        }
    }
}

void ACMPawn::UpdateReplicatedSegmentStates()
{
    const int32 ReplicatedSegmentCount = FMath::Max(
        0,
        FMath::Min(ActiveSegmentCount, BodySegments.Num()) - 1
    );
    ReplicatedSegmentStates.SetNum(ReplicatedSegmentCount);

    for (int32 StateIndex = 0;
        StateIndex < ReplicatedSegmentCount;
        ++StateIndex)
    {
        const UStaticMeshComponent* SegmentBody =
            BodySegments[StateIndex + 1];
        if (!SegmentBody)
        {
            continue;
        }

        ReplicatedSegmentStates[StateIndex].Location =
            SegmentBody->GetComponentLocation();
        ReplicatedSegmentStates[StateIndex].Rotation =
            SegmentBody->GetComponentRotation();
    }
}

void ACMPawn::ApplyReplicatedSegmentStates(float DeltaTime)
{
    if (!bHasReceivedSegmentStates)
    {
        return;
    }

    constexpr float SmoothingSpeed = 15.0f;
    constexpr float TeleportDistance = 500.0f;

    for (int32 StateIndex = 0;
        StateIndex < ReplicatedSegmentStates.Num();
        ++StateIndex)
    {
        const int32 SegmentIndex = StateIndex + 1;
        if (!BodySegments.IsValidIndex(SegmentIndex)
            || !BodySegments[SegmentIndex])
        {
            continue;
        }

        UStaticMeshComponent* SegmentBody = BodySegments[SegmentIndex];
        const FCMReplicatedSegmentState& TargetState =
            ReplicatedSegmentStates[StateIndex];
        const FVector TargetLocation = TargetState.Location;
        const FVector CurrentLocation = SegmentBody->GetComponentLocation();

        const bool bShouldTeleport = FVector::DistSquared(
            CurrentLocation,
            TargetLocation
        ) > FMath::Square(TeleportDistance);

        const FVector NewLocation = bShouldTeleport
            ? TargetLocation
            : FMath::VInterpTo(
                CurrentLocation,
                TargetLocation,
                DeltaTime,
                SmoothingSpeed
            );
        const FRotator NewRotation = bShouldTeleport
            ? TargetState.Rotation
            : FMath::RInterpTo(
                SegmentBody->GetComponentRotation(),
                TargetState.Rotation,
                DeltaTime,
                SmoothingSpeed
            );

        SegmentBody->SetWorldLocationAndRotation(
            NewLocation,
            NewRotation,
            false,
            nullptr,
            ETeleportType::TeleportPhysics
        );
    }
}

void ACMPawn::OnRep_SegmentStates()
{
    bHasReceivedSegmentStates = true;
}

bool ACMPawn::TraceGround(
    USceneComponent* FootPoint,
    FHitResult& OutHit
) const
{
    if (!FootPoint)
    {
        return false;
    }

    const FVector Start = FootPoint->GetComponentLocation();
    const FVector End =
        Start - FVector::UpVector * GroundContactDistance;

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);

    TArray<FHitResult> Hits;
    GetWorld()->SweepMultiByChannel(
        Hits,
        Start,
        End,
        FQuat::Identity,
        GroundTraceChannel,
        FCollisionShape::MakeSphere(GroundCheckRadius),
        QueryParams
    );

    bool bHasGroundContact = false;
    for (const FHitResult& Hit : Hits)
    {
        if (Hit.ImpactNormal.Z >= MinimumGroundNormalZ)
        {
            OutHit = Hit;
            bHasGroundContact = true;
            break;
        }
    }

    if (bDrawGroundContactDebug)
    {
        DrawDebugSphere(
            GetWorld(),
            End,
            GroundCheckRadius,
            12,
            bHasGroundContact ? FColor::Green : FColor::Red,
            false,
            0.5f,
            0,
            1.5f
        );
    }

    return bHasGroundContact;
}
