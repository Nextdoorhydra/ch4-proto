#include "CMChimera.h"

#include "Ability/CMChimeraAttributeSet.h"
#include "Ability/CMStaminaGameplayEffects.h"
#include "AbilitySystemComponent.h"
#include "Data/Body/CMBodyTableRow.h"
#include "Movement/CMLineBodyMovementCoordinator.h"
#include "Player/CMControlBody.h"
#include "Player/CMDebugPartActor.h"
#include "Player/CMPartSlotComponent.h"
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
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

// LineBody 데이터 흐름과 상태 변화만 모아 볼 수 있는 전용 로그 카테고리다.
// 콘솔에서 `Log LogChimeraLineBody Verbose`로 상세 로그를 켤 수 있다.
DEFINE_LOG_CATEGORY_STATIC(LogChimeraLineBody, Log, All);

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
        TEXT("/Game/Data/Body/DT_BodyDataTable.DT_BodyDataTable")
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

void ACMChimera::ActivatePartSlot(
    const FCMPartSlotAddress& PartSlotAddress,
    ACMPlayerState* ContributingPlayerState
)
{
    if (!HasAuthority()
        || !CMControl::IsValidPartSlot(
            PartSlotAddress,
            ActiveSegmentCount))
    {
        return;
    }

    UCMPartSlotComponent* PartSlot =
        GetPartSlotComponent(PartSlotAddress);
    if (PartSlot && PartSlot->HasAttachedPart())
    {
#if !UE_BUILD_SHIPPING
        ACMDebugPartActor* DebugPart =
            Cast<ACMDebugPartActor>(PartSlot->GetAttachedPart());
        if (DebugPart)
        {
            DebugPart->SetContributingPlayerState(
                ContributingPlayerState
            );
        }
#endif

        const bool bActivated = PartSlot->TryActivateGrantedAbility();
#if !UE_BUILD_SHIPPING
        if (DebugPart && !bActivated)
        {
            DebugPart->ConsumeContributingPlayerState();
        }
#endif
        UE_LOG(LogChimeraLineBody, Log,
            TEXT("[Attached Part Input] Slot=(%d,%d) Part=%s Activated=%s"),
            PartSlotAddress.SegmentIndex,
            PartSlotAddress.PartSlotIndex,
            *GetNameSafe(PartSlot->GetAttachedPart()),
            bActivated ? TEXT("true") : TEXT("false"));
        return;
    }

    UE_LOG(LogChimeraLineBody, Verbose,
        TEXT("[Empty PartSlot] PartSlot=(%d,%d) has no attached Part action."),
        PartSlotAddress.SegmentIndex,
        PartSlotAddress.PartSlotIndex);
}

#if !UE_BUILD_SHIPPING
void ACMChimera::ActivateDebugLegPart(
    const FCMPartSlotAddress& PartSlotAddress,
    ACMPlayerState* ContributingPlayerState
)
{
    if (!HasAuthority()
        || !CMControl::IsValidPartSlot(
            PartSlotAddress,
            ActiveSegmentCount))
    {
        return;
    }

    const int32 SegmentIndex = PartSlotAddress.SegmentIndex;
    UCMPartSlotComponent* PartSlot =
        GetPartSlotComponent(PartSlotAddress);
    if (!PartSlot)
    {
        return;
    }

    // 실제 힘 계산과 협동 보너스 집계는 기존 이동 코드에 그대로 위임한다.
    const float SafeStaminaCost = FMath::Max(LegStaminaCost, 0.0f);
    if (AttributeSet->GetStamina() < SafeStaminaCost)
    {
        UE_LOG(LogChimeraLineBody, Log,
            TEXT("[Input Rejected] Not enough shared Stamina. Required=%.1f Current=%.1f PartSlot=(%d,%d)"),
            SafeStaminaCost,
            AttributeSet->GetStamina(),
            PartSlotAddress.SegmentIndex,
            PartSlotAddress.PartSlotIndex);
        return;
    }

    // Cost is paid only after the coordinator confirms that the foot was
    // grounded and the physical action actually happened.
    const bool bActionSucceeded = MovementCoordinator
        && MovementCoordinator->TryActivateLeg(
            *this,
            SegmentIndex,
            PartSlot,
            ContributingPlayerState
        );
    if (bActionSucceeded)
    {
        ApplyStaminaCost(SafeStaminaCost);
    }
}
#endif

