#include "Test/CMTestHUD.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraTestHUD, Log, All);

// 로컬 플레이어에게 Test Overlay를 생성하고 게임·UI 동시 입력 설정
void ACMTestHUD::BeginPlay()
{
    Super::BeginPlay();

#if UE_BUILD_SHIPPING
    return;
#else
    APlayerController* PlayerController = GetOwningPlayerController();
    if (!PlayerController || !PlayerController->IsLocalController())
    {
        return;
    }
    if (!TestOverlayClass)
    {
        UE_LOG(LogChimeraTestHUD, Warning,
            TEXT("TestHUD에 TestOverlayClass가 설정되지 않았습니다. HUD=%s"),
            *GetName());
        return;
    }

    TestOverlay = CreateWidget<UUserWidget>(PlayerController, TestOverlayClass);
    if (!TestOverlay)
    {
        UE_LOG(LogChimeraTestHUD, Error,
            TEXT("Test Overlay 생성에 실패했습니다. Class=%s"),
            *GetNameSafe(TestOverlayClass));
        return;
    }

    TestOverlay->AddToViewport();
    PlayerController->bShowMouseCursor = bShowTestMouseCursor;
    PlayerController->SetInputMode(FInputModeGameAndUI());
#endif
}

// 맵 종료 시 Test Overlay를 제거하고 게임 입력 상태로 복원
void ACMTestHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (TestOverlay)
    {
        TestOverlay->RemoveFromParent();
        TestOverlay = nullptr;
    }

    if (APlayerController* PlayerController = GetOwningPlayerController())
    {
        PlayerController->bShowMouseCursor = false;
        PlayerController->SetInputMode(FInputModeGameOnly());
    }
    Super::EndPlay(EndPlayReason);
}
