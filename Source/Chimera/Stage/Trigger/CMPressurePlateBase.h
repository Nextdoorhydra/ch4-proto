#pragma once

#include "CoreMinimal.h"
#include "Stage/Trigger/CMStageButtonBase.h"

#include "CMPressurePlateBase.generated.h"

class UBoxComponent;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
// 영역 안의 데이터 무게 합계가 기준을 넘으면 눌리고 낮아지면 해제되는 감압판
class CHIMERA_API ACMPressurePlateBase : public ACMStageButtonBase
{
    GENERATED_BODY()

public:
    ACMPressurePlateBase();

    UFUNCTION(BlueprintPure, Category = "Chimera|Mechanism|Pressure Plate")
    float GetCurrentWeight() const { return HasAuthority() ? CurrentWeight : GetPresentationState().CurrentWeight; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Mechanism|Pressure Plate")
    float GetRequiredWeight() const { return HasAuthority() ? RequiredWeight : GetPresentationState().RequiredWeight; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Mechanism|Pressure Plate")
    float GetReleaseWeight() const { return HasAuthority() ? ReleaseWeight : GetPresentationState().ReleaseWeight; }

protected:
    virtual void FillPresentationState(FCMTriggerPresentationState& State) const override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void HandleElementReset_Implementation() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Pressure Plate")
    TObjectPtr<UBoxComponent> PressureVolume;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Chimera|Mechanism|Pressure Plate|Presentation")
    TObjectPtr<UStaticMeshComponent> PlateVisualMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Pressure Plate",
        meta = (ClampMin = "0.0"))
    float RequiredWeight = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Pressure Plate",
        meta = (ClampMin = "0.0"))
    float ReleaseWeight = 90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Mechanism|Pressure Plate|Presentation")
    FLinearColor OffColor = FLinearColor(0.05f, 0.05f, 0.05f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Mechanism|Pressure Plate|Presentation")
    FLinearColor OnColor = FLinearColor::Red;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Mechanism|Pressure Plate|Presentation")
    FLinearColor DisabledColor = FLinearColor(0.02f, 0.02f, 0.02f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Mechanism|Pressure Plate|Presentation",
        meta = (ClampMin = "0.0"))
    float OffEmissiveIntensity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Mechanism|Pressure Plate|Presentation",
        meta = (ClampMin = "0.0"))
    float OnEmissiveIntensity = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Mechanism|Pressure Plate|Presentation",
        meta = (ClampMin = "0"))
    int32 MaterialSlotIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Mechanism|Pressure Plate|Presentation")
    FName ColorParameterName = TEXT("ButtonColor");

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Mechanism|Pressure Plate|Presentation")
    FName EmissiveParameterName = TEXT("EmissiveIntensity");

    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Mechanism|Pressure Plate")
    void OnPressureChanged(float NewWeight, bool bIsPressed);

private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FCMTriggerPresentationTest;
#endif
    UFUNCTION()
    void HandlePressureBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void HandlePressureEndOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex);

    UFUNCTION()
    void HandlePresentationStateChanged(
        const FCMTriggerPresentationState& State);

    void RecalculatePressure(AActor* ChangedActor);
    float ResolveMechanismWeight(const AActor* Actor) const;
    void ApplyPresentationState(const FCMTriggerPresentationState& State);

    TMap<TWeakObjectPtr<AActor>, int32> OverlapCounts;
    float CurrentWeight = 0.0f;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> PlateMaterial;
};
