#include "Stage/Obstacle/CMConveyorSegmentActor.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"

ACMConveyorSegmentActor::ACMConveyorSegmentActor()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    ConveyorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ConveyorMesh"));
    ConveyorMesh->SetupAttachment(SceneRoot);
    ConveyorMesh->SetMobility(EComponentMobility::Movable);
    ConveyorMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    ConveyorMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

    ConveyorPath = CreateDefaultSubobject<USplineComponent>(TEXT("ConveyorPath"));
    ConveyorPath->SetupAttachment(SceneRoot);
    ConveyorPath->SetMobility(EComponentMobility::Movable);
    ConveyorPath->SetDrawDebug(true);
    ConveyorPath->bEditableWhenInherited = true;
#if WITH_EDITORONLY_DATA
    ConveyorPath->EditorUnselectedSplineSegmentColor = FLinearColor(0.0f, 0.8f, 0.2f);
    ConveyorPath->EditorSelectedSplineSegmentColor = FLinearColor::Yellow;
    ConveyorPath->EditorTangentColor = FLinearColor(1.0f, 0.5f, 0.0f);
#endif

    StartConnector = CreateDefaultSubobject<UArrowComponent>(TEXT("StartConnector"));
    StartConnector->SetupAttachment(SceneRoot);
    StartConnector->SetMobility(EComponentMobility::Movable);
    StartConnector->SetHiddenInGame(true);
    StartConnector->ArrowColor = FColor::Green;

    EndConnector = CreateDefaultSubobject<UArrowComponent>(TEXT("EndConnector"));
    EndConnector->SetupAttachment(SceneRoot);
    EndConnector->SetMobility(EComponentMobility::Movable);
    EndConnector->SetHiddenInGame(true);
    EndConnector->ArrowColor = FColor::Red;

    RebuildGeneratedPath();
    UpdateConnectors();
}

void ACMConveyorSegmentActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ConveyorMesh->SetStaticMesh(ConveyorMeshAsset);
    FVector MeshScale = ConveyorMesh->GetRelativeScale3D();
    MeshScale.Y = FMath::Abs(MeshScale.Y) * (SegmentShape == ECMConveyorSegmentShape::CurveLeft90 ? -1.0f : 1.0f);
    ConveyorMesh->SetRelativeScale3D(MeshScale);
    RebuildGeneratedPath();
    UpdateConnectors();
}

void ACMConveyorSegmentActor::SnapToPreviousSegment()
{
    if (!IsValid(PreviousPlacementSegment) || PreviousPlacementSegment == this)
    {
        return;
    }

    RebuildGeneratedPath();
    const FTransform StartLocalTransform = ConveyorPath->GetTransformAtSplinePoint(0, ESplineCoordinateSpace::Local, true);
    const FTransform TargetActorTransform = StartLocalTransform.Inverse() * PreviousPlacementSegment->GetEndWorldTransform();
    SetActorTransform(TargetActorTransform);
    UpdateConnectors();
}

FTransform ACMConveyorSegmentActor::GetStartWorldTransform() const
{
    return ConveyorPath->GetTransformAtSplinePoint(0, ESplineCoordinateSpace::World, true);
}

FTransform ACMConveyorSegmentActor::GetEndWorldTransform() const
{
    const int32 LastPointIndex = FMath::Max(ConveyorPath->GetNumberOfSplinePoints() - 1, 0);
    return ConveyorPath->GetTransformAtSplinePoint(LastPointIndex, ESplineCoordinateSpace::World, true);
}

void ACMConveyorSegmentActor::RebuildGeneratedPath()
{
    ConveyorPath->ClearSplinePoints(false);
    const float SafeHeight = PathHeight;
    if (SegmentShape == ECMConveyorSegmentShape::Straight)
    {
        const float HalfLength = FMath::Max(StraightLength, 1.0f) * 0.5f;
        ConveyorPath->AddSplinePoint(FVector(-HalfLength, 0.0f, SafeHeight), ESplineCoordinateSpace::Local, false);
        ConveyorPath->AddSplinePoint(FVector(HalfLength, 0.0f, SafeHeight), ESplineCoordinateSpace::Local, false);
        ConveyorPath->SetSplinePointType(0, ESplinePointType::Linear, false);
        ConveyorPath->SetSplinePointType(1, ESplinePointType::Linear, false);
    }
    else
    {
        const float SafeRadius = FMath::Max(CurveRadius, 1.0f);
        const float TurnSign = SegmentShape == ECMConveyorSegmentShape::CurveLeft90 ? 1.0f : -1.0f;
        const float TangentLength = SafeRadius * 1.65685425f;
        ConveyorPath->AddSplinePoint(FVector(0.0f, 0.0f, SafeHeight), ESplineCoordinateSpace::Local, false);
        ConveyorPath->AddSplinePoint(FVector(SafeRadius, TurnSign * SafeRadius, SafeHeight), ESplineCoordinateSpace::Local, false);
        ConveyorPath->SetSplinePointType(0, ESplinePointType::CurveCustomTangent, false);
        ConveyorPath->SetSplinePointType(1, ESplinePointType::CurveCustomTangent, false);
        ConveyorPath->SetTangentsAtSplinePoint(0, FVector(TangentLength, 0.0f, 0.0f), FVector(TangentLength, 0.0f, 0.0f), ESplineCoordinateSpace::Local, false);
        ConveyorPath->SetTangentsAtSplinePoint(1, FVector(0.0f, TurnSign * TangentLength, 0.0f), FVector(0.0f, TurnSign * TangentLength, 0.0f), ESplineCoordinateSpace::Local, false);
    }
    ConveyorPath->SetClosedLoop(false, false);
    ConveyorPath->UpdateSpline();
}

void ACMConveyorSegmentActor::UpdateConnectors()
{
    const FTransform StartTransform = ConveyorPath->GetTransformAtSplinePoint(0, ESplineCoordinateSpace::Local, true);
    const int32 LastPointIndex = FMath::Max(ConveyorPath->GetNumberOfSplinePoints() - 1, 0);
    const FTransform EndTransform = ConveyorPath->GetTransformAtSplinePoint(LastPointIndex, ESplineCoordinateSpace::Local, true);
    StartConnector->SetRelativeLocationAndRotation(StartTransform.GetLocation(), StartTransform.Rotator());
    EndConnector->SetRelativeLocationAndRotation(EndTransform.GetLocation(), EndTransform.Rotator());
}
