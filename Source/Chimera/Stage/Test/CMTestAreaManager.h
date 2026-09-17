#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Stage/Test/CMTestAreaTypes.h"

#include "CMTestAreaManager.generated.h"

class ACMChimera;
class APlayerStart;
struct FCMTestAreaEntry;
class UCMTestAreaDefinition;
class ULevel;

UCLASS()
// Test 서브레벨 순서·초기화·공용 키메라 텔레포트 관리
class CHIMERA_API ACMTestAreaManager : public AActor
{
    GENERATED_BODY()

public:
    ACMTestAreaManager();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Definition 배열 순서로 Test Area UI 정보 반환
    UFUNCTION(BlueprintCallable, Category = "Chimera|Test Area")
    TArray<FCMTestAreaInfo> GetAvailableAreas() const;

    // 서버에서 대상 Area를 초기화하고 공용 키메라를 시작점으로 이동
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Test Area")
    bool TeleportToArea(FName AreaId);

    // 목적지 Area 다음 순서 또는 첫 번째 Area로 순환 이동
    bool AdvanceToNextArea(FName CompletedAreaId, ACMChimera* Chimera);

    // Test Actor가 속한 Sublevel의 시작점으로 공용 키메라 복귀
    bool RestartAreaForLevel(const ULevel* SourceLevel, ACMChimera* Chimera);

    UFUNCTION(BlueprintPure, Category = "Chimera|Test Area")
    FName GetCurrentAreaId() const { return CurrentAreaId; }

    // Definition 첫 구역의 PlayerStart를 최초 키메라 생성 위치로 제공
    APlayerStart* GetInitialPlayerStart() const;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Test Area")
    FCMTestAreaChangedSignature OnTestAreaChanged;

protected:
    virtual void BeginPlay() override;

private:
    APlayerStart* FindPlayerStart(FName StartTag) const;
    bool IsPlayerStartInAreaLevel(
        const APlayerStart* PlayerStart,
        const FCMTestAreaEntry& Area) const;
    ACMChimera* FindSharedChimera() const;
    void ResetArea(const APlayerStart* PlayerStart) const;
    bool TeleportChimeraToArea(ACMChimera* Chimera, FName AreaId);

    UFUNCTION()
    void OnRep_CurrentAreaId();

    UPROPERTY(ReplicatedUsing = OnRep_CurrentAreaId)
    FName CurrentAreaId;

    // Test Persistent Level 내부 구역 순서와 PlayerStart 태그 설정
    UPROPERTY(EditInstanceOnly, Category = "Chimera|Test Area")
    TObjectPtr<UCMTestAreaDefinition> AreaDefinition;
};
