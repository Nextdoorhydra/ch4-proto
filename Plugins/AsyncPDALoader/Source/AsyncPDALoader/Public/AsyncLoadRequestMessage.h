#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AsyncLoadRequestMessage.generated.h"

// Scope와 Timing에 해당하는 PDA 로드를 요청한다.
USTRUCT(BlueprintType)
struct FAsyncLoadRequestMessage
{
	GENERATED_BODY()

	// 완료 메시지와 요청을 연결한다. 비어 있으면 로더가 생성한다.
	UPROPERTY(BlueprintReadOnly, Category = "Load")
	FGuid CorrelationId;

	// 게임이 정의한 로드 범위. INDEX_NONE은 공통 범위다.
	UPROPERTY(BlueprintReadOnly, Category = "Load")
	int32 Scope = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Load")
	FGameplayTag TimingTag;

	// 스트리밍 우선순위. 높을수록 먼저 처리된다.
	UPROPERTY(BlueprintReadOnly, Category = "Load")
	int32 LoadPriority = 0;

	// 요청할 Asset Bundle 목록.
	UPROPERTY(BlueprintReadOnly, Category = "Load")
	TArray<FName> AssetBundles;
};

