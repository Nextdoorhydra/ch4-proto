#include "CMChimera.h"

#include "Ability/CMChimeraAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Movement/CMLineBodyMovementCoordinator.h"
#include "Player/CMPartSlotComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Engine/DataTable.h"
#include "Net/UnrealNetwork.h"

// LineBody 데이터 흐름과 상태 변화만 모아 볼 수 있는 전용 로그 카테고리다.
// 콘솔에서 `Log LogChimeraLineBody Verbose`로 상세 로그를 켤 수 있다.
DEFINE_LOG_CATEGORY(LogChimeraLineBody);

ACMChimera::ACMChimera()
{
    constexpr int32 MaxSegmentCount = CMControl::MaxSegments;
    const FVector RearLeftSlotLocation(-40.0f, -60.0f, -30.0f);
    const FVector RearRightSlotLocation(-40.0f, 60.0f, -30.0f);
    const FVector FrontLeftSlotLocation(40.0f, -60.0f, -30.0f);
    const FVector FrontRightSlotLocation(40.0f, 60.0f, -30.0f);

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

    AbilitySystemComponent = CreateDefaultSubobject<
        UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    // 현재는 공용 ASC의 모든 GameplayEffect/Tag 상태를 모든 참가자가 확인해야 하므로
    // Full 모드를 사용한다. 최적화가 필요해지면 Mixed 모드를 별도로 검토한다.
    AbilitySystemComponent->SetReplicationMode(
        EGameplayEffectReplicationMode::Full
    );

    AttributeSet = CreateDefaultSubobject<UCMChimeraAttributeSet>(
        TEXT("ChimeraAttributeSet")
    );

    MovementCoordinator =
        CreateDefaultSubobject<UCMLineBodyMovementCoordinator>(
            TEXT("LineBodyMovementCoordinator")
        );

    BodyDataTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(
        TEXT("/Game/Chimera/Data/Body/DT_BodyDataTable.DT_BodyDataTable")
    ));

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

    LeftFootPoint = CreateDefaultSubobject<UCMPartSlotComponent>(
        TEXT("LeftFootPoint")
    );
    LeftFootPoint->SetupAttachment(BodyMesh);
    LeftFootPoint->SetRelativeLocation(RearLeftSlotLocation);
    LeftFootPoint->InitializeSlotAddress(0, 0);

    RightFootPoint = CreateDefaultSubobject<UCMPartSlotComponent>(
        TEXT("RightFootPoint")
    );
    RightFootPoint->SetupAttachment(BodyMesh);
    RightFootPoint->SetRelativeLocation(RearRightSlotLocation);
    RightFootPoint->InitializeSlotAddress(0, 1);

    UCMPartSlotComponent* FirstLeftUpperPartSlot =
        CreateDefaultSubobject<UCMPartSlotComponent>(
            TEXT("PartSlot_0_2")
        );
    FirstLeftUpperPartSlot->SetupAttachment(BodyMesh);
    FirstLeftUpperPartSlot->SetRelativeLocation(FrontLeftSlotLocation);
    FirstLeftUpperPartSlot->InitializeSlotAddress(0, 2);

    UCMPartSlotComponent* FirstRightUpperPartSlot =
        CreateDefaultSubobject<UCMPartSlotComponent>(
            TEXT("PartSlot_0_3")
        );
    FirstRightUpperPartSlot->SetupAttachment(BodyMesh);
    FirstRightUpperPartSlot->SetRelativeLocation(FrontRightSlotLocation);
    FirstRightUpperPartSlot->InitializeSlotAddress(0, 3);

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(
        TEXT("CameraBoom")
    );
    CameraBoom->SetupAttachment(BodyMesh);
    CameraBoom->SetUsingAbsoluteRotation(true);
    CameraBoom->TargetArmLength = DefaultCameraDistance;
    CameraBoom->SetRelativeRotation(
        FRotator(InitialCameraPitch, 0.0f, 0.0f)
    );
    CameraBoom->SocketOffset = CameraSocketOffset;
    CameraBoom->TargetOffset = FVector(
        -CameraFollowBackOffset,
        0.0f,
        0.0f
    );
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
    PartSlotPoints.Add(LeftFootPoint);
    PartSlotPoints.Add(RightFootPoint);
    PartSlotPoints.Add(FirstLeftUpperPartSlot);
    PartSlotPoints.Add(FirstRightUpperPartSlot);

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
        // 생성자에서는 물리를 시작하지 않는다. StaticMesh와 활성 마디 수가 확정된 뒤
        // ConfigureSegments에서 서버 물리 Body를 한 번에 활성화한다.
        SegmentBody->SetSimulatePhysics(false);
        SegmentBody->SetEnableGravity(bEnableBodyGravity);
        SegmentBody->SetLinearDamping(BodyLinearDamping);
        SegmentBody->SetAngularDamping(BodyAngularDamping);
        SegmentBody->SetCollisionProfileName(BodyCollisionProfile);

        UCMPartSlotComponent* SegmentLeftFoot =
            CreateDefaultSubobject<UCMPartSlotComponent>(
                *FString::Printf(TEXT("LeftFootPoint_%d"), Index + 1)
            );
        SegmentLeftFoot->SetupAttachment(SegmentBody);
        SegmentLeftFoot->SetRelativeLocation(RearLeftSlotLocation);
        SegmentLeftFoot->InitializeSlotAddress(Index, 0);

        UCMPartSlotComponent* SegmentRightFoot =
            CreateDefaultSubobject<UCMPartSlotComponent>(
                *FString::Printf(TEXT("RightFootPoint_%d"), Index + 1)
            );
        SegmentRightFoot->SetupAttachment(SegmentBody);
        SegmentRightFoot->SetRelativeLocation(RearRightSlotLocation);
        SegmentRightFoot->InitializeSlotAddress(Index, 1);

        UCMPartSlotComponent* SegmentLeftUpperPartSlot =
            CreateDefaultSubobject<UCMPartSlotComponent>(
                *FString::Printf(
                    TEXT("PartSlot_%d_2"),
                    Index
                )
            );
        SegmentLeftUpperPartSlot->SetupAttachment(SegmentBody);
        SegmentLeftUpperPartSlot->SetRelativeLocation(FrontLeftSlotLocation);
        SegmentLeftUpperPartSlot->InitializeSlotAddress(Index, 2);

        UCMPartSlotComponent* SegmentRightUpperPartSlot =
            CreateDefaultSubobject<UCMPartSlotComponent>(
                *FString::Printf(
                    TEXT("PartSlot_%d_3"),
                    Index
                )
            );
        SegmentRightUpperPartSlot->SetupAttachment(SegmentBody);
        SegmentRightUpperPartSlot->SetRelativeLocation(FrontRightSlotLocation);
        SegmentRightUpperPartSlot->InitializeSlotAddress(Index, 3);

        BodySegments.Add(SegmentBody);
        LeftFootPoints.Add(SegmentLeftFoot);
        RightFootPoints.Add(SegmentRightFoot);
        PartSlotPoints.Add(SegmentLeftFoot);
        PartSlotPoints.Add(SegmentRightFoot);
        PartSlotPoints.Add(SegmentLeftUpperPartSlot);
        PartSlotPoints.Add(SegmentRightUpperPartSlot);
    }

    for (int32 PartSlotFlatIndex = 0;
        PartSlotFlatIndex < CMControl::MaxPartSlots;
        ++PartSlotFlatIndex)
    {
        UCMPartSlotComponent* PartSlotPoint =
            PartSlotPoints[PartSlotFlatIndex];

        UStaticMeshComponent* Marker =
            CreateDefaultSubobject<UStaticMeshComponent>(
                *FString::Printf(
                    TEXT("ControlAssignmentMarker_%d"),
                    PartSlotFlatIndex + 1
                )
            );
        Marker->SetupAttachment(PartSlotPoint);
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
                    PartSlotFlatIndex + 1
                )
            );
        MarkerText->SetupAttachment(PartSlotPoint);
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