UCMPartSlotComponent* ACMChimera::GetPartSlotComponent(
    const FCMPartSlotAddress& PartSlotAddress
) const
{
    if (!CMControl::IsValidPartSlot(
        PartSlotAddress,
        ActiveSegmentCount))
    {
        return nullptr;
    }

    const int32 FlatIndex =
        CMControl::ToFlatPartSlotIndex(PartSlotAddress);
    return PartSlotPoints.IsValidIndex(FlatIndex)
        ? PartSlotPoints[FlatIndex]
        : nullptr;
}

bool ACMChimera::AttachPartToSlot(
    const FCMPartSlotAddress& PartSlotAddress,
    AActor* PartActor
)
{
    if (!HasAuthority())
    {
        return false;
    }

    UCMPartSlotComponent* PartSlot =
        GetPartSlotComponent(PartSlotAddress);
    return PartSlot && PartSlot->AttachPart(PartActor);
}

AActor* ACMChimera::DetachPartFromSlot(
    const FCMPartSlotAddress& PartSlotAddress
)
{
    if (!HasAuthority())
    {
        return nullptr;
    }

    UCMPartSlotComponent* PartSlot =
        GetPartSlotComponent(PartSlotAddress);
    return PartSlot ? PartSlot->DetachPart() : nullptr;
}

#if !UE_BUILD_SHIPPING
void ACMChimera::SpawnRandomDebugParts()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    ClearRandomDebugParts();

    static const ECMPartSlotType DebugPartTypes[] =
    {
        ECMPartSlotType::Head,
        ECMPartSlotType::Arm,
        ECMPartSlotType::Leg
    };

    int32 AttachedCount = 0;
    const int32 ActiveSlotCount =
        ActiveSegmentCount * CMControl::PartSlotsPerSegment;
    for (int32 FlatIndex = 0; FlatIndex < ActiveSlotCount; ++FlatIndex)
    {
        if (!PartSlotPoints.IsValidIndex(FlatIndex)
            || !PartSlotPoints[FlatIndex]
            || PartSlotPoints[FlatIndex]->HasAttachedPart())
        {
            continue;
        }

        FActorSpawnParameters SpawnParameters;
        SpawnParameters.Owner = this;
        SpawnParameters.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ACMDebugPartActor* DebugPart = GetWorld()->SpawnActor<
            ACMDebugPartActor>(
                GetActorLocation(),
                FRotator::ZeroRotator,
                SpawnParameters
            );
        if (!DebugPart)
        {
            continue;
        }

        DebugPart->InitializeDebugPart(
            DebugPartTypes[FMath::RandHelper(UE_ARRAY_COUNT(DebugPartTypes))]
        );
        if (PartSlotPoints[FlatIndex]->AttachPart(DebugPart))
        {
            ++AttachedCount;
        }
        else
        {
            DebugPart->Destroy();
        }
    }

    UE_LOG(LogChimeraLineBody, Warning,
        TEXT("[Debug Parts Ready] Attached %d random Head/Arm/Leg Parts. Press Q/W/E/R to verify slot-to-GA routing."),
        AttachedCount);
}

void ACMChimera::ClearRandomDebugParts()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    TArray<ACMDebugPartActor*> DebugParts;
    for (TActorIterator<ACMDebugPartActor> It(GetWorld()); It; ++It)
    {
        DebugParts.Add(*It);
    }

    int32 RemovedCount = 0;
    for (ACMDebugPartActor* DebugPart : DebugParts)
    {
        if (!IsValid(DebugPart))
        {
            continue;
        }

        for (UCMPartSlotComponent* PartSlot : PartSlotPoints)
        {
            if (PartSlot && PartSlot->GetAttachedPart() == DebugPart)
            {
                PartSlot->DetachPart();
                break;
            }
        }

        DebugPart->Destroy();
        ++RemovedCount;
    }

    UE_LOG(LogChimeraLineBody, Warning,
        TEXT("[Debug Parts Cleared] Removed %d diagnostic Parts."),
        RemovedCount);
}
#endif

void ACMChimera::SetPartSlotPressed(
    const FCMPartSlotAddress& PartSlotAddress,
    bool bPressed
)
{
    if (!HasAuthority()
        || !CMControl::IsValidPartSlot(
            PartSlotAddress,
            ActiveSegmentCount))
    {
        return;
    }

    const uint32 PartSlotBit = 1u
        << CMControl::ToFlatPartSlotIndex(PartSlotAddress);
    if (bPressed)
    {
        PressedPartSlotMask |= PartSlotBit;
    }
    else
    {
        PressedPartSlotMask &= ~PartSlotBit;
    }

    ForceNetUpdate();
}

