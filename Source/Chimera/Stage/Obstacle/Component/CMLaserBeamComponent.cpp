#include "Stage/Obstacle/Component/CMLaserBeamComponent.h"

#include "Components/StaticMeshComponent.h"
#include "NiagaraComponent.h"

UCMLaserBeamComponent::UCMLaserBeamComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// 소유 Actor가 가진 Mesh와 Niagara를 공용 Beam 계산 대상으로 저장
void UCMLaserBeamComponent::ConfigurePresentation(
    UStaticMeshComponent* InBeamMesh,
    UNiagaraComponent* InBeamEffect)
{
    BeamMesh = InBeamMesh;
    BeamEffect = InBeamEffect;
}

// 중앙 Pivot Mesh를 두 점 사이에 정렬하고 Niagara에 월드 좌표 전달
void UCMLaserBeamComponent::ApplyBeam(
    const FVector& StartLocation,
    const FVector& EndLocation)
{
    const FVector Delta = EndLocation - StartLocation;
    const float BeamLength = Delta.Size();
    if (BeamLength <= UE_KINDA_SMALL_NUMBER)
    {
        SetBeamVisible(false);
        return;
    }

    const FVector BeamCenter = FMath::Lerp(StartLocation, EndLocation, 0.5f);
    const FRotator BeamRotation = Delta.Rotation();

    if (BeamMesh)
    {
        BeamMesh->SetWorldLocationAndRotation(BeamCenter, BeamRotation);
        BeamMesh->SetWorldScale3D(FVector(
            BeamLength / FMath::Max(MeshOriginalLength, UE_KINDA_SMALL_NUMBER),
            BeamThickness * 2.0f / FMath::Max(MeshOriginalLength, UE_KINDA_SMALL_NUMBER),
            BeamThickness * 2.0f / FMath::Max(MeshOriginalLength, UE_KINDA_SMALL_NUMBER)));
    }

    if (BeamEffect)
    {
        BeamEffect->SetVariablePosition(NiagaraStartParameter, StartLocation);
        BeamEffect->SetVariablePosition(NiagaraEndParameter, EndLocation);
    }
}

// 연결된 모든 Beam 표현의 활성 상태 통일
void UCMLaserBeamComponent::SetBeamVisible(bool bVisible)
{
    if (BeamMesh)
    {
        BeamMesh->SetVisibility(bVisible, true);
    }
    if (BeamEffect)
    {
        BeamEffect->SetVisibility(bVisible, true);
        if (bVisible)
        {
            BeamEffect->Activate(true);
        }
        else
        {
            BeamEffect->Deactivate();
        }
    }
}