void ACMChimera::BeginPlay()
{
    Super::BeginPlay();

    // ASC의 OwnerActor와 AvatarActor는 모두 Shared Chimera Pawn이다.
    // 플레이어는 이 Pawn을 각자 소유하지 않고, ControlBody를 통해 입력 권한만 전달한다.
    AbilitySystemComponent->InitAbilityActorInfo(this, this);

    // 순서가 중요하다.
    // 1) CSV/DT 값을 ASC·마디 체력·물리 변수로 분배하고
    // 2) 그 물리 변수로 실제 마디 컴포넌트와 Constraint를 설정한다.
    InitializeFromBodyData();
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

void ACMChimera::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplyBlueprintSettings();
}

void ACMChimera::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // TargetOffset은 설정 변경 시에만 갱신하면 되어 Tick에서 다시 쓸 필요가 없다.
    UpdateControlAssignmentMarkers(DeltaTime);

    if (!HasAuthority())
    {
        ApplyReplicatedSegmentStates(DeltaTime);
        return;
    }

    if (MovementCoordinator)
    {
        MovementCoordinator->UpdateServerMovement(*this);
    }

    UpdateReplicatedSegmentStates();
}

// 서버에서 전후 Force와 좌우 Yaw Torque를 적용해 충돌 가능한 테스트 이동 제공
void ACMChimera::ApplyDebugMovementInput(
    float ForwardInput,
    float TurnInput)
{
#if !UE_BUILD_SHIPPING
    if (!HasAuthority() || !BodyMesh || !BodyMesh->IsSimulatingPhysics())
    {
        return;
    }

    FVector ForwardDirection = BodyMesh->GetForwardVector();
    ForwardDirection.Z = 0.0f;
    ForwardDirection.Normalize();

    if (!FMath::IsNearlyZero(ForwardInput))
    {
        BodyMesh->AddForce(
            ForwardDirection * ForwardInput * DebugMovementForce);
    }
    if (!FMath::IsNearlyZero(TurnInput))
    {
        BodyMesh->AddTorqueInRadians(
            FVector::UpVector * TurnInput * DebugTurnTorque);
    }
#endif
}

