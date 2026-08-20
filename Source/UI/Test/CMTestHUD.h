#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"

#include "CMTestHUD.generated.h"

class UUserWidget;

UCLASS(Blueprintable)
// Test 맵의 로컬 플레이어에게 구역 이동 UI를 생성하고 입력 모드 관리
class UI_API ACMTestHUD : public AHUD
{
    GENERATED_BODY()

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 화면에 생성할 Test 전용 최상위 Widget Blueprint
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Testing")
    TSubclassOf<UUserWidget> TestOverlayClass;

    // Test HUD 생성 시 UI 클릭을 위한 마우스 커서 표시
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Testing")
    bool bShowTestMouseCursor = true;

private:
    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> TestOverlay;
};
