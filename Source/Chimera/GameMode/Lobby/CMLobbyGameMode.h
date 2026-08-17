#pragma once

#include "CoreMinimal.h"
#include "GameMode/CMGameMode.h"

#include "CMLobbyGameMode.generated.h"

class UCMCampaignDefinition;

UCLASS()
// 로비 참가자 입장·퇴장 처리 및 Ready 상태 집계 담당
class CHIMERA_API ACMLobbyGameMode : public ACMGameMode
{
    GENERATED_BODY()

public:
    ACMLobbyGameMode();

    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

    // 참가 인원·Ready 집계 결과 갱신 처리
    void RefreshLobbySummary();

    // 로비 조건과 캠페인 설정을 검증한 뒤 첫 스테이지 Travel 시작
    bool TryStartCampaign(APlayerController* RequestingPlayer);

protected:
    // 이 로비에서 시작할 정식 캠페인 PDA
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Campaign")
    TObjectPtr<UCMCampaignDefinition> CampaignDefinition;

private:
    bool bCampaignStartInProgress = false;
};
