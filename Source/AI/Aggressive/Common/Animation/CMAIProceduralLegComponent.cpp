#include "Aggressive/Common/Animation/CMAIProceduralLegComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/ConstructorHelpers.h"

const FName UCMAIProceduralLegComponent::ThighBoneName(TEXT("thigh_l"));
const FName UCMAIProceduralLegComponent::CalfBoneName(TEXT("calf_l"));
const FName UCMAIProceduralLegComponent::FootBoneName(TEXT("foot_l"));

FVector CMAIProceduralLeg::CalculateSwingLocation(const FVector& Start, const FVector& Target, float Phase, float StepHeight)
{
    const float SafePhase = FMath::Clamp(Phase, 0.0f, 1.0f);
    const float SmoothPhase = FMath::SmoothStep(0.0f, 1.0f, SafePhase);
    return FMath::Lerp(Start, Target, SmoothPhase) + FVector::UpVector * FMath::Sin(SafePhase * PI) * FMath::Max(StepHeight, 0.0f);
}

FVector CMAIProceduralLeg::CalculateDesiredFootLocation(const FVector& ContactLocation, const FVector& PlanarVelocity, float LeadSeconds, float MaximumLeadDistance)
{
    FVector SafePlanarVelocity = PlanarVelocity;
    SafePlanarVelocity.Z = 0.0f;
    const FVector LeadOffset = (SafePlanarVelocity * FMath::Max(LeadSeconds, 0.0f)).GetClampedToMaxSize(FMath::Max(MaximumLeadDistance, 0.0f));
    return ContactLocation + LeadOffset;
}

FVector CMAIProceduralLeg::CalculateKneeLocation(const FVector& Hip, const FVector& Foot, const FVector& PoleDirection, float UpperLength, float LowerLength)
{
    const float SafeUpperLength = FMath::Max(UpperLength, 0.01f);
    const float SafeLowerLength = FMath::Max(LowerLength, 0.01f);
    const FVector HipToFoot = Foot - Hip;
    const FVector ChainDirection = HipToFoot.GetSafeNormal(SMALL_NUMBER, FVector::DownVector);
    const float MinimumReach = FMath::Abs(SafeUpperLength - SafeLowerLength) + 0.01f;
    const float MaximumReach = SafeUpperLength + SafeLowerLength - 0.01f;
    const float Reach = FMath::Clamp(HipToFoot.Size(), MinimumReach, FMath::Max(MaximumReach, MinimumReach));
    const float AlongDistance = (FMath::Square(SafeUpperLength) - FMath::Square(SafeLowerLength) + FMath::Square(Reach)) / (2.0f * Reach);
    const float BendDistance = FMath::Sqrt(FMath::Max(FMath::Square(SafeUpperLength) - FMath::Square(AlongDistance), 0.0f));
    FVector BendDirection = PoleDirection - ChainDirection * FVector::DotProduct(PoleDirection, ChainDirection);
    BendDirection = BendDirection.GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);
    return Hip + ChainDirection * AlongDistance + BendDirection * BendDistance;
}

bool CMAIProceduralLeg::ShouldStartStep(const FVector& PlantedLocation, const FVector& DesiredLocation, float PlanarSpeed, float TriggerDistance, float MinimumSpeed)
{
    const float SafeTriggerDistance = FMath::Max(TriggerDistance, 0.0f);
    return PlanarSpeed >= FMath::Max(MinimumSpeed, 0.0f) && FVector::DistSquared2D(PlantedLocation, DesiredLocation) >= FMath::Square(SafeTriggerDistance);
}

bool CMAIProceduralLeg::ShouldReplantFoot(const FVector& FootAnchor, const FVector& DesiredLocation, float PlanarSpeed, float StepTriggerDistance, float MinimumSpeed, float EmergencyDistance)
{
    const float DistanceSquared = FVector::DistSquared2D(FootAnchor, DesiredLocation);
    const bool bStoppedAwayFromContact = PlanarSpeed < FMath::Max(MinimumSpeed, 0.0f) && DistanceSquared >= FMath::Square(FMath::Max(StepTriggerDistance, 0.0f));
    const bool bBeyondSafeReach = DistanceSquared >= FMath::Square(FMath::Max(EmergencyDistance, 0.0f));
    return bStoppedAwayFromContact || bBeyondSafeReach;
}

UCMAIProceduralLegComponent::UCMAIProceduralLegComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetCanEverAffectNavigation(false);

    static ConstructorHelpers::FObjectFinder<USkeletalMesh> LegMeshAsset(TEXT("/Game/CoreC/02BaseBody/SKM/male_Leg_L.male_Leg_L"));
    if (LegMeshAsset.Succeeded())
    {
        SetSkinnedAssetAndUpdate(LegMeshAsset.Object, true);
    }
}

