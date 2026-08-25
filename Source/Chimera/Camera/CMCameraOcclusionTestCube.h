#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CMCameraOcclusionTestCube.generated.h"

class UStaticMeshComponent;

/** Placeable cube with a ready-to-use camera occlusion test material. */
UCLASS(meta = (DisplayName = "CM Camera Occlusion Test Cube"))
class CHIMERA_API ACMCameraOcclusionTestCube : public AActor
{
    GENERATED_BODY()

public:
    ACMCameraOcclusionTestCube();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Test")
    TObjectPtr<UStaticMeshComponent> CubeMesh;
};
