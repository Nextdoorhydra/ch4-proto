#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPtr.h"
#include "NKMUIManagerSettings.generated.h"

class UNKMUIPolicy;
class UNKMUIExtensionData;

// defaultconfig: DefaultGame.ini에 저장
// DisplayName: 에디터 프로젝트 세팅 목록에 보여질 이름
UCLASS(Config=Game, defaultconfig, meta = (DisplayName = "NKM UI Manager"))
class NKMUI_API UNKMUIManagerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UNKMUIManagerSettings();

	UPROPERTY(Config, EditAnywhere, Category = "UI")
	TSoftClassPtr<UNKMUIPolicy> DefaultPolicyClass;

	UPROPERTY(Config, EditAnywhere, Category = "UI|Extension")
	TArray<TSoftObjectPtr<UNKMUIExtensionData>> ExtensionDataAssets;
};