void ACMChimera::ClearPressedControlParts()
{
    if (!HasAuthority())
    {
        return;
    }

    const bool bHadPressedPart = PressedPartSlotMask != 0;
    PressedPartSlotMask = 0;

    // 전체 사망·재할당·게임 종료에서도 각 플레이어 Pawn 안에 과거 Press가
    // 남지 않도록 Shared 상태와 ControlBody 상태를 같은 시점에 초기화한다.
    for (TActorIterator<ACMControlBody> It(GetWorld()); It; ++It)
    {
        It->ClearPressedControlSlots();
    }

    if (bHadPressedPart)
    {
        ForceNetUpdate();
    }
}

void ACMChimera::ApplyDamageToSegment(
    int32 SegmentIndex,
    float Damage
)
{
    if (!HasAuthority()
        || !SegmentHealthStates.IsValidIndex(SegmentIndex)
        || Damage <= 0.0f)
    {
        UE_LOG(LogChimeraLineBody, Verbose,
            TEXT("[Segment Damage Rejected] Authority=%s Segment=%d Damage=%.1f"),
            HasAuthority() ? TEXT("true") : TEXT("false"),
            SegmentIndex,
            Damage);
        return;
    }

    FCMBodySegmentHealthState& SegmentState =
        SegmentHealthStates[SegmentIndex];
    if (SegmentState.bDead)
    {
        UE_LOG(LogChimeraLineBody, Verbose,
            TEXT("[Segment Damage Ignored] Segment=%d is already dead."),
            SegmentIndex);
        return;
    }

    const float OldHealth = SegmentState.Health;
    SegmentState.Health = FMath::Max(
        SegmentState.Health - Damage,
        0.0f
    );
    SegmentState.bDead = SegmentState.Health <= 0.0f;

    UE_LOG(LogChimeraLineBody, Log,
        TEXT("[Segment Damage] Segment=%d Damage=%.1f Health=%.1f -> %.1f"),
        SegmentIndex, Damage, OldHealth, SegmentState.Health);

    if (SegmentState.bDead)
    {
        // Segment HP는 그 Segment를 소유한 플레이어의 생존 여부만 결정한다.
        // PartSlot 소유권은 여러 Segment에 섞여 있으므로, 죽은 Segment 위의
        // 슬롯을 다른 플레이어가 계속 조작할 수 있게 전역 슬롯 상태는 지우지 않는다.
        for (TActorIterator<ACMControlBody> It(GetWorld()); It; ++It)
        {
            It->HandleSegmentDestroyed(SegmentIndex);
        }

        UE_LOG(LogChimeraLineBody, Warning,
            TEXT("[Segment Death] Segment=%d died; only its owning player's Q/W/E/R input was disabled. PartSlots remain usable."),
            SegmentIndex);

        // The owning ControlBody was disabled above. This delegate is the
        // external handoff for PlayerState/GameMode/UI systems that also need
        // to record or display the individual player's defeat.
        OnSegmentDestroyed.Broadcast(SegmentIndex);

        if (!bAllSegmentsDeathNotified && AreAllSegmentsDead())
        {
            bAllSegmentsDeathNotified = true;
            ClearPressedControlParts();

            UE_LOG(LogChimeraLineBody, Error,
                TEXT("[All Segments Dead] Every active segment is dead. Broadcasting OnAllSegmentsDead once on the server."));
            OnAllSegmentsDead.Broadcast();
        }
    }

    ForceNetUpdate();
}

bool ACMChimera::IsSegmentAlive(int32 SegmentIndex) const
{
    return SegmentHealthStates.IsValidIndex(SegmentIndex)
        && !SegmentHealthStates[SegmentIndex].bDead;
}

TArray<FCMBodySegmentHealthState> ACMChimera::GetSegmentHealthStates() const
{
    return SegmentHealthStates;
}

