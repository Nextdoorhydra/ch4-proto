#include "Camera/CMCameraOcclusionTestCube.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ACMCameraOcclusionTestCube::ACMCameraOcclusionTestCube()
{
    PrimaryActorTick.bCanEverTick = false;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(
        TEXT("/Engine/BasicShapes/Cube.Cube")
    );
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> TestMaterial(
        TEXT("/Game/Chimera/Debug/CameraOcclusion/M_CMCameraOcclusionTest.M_CMCameraOcclusionTest")
    );

    CubeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CubeMesh"));
    SetRootComponent(CubeMesh);
    CubeMesh->SetMobility(EComponentMobility::Static);
    CubeMesh->SetCollisionProfileName(TEXT("BlockAll"));
    CubeMesh->SetCollisionObjectType(ECC_WorldStatic);
    CubeMesh->SetRelativeScale3D(FVector(2.5f, 2.5f, 3.0f));

    if (CubeMeshAsset.Succeeded())
    {
        CubeMesh->SetStaticMesh(CubeMeshAsset.Object);
    }
    if (TestMaterial.Succeeded())
    {
        CubeMesh->SetMaterial(0, TestMaterial.Object);
    }
}
