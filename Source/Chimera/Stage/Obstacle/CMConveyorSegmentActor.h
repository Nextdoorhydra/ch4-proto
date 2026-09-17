#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CMConveyorSegmentActor.generated.h"

class UArrowComponent;
class USceneComponent;
class USplineComponent;
class UStaticMesh;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ECMConveyorSegmentShape : uint8
{
    Straight,
    CurveLeft90,
    CurveRight90
};

/** Placeable conveyor mesh with a generated local movement path and endpoint connectors. */
UCLASS(Blueprintable)
class CHIMERA_API ACMConveyorSegmentActor : public AActor
{
    GENERATED_BODY()

public:
    ACMConveyorSegmentActor();

    virtual void OnConstruction(const FTransform& Transform) override;

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Chimera|Conveyor|Placement")
    void SnapToPreviousSegment();

    UFUNCTION(BlueprintPure, Category = "Chimera|Conveyor")
    USplineComponent* GetConveyorPath() const { return ConveyorPath; }

    FTransform GetStartWorldTransform() const;
    FTransform GetEndWorldTransform() const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<UStaticMeshComponent> ConveyorMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<USplineComponent> ConveyorPath;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<UArrowComponent> StartConnector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<UArrowComponent> EndConnector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor|Segment")
    TObjectPtr<UStaticMesh> ConveyorMeshAsset;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor|Segment")
    ECMConveyorSegmentShape SegmentShape = ECMConveyorSegmentShape::Straight;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor|Segment", meta = (ClampMin = "1.0", ForceUnits = "cm", EditCondition = "SegmentShape == ECMConveyorSegmentShape::Straight", EditConditionHides))
    float StraightLength = 240.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor|Segment", meta = (ClampMin = "1.0", ForceUnits = "cm", EditCondition = "SegmentShape != ECMConveyorSegmentShape::Straight", EditConditionHides))
    float CurveRadius = 305.58f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor|Segment", meta = (ForceUnits = "cm"))
    float PathHeight = 123.18f;

    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Chimera|Conveyor|Placement")
    TObjectPtr<ACMConveyorSegmentActor> PreviousPlacementSegment;

private:
    void RebuildGeneratedPath();
    void UpdateConnectors();
};