bool ACMChimera::AreAllSegmentsDead() const
{
    if (SegmentHealthStates.IsEmpty())
    {
        return false;
    }

    for (const FCMBodySegmentHealthState& SegmentState
        : SegmentHealthStates)
    {
        if (!SegmentState.bDead)
        {
            return false;
        }
    }

    return true;
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

bool ACMChimera::InitializeFromBodyData()
{
    UE_LOG(LogChimeraLineBody, Verbose,
        TEXT("[Data Load] Loading table=%s row=%s"),
        *BodyDataTable.ToSoftObjectPath().ToString(),
        *BodyRowName.ToString());

    UDataTable* LoadedBodyDataTable = BodyDataTable.LoadSynchronous();
    if (!LoadedBodyDataTable)
    {
        UE_LOG(LogChimeraLineBody, Error,
            TEXT("[Data Load Failed] Body DataTable could not be loaded: %s"),
            *BodyDataTable.ToSoftObjectPath().ToString());
        return false;
    }

    const FCMBodyTableRow* BodyRow =
        LoadedBodyDataTable->FindRow<FCMBodyTableRow>(
            BodyRowName,
            TEXT("ACMChimera::InitializeFromBodyData")
        );
    if (!BodyRow)
    {
        UE_LOG(LogChimeraLineBody, Error,
            TEXT("[Data Load Failed] Body row was not found: %s"),
            *BodyRowName.ToString());
        return false;
    }

    if (BodyRow->BodyType != TEXT("Line"))
    {
        UE_LOG(LogChimeraLineBody, Warning,
            TEXT("[Layout Mismatch] ACMChimera uses the Line layout, but row %s has BodyType %s."),
            *BodyRowName.ToString(),
            *BodyRow->BodyType.ToString());
    }

    // CSV의 물리 열은 먼저 Pawn의 런타임 튜닝 값으로 옮긴다.
    // 바로 뒤의 ConfigureSegments()가 이 값을 각 Chaos 컴포넌트에 실제 적용한다.
    BodySegmentMass = BodyRow->SegmentMass;
    BodyGroundFriction = BodyRow->GroundFriction;
    BodyLinearDamping = BodyRow->LinearDamping;
    BodyAngularDamping = BodyRow->AngularDamping;
    MaxSpeed = BodyRow->MaxVelocity;

    RuntimeBodyPhysicalMaterial = NewObject<UPhysicalMaterial>(this);
    RuntimeBodyPhysicalMaterial->Friction = BodyGroundFriction;

    UE_LOG(LogChimeraLineBody, Log,
        TEXT("[CSV -> Physics] Row=%s Type=%s Mass=%.1f Friction=%.2f LinearDamping=%.2f AngularDamping=%.2f MaxVelocity=%.1f"),
        *BodyRowName.ToString(),
        *BodyRow->BodyType.ToString(),
        BodySegmentMass,
        BodyGroundFriction,
        BodyLinearDamping,
        BodyAngularDamping,
        MaxSpeed);

    // 초기 상태를 만드는 주체는 서버 하나뿐이다. 클라이언트는 아래 값들을
    // ASC 및 SegmentHealthStates 복제로 받아 표시/판정에 사용한다.
    if (HasAuthority())
    {
        InitializeSharedAttributes(
            BodyRow->MaxStamina,
            BodyRow->StaminaRegen
        );
        InitializeSegmentHealth(BodyRow->SegmentMaxHP);

        UE_LOG(LogChimeraLineBody, Log,
            TEXT("[LineBody Ready] Row=%s ID=%s SegmentHP=%.1f SharedStamina=%.1f RegenPerSecond=%.1f Segments=%d"),
            *BodyRowName.ToString(),
            *BodyRow->ID.ToString(),
            BodyRow->SegmentMaxHP,
            BodyRow->MaxStamina,
            BodyRow->StaminaRegen,
            SegmentHealthStates.Num()
        );
    }

    return true;
}

void ACMChimera::InitializeSharedAttributes(
    float MaxStamina,
    float StaminaRegen
)
{
    if (!HasAuthority() || !AbilitySystemComponent || !AttributeSet)
    {
        return;
    }

    // Max를 먼저 넣고 Current를 나중에 넣어 PreAttributeChange의 Clamp가
    // 아직 0인 Max 값에 Current를 잘라버리지 않게 한다.
    AbilitySystemComponent->SetNumericAttributeBase(
        UCMChimeraAttributeSet::GetMaxStaminaAttribute(), MaxStamina);
    AbilitySystemComponent->SetNumericAttributeBase(
        UCMChimeraAttributeSet::GetStaminaAttribute(), MaxStamina);
    AbilitySystemComponent->SetNumericAttributeBase(
        UCMChimeraAttributeSet::GetStaminaRegenAttribute(), StaminaRegen);

    UE_LOG(LogChimeraLineBody, Log,
        TEXT("[CSV -> ASC] Stamina=%.1f/%.1f RegenPerSecond=%.1f"),
        AttributeSet->GetStamina(),
        AttributeSet->GetMaxStamina(),
        AttributeSet->GetStaminaRegen());

    StartStaminaRegeneration();
}

void ACMChimera::InitializeSegmentHealth(float SegmentMaxHealth)
{
    if (!HasAuthority())
    {
        return;
    }

    bAllSegmentsDeathNotified = false;
    ConfiguredSegmentMaxHealth = SegmentMaxHealth;

    const int32 SegmentCount = FMath::Clamp(
        ActiveSegmentCount,
        1,
        BodySegments.Num()
    );
    SegmentHealthStates.SetNum(SegmentCount);

    // 모든 마디는 같은 CSV SegmentMaxHP로 시작하지만 이후 피해와 죽음은
    // 배열 원소별로 독립 처리된다. 추후 파츠 효과가 생기면 이 지점에서 보정한다.
    for (int32 SegmentIndex = 0;
        SegmentIndex < SegmentCount;
        ++SegmentIndex)
    {
        FCMBodySegmentHealthState& SegmentState =
            SegmentHealthStates[SegmentIndex];
        SegmentState.SegmentIndex = SegmentIndex;
        SegmentState.Health = SegmentMaxHealth;
        SegmentState.MaxHealth = SegmentMaxHealth;
        SegmentState.bDead = false;
    }

    UE_LOG(LogChimeraLineBody, Log,
        TEXT("[CSV -> Segments] Initialized %d segments with %.1f HP each."),
        SegmentCount, SegmentMaxHealth);

    ForceNetUpdate();
}

void ACMChimera::StartStaminaRegeneration()
{
    if (!HasAuthority()
        || !AbilitySystemComponent
        || !AttributeSet)
    {
        return;
    }

    const FGameplayEffectSpecHandle RegenSpec =
        AbilitySystemComponent->MakeOutgoingSpec(
            UCMStaminaRegenGameplayEffect::StaticClass(),
            1.0f,
            AbilitySystemComponent->MakeEffectContext()
        );
    if (!RegenSpec.IsValid())
    {
        UE_LOG(LogChimeraLineBody, Error,
            TEXT("[GAS Stamina] Failed to create the periodic regeneration GameplayEffect spec."));
        return;
    }

    AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(
        *RegenSpec.Data.Get()
    );

    UE_LOG(LogChimeraLineBody, Log,
        TEXT("[GAS Stamina] Periodic regeneration started. AmountPerSecond=%.1f"),
        AttributeSet->GetStaminaRegen());
}

void ACMChimera::ApplyStaminaCost(float Cost)
{
    if (!HasAuthority()
        || !AbilitySystemComponent
        || !AttributeSet
        || Cost <= 0.0f)
    {
        return;
    }

    const float OldStamina = AttributeSet->GetStamina();
    const FGameplayEffectSpecHandle CostSpec =
        AbilitySystemComponent->MakeOutgoingSpec(
            UCMStaminaCostGameplayEffect::StaticClass(),
            1.0f,
            AbilitySystemComponent->MakeEffectContext()
        );
    if (!CostSpec.IsValid())
    {
        UE_LOG(LogChimeraLineBody, Error,
            TEXT("[GAS Stamina] Failed to create the action-cost GameplayEffect spec."));
        return;
    }

    CostSpec.Data->SetSetByCallerMagnitude(
        UCMStaminaCostGameplayEffect::StaminaCostDataName,
        -Cost
    );
    AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(
        *CostSpec.Data.Get()
    );

    UE_LOG(LogChimeraLineBody, Log,
        TEXT("[GAS Stamina Cost] Cost=%.1f Stamina=%.1f -> %.1f"),
        Cost,
        OldStamina,
        AttributeSet->GetStamina());
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

void ACMChimera::ConfigureSegments()
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
        SegmentBody->SetMobility(EComponentMobility::Movable);
        SegmentBody->SetSimulatePhysics(bIsActive);
        if (bIsActive)
        {
            ConfigureBodyRotationLock(SegmentBody);
        }
    }

    // Constraint는 서버의 Chaos 시뮬레이션에만 필요하다.
    // 모든 활성 마디가 Dynamic Body가 된 뒤 생성해야 both-static 조인트가 생기지 않는다.
    if (!HasAuthority())
    {
        return;
    }

    for (UPhysicsConstraintComponent* ExistingConstraint
        : SegmentConstraints)
    {
        if (ExistingConstraint)
        {
            ExistingConstraint->DestroyComponent();
        }
    }
    SegmentConstraints.Reset();
    SegmentConstraints.Reserve(FMath::Max(ActiveSegmentCount - 1, 0));
    for (int32 Index = 0; Index < ActiveSegmentCount - 1; ++Index)
    {
        UStaticMeshComponent* FrontBody = BodySegments[Index];
        UStaticMeshComponent* RearBody = BodySegments[Index + 1];

        if (!FrontBody
            || !RearBody
            || !FrontBody->IsSimulatingPhysics()
            || !RearBody->IsSimulatingPhysics())
        {
            UE_LOG(LogChimeraLineBody, Error,
                TEXT("[Constraint Skipped] Segment pair %d-%d is not ready for physics. Front=%d Rear=%d"),
                Index,
                Index + 1,
                FrontBody && FrontBody->IsSimulatingPhysics(),
                RearBody && RearBody->IsSimulatingPhysics());
            continue;
        }

        const FVector RearLocation =
            FrontBody->GetComponentLocation()
            - FrontBody->GetForwardVector()
                * SegmentSpacing * SegmentScale;
        RearBody->SetWorldLocationAndRotation(
            RearLocation,
            FrontBody->GetComponentQuat()
        );

        UPhysicsConstraintComponent* Constraint =
            NewObject<UPhysicsConstraintComponent>(
                this,
                MakeUniqueObjectName(
                    this,
                    UPhysicsConstraintComponent::StaticClass(),
                    *FString::Printf(
                        TEXT("SegmentConstraint_%d"),
                        Index
                    )
                )
            );
        if (!Constraint)
        {
            UE_LOG(LogChimeraLineBody, Error,
                TEXT("[Constraint Create Failed] Segment pair %d-%d"),
                Index,
                Index + 1);
            continue;
        }

        Constraint->SetupAttachment(BodyMesh);
        Constraint->ComponentName1.ComponentName = FrontBody->GetFName();
        Constraint->ComponentName2.ComponentName = RearBody->GetFName();
        // BodySegments는 UPROPERTY 배열이라 이름 탐색만으로는 개별 마디를 못 찾을 수 있다.
        // 등록 전에 실제 컴포넌트 포인터를 지정해 RootComponent 폴백을 막는다.
        Constraint->OverrideComponent1 = FrontBody;
        Constraint->OverrideComponent2 = RearBody;
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
        Constraint->SetDisableCollision(
            bDisableCollisionBetweenSegments
        );
        AddInstanceComponent(Constraint);
        Constraint->RegisterComponent();
        SegmentConstraints.Add(Constraint);

        UE_LOG(LogChimeraLineBody, Log,
            TEXT("[Constraint Ready] Segment pair %d-%d Front=%s Rear=%s"),
            Index,
            Index + 1,
            *GetNameSafe(FrontBody),
            *GetNameSafe(RearBody));
    }
}

