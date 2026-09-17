#pragma once

#include "CoreMinimal.h"
#include "Stage/Trigger/CMStageButtonBase.h"

#include "CMBasicButtonBase.generated.h"

class UBoxComponent;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;
class ACMArmPart;

UCLASS(Blueprintable)
// 팔 스윙 탐지가 타격 영역을 감지하면 작동하는 기본 버튼
class CHIMERA_API ACMBasicButtonBase : public ACMStageButtonBase
{
    GENERATED_BODY()

public:
    ACMBasicButtonBase();
    virtual void Tick(float DeltaSeconds) override;

    // 서버의 팔 스윙 탐지에서만 호출하며 같은 공격은 버튼당 한 번만 처리
    void NotifySwingHit(ACMArmPart* ArmPart, UPrimitiveComponent* HitComponent);
    UBoxComponent* GetHitVolume() const { return HitVolume; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void HandleElementReset_Implementation() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Basic Button")
    TObjectPtr<UBoxComponent> HitVolume;

    // 버튼 캡 메시를 이 Root 아래에서 이동시켜 논리 Collision과 시각 표현을 분리한다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Basic Button|Presentation")
    TObjectPtr<USceneComponent> ButtonVisualRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Basic Button|Presentation")
    TObjectPtr<UStaticMeshComponent> ButtonVisualMesh;

    // 켜면 타격마다 ON/OFF 전환. 끄면 잠시 ON 후 자동 OFF. 둘 다 상태 신호를 전달한다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Basic Button")
    bool bToggleOnHit = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Basic Button|Presentation",
        meta = (ClampMin = "0.0", Units = "cm"))
    float PressDepth = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Basic Button|Presentation")
    FVector LocalPressDirection = FVector(0.0f, 0.0f, -1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Basic Button|Presentation",
        meta = (ClampMin = "0.0", Units = "s"))
    float PressDuration = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Basic Button|Presentation",
        meta = (EditCondition = "!bToggleOnHit", EditConditionHides, ClampMin = "0.0", Units = "s"))
    float PulseHoldDuration = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Basic Button|Presentation",
        meta = (ClampMin = "0.0", Units = "s"))
    float ReleaseDuration = 0.12f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Basic Button|Presentation")
    FLinearColor OffColor = FLinearColor(0.15f, 0.01f, 0.01f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Basic Button|Presentation")
    FLinearColor OnColor = FLinearColor::Red;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Basic Button|Presentation")
    FLinearColor DisabledColor = FLinearColor(0.03f, 0.03f, 0.03f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Basic Button|Presentation",
        meta = (ClampMin = "0.0"))
    float OffEmissiveIntensity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Basic Button|Presentation",
        meta = (ClampMin = "0.0"))
    float OnEmissiveIntensity = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Basic Button|Presentation",
        meta = (ClampMin = "0"))
    int32 MaterialSlotIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Basic Button|Presentation")
    FName ColorParameterName = TEXT("ButtonColor");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Basic Button|Presentation")
    FName EmissiveParameterName = TEXT("EmissiveIntensity");

private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FCMTriggerPresentationTest;
#endif
    UFUNCTION()
    void HandlePresentationStateChanged(
        const FCMTriggerPresentationState& State);

    UFUNCTION()
    void HandleBasicButtonActivated(AActor* TriggeringActor);

    void ScheduleMomentaryRelease();

    void SetVisualTarget(bool bPressed);
    void ReturnPulseVisual();
    void ApplyVisualState();

    TMap<TWeakObjectPtr<ACMArmPart>, FGuid> LastSwingAttackIds;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> ButtonMaterial;
    FVector ReleasedVisualLocation = FVector::ZeroVector;
    FVector PressedVisualLocation = FVector::ZeroVector;
    float VisualAlpha = 0.0f;
    float TargetVisualAlpha = 0.0f;
    bool bPresentationEnabled = true;
    FTimerHandle PulseReturnTimerHandle;

    // 유효한 팔 타격에 반복/일회성 규칙 적용
    void HandleValidButtonInput(AActor* TriggeringActor);
};
