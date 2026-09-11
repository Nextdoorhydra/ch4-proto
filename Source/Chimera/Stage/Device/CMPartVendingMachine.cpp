#include "Stage/Device/CMPartVendingMachine.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Parts/Arm/CMArmPart.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Stage/Device/CMPartVendingMachineWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraPartVendingMachine, Log, All);

ACMPartVendingMachine::ACMPartVendingMachine()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(false);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    MachineMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("MachineMesh"));
    MachineMesh->SetupAttachment(SceneRoot);
    MachineMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

    HitVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("HitVolume"));
    HitVolume->SetupAttachment(SceneRoot);
    HitVolume->SetBoxExtent(FVector(75.0f, 75.0f, 100.0f));
    HitVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    HitVolume->SetCollisionObjectType(ECC_WorldDynamic);
    HitVolume->SetCollisionResponseToAllChannels(ECR_Overlap);
    HitVolume->SetGenerateOverlapEvents(true);
    HitVolume->SetCanEverAffectNavigation(false);

    DispensePoint = CreateDefaultSubobject<USceneComponent>(
        TEXT("DispensePoint"));
    DispensePoint->SetupAttachment(SceneRoot);
    DispensePoint->SetRelativeLocation(FVector(100.0f, 0.0f, 50.0f));

    DisplayWidget = CreateDefaultSubobject<UWidgetComponent>(
        TEXT("DisplayWidget"));
    DisplayWidget->SetupAttachment(SceneRoot);
    DisplayWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 180.0f));
    DisplayWidget->SetWidgetSpace(EWidgetSpace::World);
    DisplayWidget->SetBlendMode(EWidgetBlendMode::Transparent);
    DisplayWidget->SetTwoSided(true);
    DisplayWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DisplayWidget->SetGenerateOverlapEvents(false);
    DisplayWidget->SetWidgetClass(UCMPartVendingMachineWidget::StaticClass());
    DisplayWidget->SetDrawSize(FVector2D(180.0f, 180.0f));
    DisplayWidget->SetPivot(FVector2D(0.5f, 0.5f));
    DisplayWidget->SetBackgroundColor(FLinearColor::Transparent);
    DisplayWidget->SetBoundsScale(2.0f);
}

void ACMPartVendingMachine::BeginPlay()
{
    Super::BeginPlay();

    RemainingUses = FMath::Max(0, RemainingUses);
    DisplayBaseLocation = DisplayWidget->GetRelativeLocation();
    MachineMeshBaseLocation = MachineMesh->GetRelativeLocation();
    HitShakeElapsed = HitShakeDuration;
    DisplayWidget->InitWidget();
    ApplyDisplayPresentation();
    SetActorTickEnabled(GetNetMode() != NM_DedicatedServer);
}

void ACMPartVendingMachine::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UpdateHitShake(DeltaSeconds);

    const float HeightOffset = FMath::Sin(
        GetGameTimeSinceCreation() * DisplayFloatSpeed * 2.0f * PI)
        * DisplayFloatAmplitude;
    DisplayWidget->SetRelativeLocation(
        DisplayBaseLocation + FVector(0.0f, 0.0f, HeightOffset));

    const APlayerController* LocalController =
        UGameplayStatics::GetPlayerController(this, 0);
    const APlayerCameraManager* CameraManager = LocalController
        ? LocalController->PlayerCameraManager
        : nullptr;
    if (CameraManager)
    {
        DisplayWidget->SetWorldRotation(
            (CameraManager->GetCameraLocation()
                - DisplayWidget->GetComponentLocation()).Rotation());
    }
}

void ACMPartVendingMachine::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACMPartVendingMachine, RemainingUses);
}

bool ACMPartVendingMachine::ReceiveCombatHit_Implementation(
    const FCMCombatHitRequest& Request)
{
    if (!HasAuthority()
        || !IsValid(Cast<ACMArmPart>(Request.SourcePart))
        || !Request.AttackId.IsValid())
    {
        return false;
    }
    if (Request.AttackId == LastAcceptedAttackId)
    {
        return true;
    }
    if (RemainingUses <= 0)
    {
        return false;
    }

    ACMPartActorBase* DispensedPart = DispensePart();
    if (!DispensedPart)
    {
        return false;
    }

    LastAcceptedAttackId = Request.AttackId;
    --RemainingUses;
    ApplyDisplayPresentation();
    OnRemainingUsesChanged(RemainingUses);
    ForceNetUpdate();
    MulticastPlayHitShake();
    OnPartDispensed(DispensedPart);
    return true;
}

void ACMPartVendingMachine::MulticastPlayHitShake_Implementation()
{
    if (GetNetMode() != NM_DedicatedServer)
    {
        HitShakeElapsed = 0.0f;
    }
}

void ACMPartVendingMachine::UpdateHitShake(float DeltaSeconds)
{
    if (HitShakeElapsed >= HitShakeDuration || HitShakeDuration <= 0.0f)
    {
        MachineMesh->SetRelativeLocation(MachineMeshBaseLocation);
        return;
    }

    HitShakeElapsed = FMath::Min(
        HitShakeElapsed + DeltaSeconds, HitShakeDuration);
    const float Alpha = HitShakeElapsed / HitShakeDuration;
    const float Envelope = 1.0f - Alpha;
    const float Phase = HitShakeElapsed * HitShakeFrequency * 2.0f * PI;
    const FVector Offset(
        FMath::Sin(Phase) * HitShakeDistance.X,
        FMath::Sin(Phase * 1.37f + HALF_PI) * HitShakeDistance.Y,
        FMath::Sin(Phase * 0.73f + PI) * HitShakeDistance.Z);
    MachineMesh->SetRelativeLocation(
        MachineMeshBaseLocation + Offset * Envelope);
}

void ACMPartVendingMachine::OnRep_RemainingUses()
{
    ApplyDisplayPresentation();
    OnRemainingUsesChanged(RemainingUses);
}

void ACMPartVendingMachine::ApplyDisplayPresentation()
{
    if (UCMPartVendingMachineWidget* Widget =
        Cast<UCMPartVendingMachineWidget>(DisplayWidget->GetUserWidgetObject()))
    {
        Widget->SetPresentation(PartIcon, RemainingUses);
    }
}

ACMPartActorBase* ACMPartVendingMachine::DispensePart()
{
    if (!HasAuthority() || !PartClass || !DispensePoint)
    {
        UE_LOG(LogChimeraPartVendingMachine, Warning,
            TEXT("Part vending machine has no valid output. Machine=%s PartClass=%s"),
            *GetName(), *GetNameSafe(PartClass.Get()));
        return nullptr;
    }

    ACMPartActorBase* Part = ACMPartActorBase::SpawnPartFromDataRows(
        this,
        PartClass,
        PartRowName,
        TierRowName,
        DispensePoint->GetComponentTransform(),
        nullptr);
    if (!Part)
    {
        return nullptr;
    }

    if (USkeletalMeshComponent* PartMesh = Part->GetPartMesh())
    {
        const FVector EjectVelocity = DispensePoint->GetComponentTransform()
            .TransformVectorNoScale(LocalEjectVelocity);
        PartMesh->AddImpulse(EjectVelocity, NAME_None, true);
    }

    UE_LOG(LogChimeraPartVendingMachine, Display,
        TEXT("Part dispensed. Machine=%s Part=%s Class=%s"),
        *GetName(), *GetNameSafe(Part), *GetNameSafe(PartClass.Get()));
    return Part;
}
