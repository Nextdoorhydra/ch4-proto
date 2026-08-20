#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "CMTestAreaDefinition.generated.h"

class UWorld;

USTRUCT(BlueprintType)
struct CHIMERA_API FCMTestAreaEntry
{
    GENERATED_BODY()

    // 목적지와 UI 요청에서 사용하는 구역 식별자
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Test Area")
    FName AreaId;

    // 구역 Actor가 저장되어야 하는 Always Loaded Sublevel
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Test Area")
    TSoftObjectPtr<UWorld> AreaLevel;

    // 구역 도착 위치로 사용할 PlayerStart의 PlayerStartTag
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Test Area")
    FName StartTag;

    // 테스트 구역 선택 UI에 표시할 이름
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Test Area")
    FText DisplayName;
};

UCLASS(BlueprintType)
// 하나의 Test Persistent Level 안에서 이동할 Sublevel 순서와 시작점 태그 보관
class CHIMERA_API UCMTestAreaDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // 배열 순서가 목적지 도달 시 이동 순서이며 마지막 다음에는 첫 구역으로 순환
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Test Area",
        meta = (TitleProperty = "AreaId"))
    TArray<FCMTestAreaEntry> Areas;

    // AreaId에 해당하는 구역 설정 반환
    const FCMTestAreaEntry* FindArea(FName AreaId) const;

    // 완료한 구역 다음 또는 첫 번째 구역 설정 반환
    const FCMTestAreaEntry* FindNextArea(FName CompletedAreaId) const;

#if WITH_EDITOR
    // 저장 시 빈 값과 중복 AreaId·AreaLevel·StartTag를 콘텐츠 검증에 보고
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
