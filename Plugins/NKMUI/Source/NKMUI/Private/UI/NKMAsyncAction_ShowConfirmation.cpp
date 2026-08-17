#include "UI/NKMAsyncAction_ShowConfirmation.h"
#include "Engine/GameInstance.h"
#include "UI/Subsystem/NKMUIManagerSubsystem.h"
#include "UI/NKMUITagList.h" // UI.Layer.Modal 태그를 가져오기 위함

UNKMAsyncAction_ShowConfirmation* UNKMAsyncAction_ShowConfirmation::ShowConfirmation(UObject* WorldContextObject, TSubclassOf<UNKMUIDialogBase> DialogClass, FNKMUIDialogDescriptor Descriptor)
{
	UNKMAsyncAction_ShowConfirmation* Action = NewObject<UNKMAsyncAction_ShowConfirmation>();
	Action->WorldContextObject = WorldContextObject;
	Action->TargetDialogClass = DialogClass;
	Action->TargetDescriptor = Descriptor;
	if (WorldContextObject)
	{
		Action->RegisterWithGameInstance(WorldContextObject);
	}

	return Action;
}

void UNKMAsyncAction_ShowConfirmation::Activate()
{
	if (!WorldContextObject || !TargetDialogClass)
	{
		HandleDialogClosed(ENKMUIDialogResult::Ignored);
		return;
	}

	UWorld* World = WorldContextObject->GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	if (!GI)
	{
		HandleDialogClosed(ENKMUIDialogResult::Ignored);
		return;
	}

	if (UNKMUIManagerSubsystem* UIManager = GI->GetSubsystem<UNKMUIManagerSubsystem>())
	{
		// 이전에 만든 UIManager를 통해 Modal Layer에 팝업 Push
		UNKMUIActivatableWidget* Widget = Cast<UNKMUIActivatableWidget>(
			UIManager->PushWidget(UITags::UI_Layer_Modal, TargetDialogClass));

		if (UNKMUIDialogBase* Dialog = Cast<UNKMUIDialogBase>(Widget))
		{
			// 데이터 주입 및 종료 콜백 연결
			Dialog->SetupDialog(TargetDescriptor);
			Dialog->OnDialogClosed.AddDynamic(this, &UNKMAsyncAction_ShowConfirmation::HandleDialogClosed);
			return;
		}
	}

	// 실패 시 무시됨 처리
	HandleDialogClosed(ENKMUIDialogResult::Ignored);
}

void UNKMAsyncAction_ShowConfirmation::HandleDialogClosed(ENKMUIDialogResult Result)
{
	if (bCompleted)
	{
		return;
	}
	bCompleted = true;

	// 팝업이 닫히면 블루프린트 핀으로 신호를 보내고, 비동기 노드 스스로를 메모리에서 해제합니다.
	OnResult.Broadcast(Result);
	SetReadyToDestroy();
}