void ACMChimera::ConfigureBodyRotationLock(
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

void ACMChimera::ConfigureNetworkPhysics()
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

void ACMChimera::UpdateReplicatedSegmentStates()
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

void ACMChimera::ApplyReplicatedSegmentStates(float DeltaTime)
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

void ACMChimera::OnRep_SegmentStates()
{
    bHasReceivedSegmentStates = true;
}

void ACMChimera::OnRep_SegmentHealthStates()
{
    // 이 함수가 클라이언트에서 호출되면 서버의 마디별 체력/사망 상태를
    // 정상적으로 전달받았다는 뜻이다. UI와 사망 연출은 이후 여기서 갱신할 수 있다.
    UE_LOG(LogChimeraLineBody, Log,
        TEXT("[Client Replication] Received %d segment health states."),
        SegmentHealthStates.Num());

    for (const FCMBodySegmentHealthState& SegmentState : SegmentHealthStates)
    {
        UE_LOG(LogChimeraLineBody, Verbose,
            TEXT("[Client Segment] Index=%d Health=%.1f/%.1f Dead=%s"),
            SegmentState.SegmentIndex,
            SegmentState.Health,
            SegmentState.MaxHealth,
            SegmentState.bDead ? TEXT("true") : TEXT("false"));
    }
}

void ACMChimera::OnRep_ActiveSegmentCount()
{
    ConfigureSegments();
    ConfigureNetworkPhysics();

    UE_LOG(LogChimeraLineBody, Log,
        TEXT("[Client Segment Count] ActiveSegments=%d PartSlots=%d"),
        ActiveSegmentCount,
        ActiveSegmentCount * CMControl::PartSlotsPerSegment);
}

