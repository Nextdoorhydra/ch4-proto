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
        // BeamThickness는 충돌의 반지름이므로 Niagara에는 전체 폭을 전달
        BeamEffect->SetVariableFloat(NiagaraWidthParameter, BeamThickness * 2.0f);
        // Niagara의 BeamColor는 Scale RGB 입력과 같은 Vector3 타입으로 사용
        BeamEffect->SetVariableVec3(
            NiagaraColorParameter,
            FVector(BeamColor.R, BeamColor.G, BeamColor.B));
    }
}

// 연결된 모든 Beam 표현의 활성 상태 통일
void UCMLaserBeamComponent::SetBeamVisible(
    bool bVisible,
    bool bAllowEffectActivation)
{
    if (BeamMesh)
    {
        BeamMesh->SetVisibility(bVisible, true);
    }
    if (BeamEffect)
    {
        const bool bShowEffect = bVisible && bAllowEffectActivation
            && GetOwner() && GetOwner()->GetNetMode() != NM_DedicatedServer;
        BeamEffect->SetVisibility(bShowEffect, true);
        if (bShowEffect && !BeamEffect->IsActive())
        {
            BeamEffect->Activate(true);
        }
        else if (!bShowEffect)
        {
            BeamEffect->Deactivate();
        }
    }
}

// 플레이어를 맞힌 동안에만 Niagara 끝점 스파크가 생성되도록 Spawn Rate 갱신
void UCMLaserBeamComponent::SetPlayerImpactActive(bool bActive)
{
    if (BeamEffect)
    {
        BeamEffect->SetVariableFloat(
            NiagaraImpactSpawnRateParameter,
            bActive ? ImpactSparkRate : 0.0f);
    }
}
