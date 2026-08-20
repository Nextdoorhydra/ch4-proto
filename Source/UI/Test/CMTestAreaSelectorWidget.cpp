#include "Test/CMTestAreaSelectorWidget.h"

#include "Components/ScrollBox.h"
#include "Player/CMPlayerController.h"
#include "Test/CMTestAreaRowWidget.h"

// Test맵 UI 생성 시 현재 로드된 Area 목록 구성
void UCMTestAreaSelectorWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshAreaList();
}

// AreaStart 목록으로 행 위젯을 만들고 비호스트 선택 버튼 비활성화
void UCMTestAreaSelectorWidget::RefreshAreaList()
{
    if (!SB_TestAreas || !AreaRowClass)
    {
        return;
    }

    SB_TestAreas->ClearChildren();
    ACMPlayerController* PlayerController = GetOwningPlayer<ACMPlayerController>();
    if (!PlayerController)
    {
        return;
    }

    const bool bCanSelect = PlayerController->CanControlTestAreas();
    for (const FCMTestAreaInfo& AreaInfo : PlayerController->GetAvailableTestAreas())
    {
        UCMTestAreaRowWidget* Row = CreateWidget<UCMTestAreaRowWidget>(
            PlayerController, AreaRowClass);
        if (!Row)
        {
            continue;
        }
        Row->InitializeArea(AreaInfo.AreaId, AreaInfo.DisplayName, bCanSelect);
        SB_TestAreas->AddChild(Row);
    }
}
