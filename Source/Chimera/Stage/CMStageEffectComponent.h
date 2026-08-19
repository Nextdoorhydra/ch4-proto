#pragma once

#include "CoreMinimal.h"
#include "Stage/CMStageElementComponent.h"

#include "CMStageEffectComponent.generated.h"

class UNiagaraComponent;

UENUM(BlueprintType)
enum class ECMStageEffectPreviewAction : uint8
{
    Activate,
    Deactivate,
    Restart,
    Burst
};

UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
// 레벨에 배치된 나이아가라 컴포넌트 묶음을 Stage Command로 제어
class CHIMERA_API UCMStageEffectComponent : public UCMStageElementComponent
{
    GENERATED_BODY()

public:
    UCMStageEffectComponent();

    // 이 StageElement가 함께 제어할 나이아가라 컴포넌트 목록
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Chimera|Stage|Effect")
    TArray<TObjectPtr<UNiagaraComponent>> Effects;

    // 에디터 Preview 버튼이 실행할 동작
    UPROPERTY(EditAnywhere, Category = "Chimera|Stage|Effect")
    ECMStageEffectPreviewAction PreviewAction = ECMStageEffectPreviewAction::Activate;

    // 선택한 PreviewAction을 에디터 월드에서 즉시 실행
    UFUNCTION(CallInEditor, Category = "Chimera|Stage|Effect")
    void PreviewSelectedAction();

    // DataTable에서 전달된 Effect Command를 나이아가라 동작으로 변환
    virtual void ExecuteStageCommand_Implementation(
        FGameplayTag CommandTag,
        UObject* CommandInstigator) override;

private:
    void ActivateEffects(bool bReset);
    void DeactivateEffects();
    void RestartEffects();
    void BurstEffects();
};
