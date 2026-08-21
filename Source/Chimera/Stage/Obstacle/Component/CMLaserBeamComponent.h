#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CMLaserBeamComponent.generated.h"

class UNiagaraComponent;
class UStaticMeshComponent;

UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
// 시작점과 끝점을 받아 레이저 Mesh와 Niagara 표현을 같은 위치에 맞춤
class CHIMERA_API UCMLaserBeamComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMLaserBeamComponent();

    // 레이저 표현에 사용할 기존 Scene Component 연결
    UFUNCTION(BlueprintCallable, Category = "Chimera|Beam Presentation")
    void ConfigurePresentation(
        UStaticMeshComponent* InBeamMesh,
        UNiagaraComponent* InBeamEffect);

    // 중앙 Pivot과 로컬 X축 길이를 기준으로 Beam 표현 갱신
    UFUNCTION(BlueprintCallable, Category = "Chimera|Beam Presentation")
    void ApplyBeam(const FVector& StartLocation, const FVector& EndLocation);

    // Mesh와 Niagara를 함께 표시하거나 숨김
    UFUNCTION(BlueprintCallable, Category = "Chimera|Beam Presentation")
    void SetBeamVisible(bool bVisible);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Beam Presentation",
        meta = (ClampMin = "0.01"))
    float MeshOriginalLength = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Beam Presentation",
        meta = (ClampMin = "0.01"))
    float BeamThickness = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Beam Presentation|Niagara")
    FName NiagaraStartParameter = TEXT("User.BeamStart");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Beam Presentation|Niagara")
    FName NiagaraEndParameter = TEXT("User.BeamEnd");

private:
    UPROPERTY(Transient)
    TObjectPtr<UStaticMeshComponent> BeamMesh;

    UPROPERTY(Transient)
    TObjectPtr<UNiagaraComponent> BeamEffect;
};
