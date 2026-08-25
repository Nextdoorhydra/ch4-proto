#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CMDismembermentTestHarness.generated.h"

class ACharacter;
class UCMBloodTransferComponent;

/** CMGore map smoke-test fixture: spawn BP_Human, then ragdoll it after a delay. */
UCLASS()
class CHIMERA_API ACMDismembermentTestHarness : public AActor
{
    GENERATED_BODY()

public:
    ACMDismembermentTestHarness();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Dismemberment|Testing")
    bool TriggerTestDeath();

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Dismemberment Test")
    TSoftClassPtr<ACharacter> SubjectClass;

    /** Reuses a matching character already placed in the test map. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Dismemberment Test")
    bool bUseExistingSubject = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Dismemberment Test",
        meta = (ClampMin = "0.0"))
    float InitialSeverDelaySeconds = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Dismemberment Test",
        meta = (ClampMin = "0.0"))
    float SeverStepIntervalSeconds = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Dismemberment Test",
        meta = (ClampMin = "0.0"))
    float PostSeverDeathDelaySeconds = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Dismemberment Test",
        meta = (ClampMin = "0.0"))
    float VerificationDelaySeconds = 0.5f;

    /** Intentionally gentle so detached parts settle instead of spinning away. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Dismemberment Test")
    FVector TestSeverImpulse = FVector(120.0f, 0.0f, 80.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Dismemberment Test")
    FVector SubjectSpawnOffset = FVector(0.0f, 0.0f, 100.0f);

    /** After death, visibly drags the detached left arm through the corpse pool. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Dismemberment Test|Blood Stroke")
    bool bRunBloodPoolStrokeVisualTest = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Dismemberment Test|Blood Stroke",
        meta = (ClampMin = "0.0"))
    float BloodStrokeVisualStartDelaySeconds = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Dismemberment Test|Blood Stroke",
        meta = (ClampMin = "1.0"))
    float BloodStrokeVisualStepDistance = 18.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Dismemberment Test|Blood Stroke",
        meta = (ClampMin = "0.01"))
    float BloodStrokeVisualStepIntervalSeconds = 0.12f;

    UFUNCTION(BlueprintPure, Category = "Dismemberment Test")
    ACharacter* GetSpawnedSubject() const
    {
        return SpawnedSubject;
    }

private:
    void HandleDeathTimerElapsed();
    void HandleSeverStepElapsed();
    void VerifyTestDeath();
    void VerifyBloodPoolStroke();
    void AdvanceBloodPoolStrokeVisualTest();
    void FinishBloodPoolStrokeVisualTest(bool bMovementCompleted);
    bool EnsureDismembermentComponent();
    class USkeletalMeshComponent* FindSubjectPartMesh(FName ComponentName) const;

    UPROPERTY(Transient)
    TObjectPtr<ACharacter> SpawnedSubject;

    UPROPERTY(Transient)
    TObjectPtr<AActor> BloodStrokeVisualPart;

    UPROPERTY(Transient)
    TObjectPtr<UCMBloodTransferComponent> BloodStrokeVisualTransfer;

    FTimerHandle DeathTimerHandle;
    FTimerHandle SeverStepTimerHandle;
    FTimerHandle VerificationTimerHandle;
    FTimerHandle BloodStrokeVerificationTimerHandle;
    int32 CurrentSeverStep = 0;
    int32 BloodStrokeVisualStep = 0;
    float BloodStrokeLoadedDistance = 0.0f;
    float BloodStrokeTravelDistance = 0.0f;
    FVector BloodStrokePoolLocation = FVector::ZeroVector;
    FVector BloodStrokeSurfaceNormal = FVector::UpVector;
    FVector BloodStrokeSurfaceTangent = FVector::ForwardVector;
    bool bBloodStrokeSmokeTestRequested = false;
};
