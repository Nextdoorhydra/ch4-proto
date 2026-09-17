#include "UI/NKMUIExtensions.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

ULocalPlayer* UNKMUIExtensions::GetLocalPlayerFromController(APlayerController* PlayerController)
{
	return PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
}