void UCMAIProceduralLegComponent::Configure(USceneComponent* InContactPoint, FVector InOutwardLocalDirection, float InPhaseOffset, float InVisualScale)
{
    ContactPoint = InContactPoint;
    OutwardLocalDirection = InOutwardLocalDirection.GetSafeNormal(SMALL_NUMBER, FVector::RightVector);
    PhaseOffset = FMath::Clamp(InPhaseOffset, 0.0f, 1.0f);
    const float SafeScale = FMath::Max(InVisualScale, 0.01f);
    SetRelativeScale3D(FVector(SafeScale));
}

void UCMAIProceduralLegComponent::BeginPlay()
{
    Super::BeginPlay();
    const bool bCanRender = GetNetMode() != NM_DedicatedServer && GetSkinnedAsset() && ContactPoint;
    SetComponentTickEnabled(bCanRender);
    SetHiddenInGame(!bCanRender);
    if (bCanRender)
    {
        InitializeLeg();
    }
}

void UCMAIProceduralLegComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bInitialized && !InitializeLeg())
    {
        return;
    }

    UWorld* World = GetWorld();
    UPrimitiveComponent* MovementBody = ResolveMovementBody();
    if (!World || !MovementBody || !ContactPoint)
    {
        return;
    }

    const double CurrentTime = World->GetTimeSeconds();
    FVector PlanarVelocity = MovementBody->GetPhysicsLinearVelocity();
    PlanarVelocity.Z = 0.0f;
    const float PlanarSpeed = PlanarVelocity.Size();
    FVector DesiredLocation = CMAIProceduralLeg::CalculateDesiredFootLocation(ContactPoint->GetComponentLocation(), PlanarVelocity, FootLeadSeconds, MaximumFootLeadDistance);
    FVector DesiredNormal = FVector::UpVector;
    FindGround(DesiredLocation, DesiredLocation, DesiredNormal);

    const FVector FootAnchor = bStepping ? StepTargetLocation : PlantedLocation;
    if (CMAIProceduralLeg::ShouldReplantFoot(FootAnchor, DesiredLocation, PlanarSpeed, StepTriggerDistance, MinimumPlanarSpeed, EmergencyReplantDistance))
    {
        PlantedLocation = DesiredLocation;
        PlantedNormal = DesiredNormal;
        StepStartLocation = DesiredLocation;
        StepStartNormal = DesiredNormal;
        StepTargetLocation = DesiredLocation;
        StepTargetNormal = DesiredNormal;
        bStepping = false;
        NextStepTime = CurrentTime + StepDuration * 0.35f;
        UpdateLegPose(PlantedLocation, PlantedNormal);
        return;
    }

    if (!bStepping && CurrentTime >= NextStepTime && CMAIProceduralLeg::ShouldStartStep(PlantedLocation, DesiredLocation, PlanarSpeed, StepTriggerDistance, MinimumPlanarSpeed))
    {
        StartStep(DesiredLocation, DesiredNormal, CurrentTime);
    }

    if (!bStepping)
    {
        UpdateLegPose(PlantedLocation, PlantedNormal);
        return;
    }

    const float Phase = FMath::Clamp(static_cast<float>((CurrentTime - StepStartTime) / FMath::Max(StepDuration, 0.01f)), 0.0f, 1.0f);
    const FVector SwingLocation = CMAIProceduralLeg::CalculateSwingLocation(StepStartLocation, StepTargetLocation, Phase, StepHeight);
    const FVector SwingNormal = FMath::Lerp(StepStartNormal, StepTargetNormal, Phase).GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
    UpdateLegPose(SwingLocation, SwingNormal);
    if (Phase >= 1.0f)
    {
        PlantedLocation = StepTargetLocation;
        PlantedNormal = StepTargetNormal;
        bStepping = false;
        NextStepTime = CurrentTime + StepDuration * 0.35f;
    }
}

bool UCMAIProceduralLegComponent::InitializeLeg()
{
    if (!GetSkinnedAsset() || !ContactPoint || GetBoneIndex(ThighBoneName) == INDEX_NONE || GetBoneIndex(CalfBoneName) == INDEX_NONE || GetBoneIndex(FootBoneName) == INDEX_NONE)
    {
        return false;
    }

    RefreshBoneTransforms();
    PlantedLocation = ContactPoint->GetComponentLocation();
    PlantedNormal = FVector::UpVector;
    FindGround(PlantedLocation, PlantedLocation, PlantedNormal);
    NextStepTime = GetWorld() ? GetWorld()->GetTimeSeconds() + PhaseOffset * FMath::Max(StepDuration, 0.01f) : 0.0;
    bInitialized = true;
    UpdateLegPose(PlantedLocation, PlantedNormal);
    return true;
}

