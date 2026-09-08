#include "Player/CMChimeraBodySegmentActor.h"

#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Parts/Tentacle/CMTentacleSegmentActor.h"
#include "Player/CMChimera.h"
#include "Player/CMChimeraIdleTentacleComponent.h"
#include "Player/CMRuntimeChildActorComponent.h"
#include "Player/CMChimeraVisualDefinition.h"
#include "Net/UnrealNetwork.h"

ECMChimeraSegmentVisualRole CMChimeraVisual::ResolveSegmentVisualRole(
    const int32 SegmentIndex,
    const int32 ActiveSegmentCount)
{
    if (SegmentIndex == 0 && ActiveSegmentCount > 0)
    {
        return ECMChimeraSegmentVisualRole::Head;
    }
    if (ActiveSegmentCount > 1
        && SegmentIndex == ActiveSegmentCount - 1)
    {
        return ECMChimeraSegmentVisualRole::Tail;
    }
    return ECMChimeraSegmentVisualRole::Body;
}

ACMChimeraBodySegmentActor::ACMChimeraBodySegmentActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(false);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    BodyVisual = CreateDefaultSubobject<USkeletalMeshComponent>(
        TEXT("BodyVisual"));
    BodyVisual->SetupAttachment(SceneRoot);
    BodyVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyVisual->SetGenerateOverlapEvents(false);
    BodyVisual->SetCanEverAffectNavigation(false);

    TentacleActor = CreateDefaultSubobject<UCMRuntimeChildActorComponent>(
        TEXT("TentacleActor"));
    TentacleActor->SetupAttachment(SceneRoot);
    TentacleActor->SetChildActorClass(ACMTentacleSegmentActor::StaticClass());

    IdleTentacles = CreateDefaultSubobject<
        UCMChimeraIdleTentacleComponent>(TEXT("IdleTentacles"));
    IdleTentacles->SetupAttachment(SceneRoot);
}

void ACMChimeraBodySegmentActor::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACMChimeraBodySegmentActor, SegmentIndex);
    DOREPLIFETIME(ACMChimeraBodySegmentActor, bSegmentActive);
    DOREPLIFETIME(ACMChimeraBodySegmentActor, VisualRole);
}

void ACMChimeraBodySegmentActor::OnConstruction(
    const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplyVisualPreset();
}

void ACMChimeraBodySegmentActor::SetTentacleActorClass(
    TSubclassOf<ACMTentacleSegmentActor> InTentacleActorClass)
{
    if (!TentacleActor
        || !InTentacleActorClass
        || TentacleActor->GetChildActorClass() == InTentacleActorClass)
    {
        return;
    }

    TentacleActor->SetChildActorClass(InTentacleActorClass);

    if (ACMTentacleSegmentActor* SegmentTentacle = GetTentacleActor())
    {
        if (HasAuthority() && ChimeraOwner && BodySegment)
        {
            SegmentTentacle->InitializeForSegment(
                ChimeraOwner,
                SegmentIndex,
                BodySegment,
                false);
        }
        SegmentTentacle->SetSegmentActive(bSegmentActive);
    }
    RefreshIdleTentacleSource();
    if (HasAuthority())
    {
        ForceNetUpdate();
    }
}

void ACMChimeraBodySegmentActor::InitializeForSegment(
    ACMChimera* InChimera,
    const int32 InSegmentIndex,
    UPrimitiveComponent* InBodySegment)
{
    ChimeraOwner = InChimera;
    SegmentIndex = InSegmentIndex;
    BodySegment = InBodySegment;

    if (HasAuthority() && InChimera)
    {
        SetOwner(InChimera);
    }

    if (ACMTentacleSegmentActor* SegmentTentacle = GetTentacleActor())
    {
        if (HasAuthority() && InChimera && InBodySegment)
        {
            SegmentTentacle->InitializeForSegment(
                InChimera,
                InSegmentIndex,
                InBodySegment,
                false);
        }
        SegmentTentacle->SetSegmentActive(bSegmentActive);
    }
    RefreshIdleTentacleSource();
}

void ACMChimeraBodySegmentActor::SetSegmentPresentation(
    const bool bInActive,
    const ECMChimeraSegmentVisualRole InVisualRole)
{
    const bool bRoleChanged = !bHasPresentationState
        || VisualRole != InVisualRole;
    const bool bActiveChanged = !bHasPresentationState
        || bSegmentActive != bInActive;

    VisualRole = InVisualRole;
    bSegmentActive = bInActive;
    bHasPresentationState = true;

    if (BodyVisual)
    {
        BodyVisual->SetVisibility(bInActive, true);
        BodyVisual->SetHiddenInGame(!bInActive, true);
        BodyVisual->SetComponentTickEnabled(bInActive);
        BodyVisual->VisibilityBasedAnimTickOption = bInActive
            ? EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones
            : EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    }

    if (ACMTentacleSegmentActor* SegmentTentacle = GetTentacleActor())
    {
        SegmentTentacle->SetSegmentActive(bInActive);
    }

    if (bRoleChanged)
    {
        ApplyVisualPreset();
        K2_ApplySegmentVisualRole(InVisualRole);
    }
    if (IdleTentacles)
    {
        IdleTentacles->SetEffectActive(bInActive);
    }
    if (bActiveChanged)
    {
        K2_SetSegmentVisualActive(bInActive);
    }
    if (HasAuthority() && (bRoleChanged || bActiveChanged))
    {
        ForceNetUpdate();
    }
}

void ACMChimeraBodySegmentActor::OnRep_PresentationState()
{
    bHasPresentationState = false;
    SetSegmentPresentation(bSegmentActive, VisualRole);
    RefreshIdleTentacleSource();
}

void ACMChimeraBodySegmentActor::ApplyVisualPreset()
{
    if (!BodyVisual || !VisualDefinition)
    {
        return;
    }

    const FCMChimeraSegmentVisualPreset& Preset =
        VisualDefinition->GetPreset(VisualRole);
    BodyVisual->SetSkeletalMeshAsset(Preset.Mesh);
    BodyVisual->SetAnimInstanceClass(Preset.AnimClass);
    BodyVisual->SetRelativeTransform(Preset.RelativeTransform);
    if (IdleTentacles)
    {
        IdleTentacles->NotifySourceMeshChanged();
    }
}

ACMTentacleSegmentActor* ACMChimeraBodySegmentActor::GetTentacleActor() const
{
    return TentacleActor
        ? Cast<ACMTentacleSegmentActor>(TentacleActor->GetChildActor())
        : nullptr;
}

void ACMChimeraBodySegmentActor::RefreshIdleTentacleSource()
{
    if (!IdleTentacles)
    {
        return;
    }

    ACMTentacleSegmentActor* SegmentTentacle = GetTentacleActor();
    IdleTentacles->ConfigureSource(
        SegmentTentacle
            ? static_cast<UMeshComponent*>(
                SegmentTentacle->GetGooBodyComponent())
            : static_cast<UMeshComponent*>(BodyVisual.Get()),
        SegmentIndex);
}