void ACMChimera::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACMChimera, ReplicatedSegmentStates);
    DOREPLIFETIME(ACMChimera, SegmentHealthStates);
    DOREPLIFETIME(ACMChimera, PressedPartSlotMask);
    DOREPLIFETIME(ACMChimera, ActiveSegmentCount);
}

UAbilitySystemComponent* ACMChimera::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void ACMChimera::SetActiveSegmentCountForPlayers(int32 PlayerCount)
{
    if (!HasAuthority())
    {
        return;
    }

    const int32 NewSegmentCount = FMath::Clamp(
        PlayerCount,
        1,
        CMControl::MaxSegments
    );
    if (ActiveSegmentCount == NewSegmentCount
        && SegmentHealthStates.Num() == NewSegmentCount)
    {
        return;
    }

    const int32 OldSegmentCount = ActiveSegmentCount;
    ActiveSegmentCount = NewSegmentCount;

    if (ConfiguredSegmentMaxHealth > 0.0f)
    {
        const int32 PreviousHealthStateCount =
            SegmentHealthStates.Num();
        SegmentHealthStates.SetNum(NewSegmentCount);
        for (int32 SegmentIndex = PreviousHealthStateCount;
            SegmentIndex < NewSegmentCount;
            ++SegmentIndex)
        {
            FCMBodySegmentHealthState& SegmentState =
                SegmentHealthStates[SegmentIndex];
            SegmentState.SegmentIndex = SegmentIndex;
            SegmentState.Health = ConfiguredSegmentMaxHealth;
            SegmentState.MaxHealth = ConfiguredSegmentMaxHealth;
            SegmentState.bDead = false;
        }
    }

    bAllSegmentsDeathNotified = false;
    ConfigureSegments();
    ConfigureNetworkPhysics();
    ForceNetUpdate();

    UE_LOG(LogChimeraLineBody, Log,
        TEXT("[Player Count -> Segments] Active segments changed %d -> %d. PartSlots=%d"),
        OldSegmentCount,
        ActiveSegmentCount,
        ActiveSegmentCount * CMControl::PartSlotsPerSegment);
}
int32 ACMChimera::GetActiveSegmentCount() const
{
    return ActiveSegmentCount;
}

// 파츠와 ControlBody를 제외하고 활성 BodySegment 컴포넌트만 Volume과 비교
bool ACMChimera::AreAllActiveBodySegmentsOverlapping(
    const UPrimitiveComponent* Volume) const
{
    if (!IsValid(Volume) || ActiveSegmentCount <= 0)
    {
        return false;
    }

    for (int32 SegmentIndex = 0;
        SegmentIndex < ActiveSegmentCount;
        ++SegmentIndex)
    {
        const UStaticMeshComponent* SegmentBody =
            BodySegments.IsValidIndex(SegmentIndex)
                ? BodySegments[SegmentIndex]
                : nullptr;
        if (!IsValid(SegmentBody)
            || !SegmentBody->IsOverlappingComponent(Volume))
        {
            return false;
        }
    }
    return true;
}

void ACMChimera::ApplyBlueprintSettings()
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
        if (BodySegmentMass > 0.0f)
        {
            SegmentBody->SetMassOverrideInKg(
                NAME_None,
                BodySegmentMass,
                true
            );
        }
        if (RuntimeBodyPhysicalMaterial)
        {
            SegmentBody->SetPhysMaterialOverride(
                RuntimeBodyPhysicalMaterial
            );
        }
        SegmentBody->SetWorldScale3D(FVector(SafeSegmentScale));

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
        CameraBoom->TargetArmLength = FMath::Max(
            DefaultCameraDistance,
            0.0f
        );
        CameraBoom->SetRelativeRotation(
            FRotator(InitialCameraPitch, 0.0f, 0.0f)
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

void ACMChimera::UpdateCameraFollowOffset()
{
    if (!CameraBoom)
    {
        return;
    }

    CameraBoom->TargetOffset = FVector(
        -CameraFollowBackOffset,
        0.0f,
        0.0f
    );
}