bool UCMAIProceduralLegComponent::FindGround(const FVector& CandidateLocation, FVector& OutLocation, FVector& OutNormal) const
{
    const UWorld* World = GetWorld();
    const AActor* Owner = GetOwner();
    if (!World || !Owner)
    {
        return false;
    }

    FHitResult GroundHit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CMAIProceduralLegGround), false, Owner);
    const FVector Start = CandidateLocation + FVector::UpVector * FMath::Max(GroundTraceUpDistance, 0.0f);
    const FVector End = CandidateLocation - FVector::UpVector * FMath::Max(GroundTraceDownDistance, 0.0f);
    if (!World->LineTraceSingleByChannel(GroundHit, Start, End, ECC_Visibility, QueryParams) || GroundHit.ImpactNormal.Z < MinimumGroundNormalZ)
    {
        return false;
    }

    OutLocation = GroundHit.ImpactPoint;
    OutNormal = GroundHit.ImpactNormal.GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
    return true;
}

UPrimitiveComponent* UCMAIProceduralLegComponent::ResolveMovementBody() const
{
    for (USceneComponent* Parent = ContactPoint ? ContactPoint->GetAttachParent() : GetAttachParent(); Parent; Parent = Parent->GetAttachParent())
    {
        if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Parent))
        {
            return Primitive;
        }
    }
    return Cast<UPrimitiveComponent>(GetOwner() ? GetOwner()->GetRootComponent() : nullptr);
}

void UCMAIProceduralLegComponent::StartStep(const FVector& DesiredLocation, const FVector& DesiredNormal, double CurrentTime)
{
    StepStartLocation = PlantedLocation;
    StepStartNormal = PlantedNormal;
    StepTargetLocation = DesiredLocation;
    StepTargetNormal = DesiredNormal;
    StepStartTime = CurrentTime;
    bStepping = true;
}

void UCMAIProceduralLegComponent::UpdateLegPose(const FVector& FootLocation, const FVector& GroundNormal)
{
    ResetBoneTransformByName(ThighBoneName);
    ResetBoneTransformByName(CalfBoneName);
    ResetBoneTransformByName(FootBoneName);
    RefreshBoneTransforms();

    const FVector HipLocation = GetComponentLocation();
    const FVector PoleDirection = GetAttachParent() ? GetAttachParent()->GetComponentTransform().TransformVectorNoScale(OutwardLocalDirection) : OutwardLocalDirection;
    FTransform ThighTransform = GetBoneTransformByName(ThighBoneName, EBoneSpaces::WorldSpace);
    const FTransform ReferenceCalfTransform = GetBoneTransformByName(CalfBoneName, EBoneSpaces::WorldSpace);
    const FTransform ReferenceFootTransform = GetBoneTransformByName(FootBoneName, EBoneSpaces::WorldSpace);
    const float UpperLength = FVector::Distance(ThighTransform.GetLocation(), ReferenceCalfTransform.GetLocation());
    const float LowerLength = FVector::Distance(ReferenceCalfTransform.GetLocation(), ReferenceFootTransform.GetLocation());
    const FVector KneeLocation = CMAIProceduralLeg::CalculateKneeLocation(HipLocation, FootLocation, PoleDirection, UpperLength, LowerLength);
    const FVector ReferenceUpperDirection = (ReferenceCalfTransform.GetLocation() - ThighTransform.GetLocation()).GetSafeNormal(SMALL_NUMBER, FVector::DownVector);
    const FVector UpperDirection = (KneeLocation - HipLocation).GetSafeNormal(SMALL_NUMBER, FVector::DownVector);
    ThighTransform.SetLocation(HipLocation);
    ThighTransform.SetRotation(FQuat::FindBetweenNormals(ReferenceUpperDirection, UpperDirection) * ThighTransform.GetRotation());
    SetBoneTransformByName(ThighBoneName, ThighTransform, EBoneSpaces::WorldSpace);
    RefreshBoneTransforms();

    FTransform CalfTransform = GetBoneTransformByName(CalfBoneName, EBoneSpaces::WorldSpace);
    FTransform FootTransform = GetBoneTransformByName(FootBoneName, EBoneSpaces::WorldSpace);
    const FVector CurrentLowerDirection = (FootTransform.GetLocation() - CalfTransform.GetLocation()).GetSafeNormal(SMALL_NUMBER, FVector::DownVector);
    const FVector LowerDirection = (FootLocation - CalfTransform.GetLocation()).GetSafeNormal(SMALL_NUMBER, FVector::DownVector);
    CalfTransform.SetRotation(FQuat::FindBetweenNormals(CurrentLowerDirection, LowerDirection) * CalfTransform.GetRotation());
    SetBoneTransformByName(CalfBoneName, CalfTransform, EBoneSpaces::WorldSpace);
    RefreshBoneTransforms();

    FootTransform = GetBoneTransformByName(FootBoneName, EBoneSpaces::WorldSpace);
    const FVector CurrentFootUp = FootTransform.GetUnitAxis(EAxis::Z).GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
    FootTransform.SetRotation(FQuat::FindBetweenNormals(CurrentFootUp, GroundNormal.GetSafeNormal(SMALL_NUMBER, FVector::UpVector)) * FootTransform.GetRotation());
    SetBoneTransformByName(FootBoneName, FootTransform, EBoneSpaces::WorldSpace);
    RefreshBoneTransforms();
}
