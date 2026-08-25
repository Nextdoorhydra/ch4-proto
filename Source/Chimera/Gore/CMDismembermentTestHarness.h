#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CMDismembermentTestHarness.generated.h"

class ACharacter;

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

    UFUNCTION(BlueprintPure, Category = "Dismemberment Test")
    ACharacter* GetSpawnedSubject() const
    {
        return SpawnedSubject;
    }

private:
    void HandleDeathTimerElapsed();
    void HandleSeverStepElapsed();
    void VerifyTestDeath();
    bool EnsureDismembermentComponent();
    class USkeletalMeshComponent* FindSubjectPartMesh(FName ComponentName) const;

    UPROPERTY(Transient)
    TObjectPtr<ACharacter> SpawnedSubject;

    FTimerHandle DeathTimerHandle;
    FTimerHandle SeverStepTimerHandle;
    FTimerHandle VerificationTimerHandle;
    int32 CurrentSeverStep = 0;
};
