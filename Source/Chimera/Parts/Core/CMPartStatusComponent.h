#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "CMPartStatusComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCMPartStatusChangedSignature);

UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
// 개별 파츠의 감전, 경직, 감속처럼 ASC를 사용하지 않는 런타임 상태 관리
class CHIMERA_API UCMPartStatusComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMPartStatusComponent();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    // 같은 발생원의 같은 상태는 중복하지 않고 지속시간과 수치를 갱신
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Part Status")
    void ApplyStatus(
        FGameplayTag StatusTag,
        float Duration,
        float MovementMultiplier,
        bool bBlocksAbility,
        UObject* Source
    );

    // 장판 이탈처럼 특정 발생원이 부여한 상태만 제거
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Part Status")
    void RemoveStatus(FGameplayTag StatusTag, UObject* Source);

    // 파츠 탈착과 파괴 시 남은 상태와 만료 타이머 정리
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Part Status")
    void ClearAllStatuses();

    UFUNCTION(BlueprintPure, Category = "Chimera|Part Status")
    bool HasStatus(FGameplayTag StatusTag) const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Part Status")
    bool BlocksAbility() const { return bAbilityBlocked; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Part Status")
    float GetMovementMultiplier() const
    {
        return RuntimeMovementMultiplier;
    }

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Part Status")
    FCMPartStatusChangedSignature OnStatusChanged;

private:
    struct FActivePartStatus
    {
        int32 Handle = INDEX_NONE;
        FGameplayTag StatusTag;
        TWeakObjectPtr<UObject> Source;
        float MovementMultiplier = 1.0f;
        bool bBlocksAbility = false;
        FTimerHandle ExpirationTimer;
    };

    // 만료된 하나의 상태를 Handle로 찾아 제거
    void HandleStatusExpired(int32 StatusHandle);

    // 활성 상태 목록에서 복제할 태그와 최종 배율을 다시 계산
    void RecalculateAggregates();

    UFUNCTION()
    void OnRep_StatusState();

    UPROPERTY(ReplicatedUsing = OnRep_StatusState)
    FGameplayTagContainer ActiveStatusTags;

    UPROPERTY(ReplicatedUsing = OnRep_StatusState)
    float RuntimeMovementMultiplier = 1.0f;

    UPROPERTY(ReplicatedUsing = OnRep_StatusState)
    bool bAbilityBlocked = false;

    TArray<FActivePartStatus> ActiveStatuses;
    int32 NextStatusHandle = 1;
};
