#include "Test/CMTestAreaRowWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Player/CMPlayerController.h"

// Area 정보와 호스트 선택 가능 여부를 행 위젯에 반영
void UCMTestAreaRowWidget::InitializeArea(
    FName NewAreaId,
    const FText& NewDisplayName,
    bool bSelectionEnabled)
{
    AreaId = NewAreaId;
    if (Txt_AreaName)
    {
        Txt_AreaName->SetText(NewDisplayName);
    }
    if (Btn_Select)
    {
        Btn_Select->SetIsEnabled(bSelectionEnabled);
    }
}

// 선택 버튼 클릭 이벤트 연결
void UCMTestAreaRowWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (Btn_Select)
    {
        Btn_Select->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSelectClicked);
    }
}

// 위젯 제거 시 선택 버튼 이벤트 해제
void UCMTestAreaRowWidget::NativeDestruct()
{
    if (Btn_Select)
    {
        Btn_Select->OnClicked.RemoveAll(this);
    }
    Super::NativeDestruct();
}

// 선택 Area를 공용 키메라 이동 서버 요청으로 전달
void UCMTestAreaRowWidget::HandleSelectClicked()
{
    if (ACMPlayerController* PlayerController = GetOwningPlayer<ACMPlayerController>())
    {
        PlayerController->RequestTeleportToTestArea(AreaId);
    }
}
