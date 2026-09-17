#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "NKMUIExtensionData.generated.h"

class UUserWidget;

// 단일 UI 확장 매핑 정보
USTRUCT(BlueprintType)
struct NKMUI_API FNKMUIExtensionEntry
{
	GENERATED_BODY()

	// UI를 주입할 목표 지점의 태그 (예: UI.Extension.HUD.HealthBar)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Extension")
	FGameplayTag ExtensionPointTag;

	// 주입할 위젯 클래스 (메모리 최적화를 위해 Soft Reference 사용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Extension")
	TSoftClassPtr<UUserWidget> WidgetClass;

	// 같은 위치에 들어갈 때의 정렬 순서 (낮을수록 먼저/아래쪽에 배치)
	UPROPERTY(EditAnywhere)
	int32 Priority = 0;
};

// UI 확장 데이터 목록을 관리하는 에셋
UCLASS(BlueprintType)
class NKMUI_API UNKMUIExtensionData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	void GetExtensionEntries(TArray<FNKMUIExtensionEntry>& OutEntries) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Extension")
	TArray<FNKMUIExtensionEntry> ExtensionEntries;
};
