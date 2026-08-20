#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "AsyncLoadTestSettings.generated.h"

// 비동기 로드 Automation Test의 프로젝트별 설정이다.
UCLASS(Config = Game, DefaultConfig, DisplayName = "Async Load Test Settings")
class ASYNCPDALOADER_API UAsyncLoadTestSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// 테스트에 사용할 맵.
	UPROPERTY(Config, EditAnywhere, Category = "Async Load Tests")
	FSoftObjectPath TestMapPath;

	// LoadCoverage 요청에 사용할 bundle 목록.
	UPROPERTY(Config, EditAnywhere, Category = "Async Load Tests")
	TArray<FName> AssetBundlesToRequest;

	// 모든 비동기 로드 테스트에서 무시할 로그 패턴.
	UPROPERTY(Config, EditAnywhere, Category = "Async Load Tests")
	TArray<FString> CommonNoisePatterns;

	// LoadCoverage에서만 무시할 타이밍 관련 로그 패턴.
	UPROPERTY(Config, EditAnywhere, Category = "Async Load Tests")
	TArray<FString> KnownTimingNoisePatterns;

	// Scope별 자산이 준비돼야 하는 Timing.
	UPROPERTY(Config, EditAnywhere, Category = "Async Load Tests")
	FGameplayTag ReadinessCheckpointTiming;

	// 필요한 최소 scoped requirement source 수.
	UPROPERTY(Config, EditAnywhere, Category = "Async Load Tests", meta = (ClampMin = "0"))
	int32 MinimumScopedRequirementSourceCount = 0;

	// 필요한 최소 Scope별 검사항목 수.
	UPROPERTY(Config, EditAnywhere, Category = "Async Load Tests", meta = (ClampMin = "0"))
	int32 MinimumScopedRequirementCheckCount = 0;

	// provider 연결과 요청의 timeout.
	UPROPERTY(Config, EditAnywhere, Category = "Async Load Tests", meta = (ClampMin = "0.1"))
	float RequestTimeoutSeconds = 15.f;

	// SelfLoadingAssets에서 필요한 최소 설정 항목 수.
	UPROPERTY(Config, EditAnywhere, Category = "Async Load Tests", meta = (ClampMin = "0"))
	int32 MinimumConfiguredSelfLoadingCheckCount = 0;
};

