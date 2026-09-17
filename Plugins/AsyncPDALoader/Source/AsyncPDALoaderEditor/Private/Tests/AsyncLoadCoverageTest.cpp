#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AsyncLoadTestUtils.h"
#include "Tests/AsyncLoadTestSettings.h"
#include "AsyncPDALoader.h"
#include "AsyncLoadRequestMessage.h"
#include "AsyncLoadCompleteMessage.h"
#include "AsyncPDALoaderTags.h"
#include "IAsyncLoadScopedRequirementSource.h"
#include "IAsyncLoadScheduleProvider.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Subsystems/WorldSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFramework/GameplayMessageSubsystem.h"

namespace AsyncLoadCoverageTestHelpers
{
	UGameInstance* FindPIEGameInstance()
	{
		if (!GEngine)
		{
			return nullptr;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.OwningGameInstance)
			{
				return Context.OwningGameInstance;
			}
		}

		return nullptr;
	}

	UWorld* FindPIEWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World())
			{
				return Context.World();
			}
		}

		return nullptr;
	}

	// World와 GameInstance에서 scoped requirement source를 찾는다.
	TArray<IAsyncLoadScopedRequirementSource*> FindScopedRequirementSources(UWorld* World, UGameInstance* GameInstance)
	{
		TArray<IAsyncLoadScopedRequirementSource*> Found;

		if (World)
		{
			for (UWorldSubsystem* Subsystem : World->GetSubsystemArrayCopy<UWorldSubsystem>())
			{
				if (IAsyncLoadScopedRequirementSource* Source = Cast<IAsyncLoadScopedRequirementSource>(Subsystem))
				{
					Found.Add(Source);
				}
			}
		}

		if (GameInstance)
		{
			for (UGameInstanceSubsystem* Subsystem : GameInstance->GetSubsystemArrayCopy<UGameInstanceSubsystem>())
			{
				if (IAsyncLoadScopedRequirementSource* Source = Cast<IAsyncLoadScopedRequirementSource>(Subsystem))
				{
					Found.Add(Source);
				}
			}
		}

		return Found;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAsyncLoadCoverageTest,
	"NetKarma.Async.LoadCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// 실제 schedule을 재생해 로드 실패, 누락 등록, 늦은 등록을 검사한다.
// 게임이 연결한 provider를 그대로 사용하며 테스트가 provider를 교체하지 않는다.
class FAsyncLoadCoverageCheckCommand : public IAutomationLatentCommand
{
public:
	explicit FAsyncLoadCoverageCheckCommand(FAutomationTestBase* InTest, const UAsyncLoadTestSettings* InSettings)
		: Test(InTest)
		, Settings(InSettings)
	{
	}

	virtual bool Update() override
	{
		switch (Phase)
		{
		case EPhase::Setup:
			return DoSetup();
		case EPhase::WaitForScheduleConnected:
			return DoWaitForScheduleConnected();
		case EPhase::WarmUp:
			return DoWarmUp();
		case EPhase::CheckResults:
			return DoCheckResults();
		}
			return true;
		}

private:
	enum class EPhase { Setup, WaitForScheduleConnected, WarmUp, CheckResults };

	struct FPendingScopeCheck
	{
		FString SourceName;
		int32 Scope = INDEX_NONE;
		TArray<FPrimaryAssetId> RequiredIds;
	};

	bool DoSetup()
	{
		GameInstance = AsyncLoadCoverageTestHelpers::FindPIEGameInstance();
		UWorld* World = AsyncLoadCoverageTestHelpers::FindPIEWorld();
		if (!GameInstance || !World)
		{
			Test->AddError(TEXT("[LoadCoverage] PIE GameInstance/World를 찾지 못했습니다."));
			return true;
		}

		Loader = GameInstance->GetSubsystem<UAsyncPDALoader>();
		if (!Loader)
		{
			Test->AddError(TEXT("[LoadCoverage] UAsyncPDALoader 서브시스템을 찾지 못했습니다."));
			return true;
		}

		// 모든 Scope별 요구사항을 수집한다.
		const TArray<IAsyncLoadScopedRequirementSource*> Sources =
			AsyncLoadCoverageTestHelpers::FindScopedRequirementSources(World, GameInstance);
		if (Sources.Num() < Settings->MinimumScopedRequirementSourceCount)
		{
			Test->AddError(FString::Printf(
				TEXT("[LoadCoverage] Scope별 requirement source가 기대보다 적습니다. ExpectedAtLeast=%d Actual=%d"),
				Settings->MinimumScopedRequirementSourceCount, Sources.Num()));
			return true;
		}

		for (IAsyncLoadScopedRequirementSource* Source : Sources)
		{
			UObject* SourceObject = Cast<UObject>(Source);
			const FString SourceName = SourceObject ? SourceObject->GetClass()->GetName() : TEXT("Unknown");

			for (const int32 Scope : Source->GetKnownScopes())
			{
				FPendingScopeCheck Check;
				Check.SourceName = SourceName;
				Check.Scope = Scope;
				Check.RequiredIds = Source->GetRequiredAssetIdsForScope(Scope);
				if (!Check.RequiredIds.IsEmpty())
				{
					PendingChecks.Add(MoveTemp(Check));
				}
			}
		}

		if (PendingChecks.Num() < Settings->MinimumScopedRequirementCheckCount)
		{
			Test->AddError(FString::Printf(
				TEXT("[LoadCoverage] 실제 required asset 검사 묶음이 기대보다 적습니다. ExpectedAtLeast=%d Actual=%d"),
				Settings->MinimumScopedRequirementCheckCount, PendingChecks.Num()));
			return true;
		}

		StartTime = FPlatformTime::Seconds();
		Phase = EPhase::WaitForScheduleConnected;
		return DoWaitForScheduleConnected();
	}

	// 게임이 provider를 연결할 때까지 기다린다.
	bool DoWaitForScheduleConnected()
	{
		Provider = Loader->GetActiveScheduleProvider();
		if (Provider)
		{
			ReachablePairs = Provider->GetReachableScopeTimingPairs();

			if (ReachablePairs.IsEmpty())
			{
				Test->AddError(TEXT("[LoadCoverage] Provider가 도달 가능한 (Scope,Timing) 조합을 하나도 선언하지 않았습니다."));
				return true;
			}

			WarmUpIndex = 0;
			Phase = EPhase::WarmUp;
			return DoWarmUp();
		}

		if (FPlatformTime::Seconds() - StartTime > Settings->RequestTimeoutSeconds)
		{
			Test->AddError(TEXT("[LoadCoverage] 스케줄 Provider 연결을 기다리다 타임아웃됐습니다 — 게임의 부팅 로직이 SetActiveScheduleProvider를 호출하지 않았을 가능성."));
			return true;
		}

		return false;
	}

	// reachable pair를 순서대로 재생한다.
	bool DoWarmUp()
	{
		if (WarmUpIndex >= ReachablePairs.Num())
		{
			Phase = EPhase::CheckResults;
			return DoCheckResults();
		}

		const TPair<int32, FGameplayTag>& CurrentPair = ReachablePairs[WarmUpIndex];

		if (!bWarmUpRequestSent)
		{
			bWarmUpRequestSent = true;
			const FGuid CorrelationId = FGuid::NewGuid();

			Waiter = FAsyncPDALoaderCompletionWaiter();
			Waiter.Begin(GameInstance, Settings->RequestTimeoutSeconds, CurrentPair.Value, CorrelationId);

			FAsyncLoadRequestMessage Msg;
			Msg.CorrelationId = CorrelationId;
			Msg.Scope = CurrentPair.Key;
			Msg.TimingTag = CurrentPair.Value;
			Msg.LoadPriority = 50;
			Msg.AssetBundles = Settings->AssetBundlesToRequest;
			UGameplayMessageSubsystem::Get(GameInstance).BroadcastMessage(AsyncPDALoaderTags::Message_Load_Request, Msg);
		}

		FAsyncLoadCompleteMessage Result;
		bool bTimedOut = false;
		if (!Waiter.Poll(Result, bTimedOut))
		{
			return false;
		}

		if (bTimedOut)
		{
			Test->AddError(FString::Printf(
				TEXT("[LoadCoverage] 웜업 요청이 타임아웃돼 이후 요청을 중단합니다. Scope=%d Timing=%s Timeout=%.1fs"),
				CurrentPair.Key, *CurrentPair.Value.ToString(), Settings->RequestTimeoutSeconds));
			return true;
		}
		else
		{
			ReportLoadFailures(Result, FString::Printf(TEXT("Scope=%d %s"), CurrentPair.Key, *CurrentPair.Value.ToString()));
		}

		// readiness 시점의 캐시 상태로 늦은 등록을 검사한다.
		if (Settings->ReadinessCheckpointTiming.IsValid() && CurrentPair.Value.MatchesTagExact(Settings->ReadinessCheckpointTiming))
		{
			CheckTimingForScope(CurrentPair.Key);
		}

		bWarmUpRequestSent = false;
		++WarmUpIndex;
		return DoWarmUp();
	}

	void ReportLoadFailures(const FAsyncLoadCompleteMessage& Result, const FString& TimingLabel)
	{
		if (Result.Result != EAsyncLoadResult::Succeeded)
		{
			Test->AddError(FString::Printf(
				TEXT("[LoadCoverage][LoadFailure] %s 요청이 성공하지 못했습니다: Result=%d"),
				*TimingLabel, static_cast<int32>(Result.Result)));
		}

		for (const FPrimaryAssetId& FailedId : Result.FailedAssetIds)
		{
			Test->AddError(FString::Printf(TEXT("[LoadCoverage][LoadFailure] %s에 등록된 자산이 로드에 실패했습니다: %s"),
				*TimingLabel, *FailedId.ToString()));
		}
	}

	void CheckTimingForScope(int32 Scope)
	{
		for (const FPendingScopeCheck& Check : PendingChecks)
		{
			if (Check.Scope != Scope)
			{
				continue;
			}

			for (const FPrimaryAssetId& RequiredId : Check.RequiredIds)
			{
				if (!Loader->FindCachedAsset(RequiredId))
				{
					Test->AddError(FString::Printf(
						TEXT("[LoadCoverage][LateRegistration] %s가 Scope=%d에 필요하다고 했는데 체크포인트 시점(%s)까지 로드되지 않았습니다: %s — 실제 필요 시점보다 늦은 (Scope,Timing)에 등록됐을 가능성"),
						*Check.SourceName, Scope, *Settings->ReadinessCheckpointTiming.ToString(), *RequiredId.ToString()));
				}
			}
		}
	}

	bool DoCheckResults()
	{
		for (const FPendingScopeCheck& Check : PendingChecks)
		{
			for (const FPrimaryAssetId& RequiredId : Check.RequiredIds)
			{
				if (!Loader->FindCachedAsset(RequiredId))
				{
					Test->AddError(FString::Printf(
						TEXT("[LoadCoverage][MissingRegistration] %s가 Scope=%d에 필요하다고 한 자산이 재생 가능한 (Scope,Timing) 전체를 다 재생해도 로드되지 않았습니다: %s — 어떤 타이밍에도 등록이 안 됐을 가능성"),
						*Check.SourceName, Check.Scope, *RequiredId.ToString()));
				}
			}
		}

		Test->AddInfo(FString::Printf(
			TEXT("[LoadCoverage] 검증 완료. ReachablePairs=%d ScopedChecks=%d"),
			ReachablePairs.Num(), PendingChecks.Num()));

		return true;
	}

	FAutomationTestBase* Test;
	const UAsyncLoadTestSettings* Settings;
	UGameInstance* GameInstance = nullptr;
	UAsyncPDALoader* Loader = nullptr;
	TScriptInterface<IAsyncLoadScheduleProvider> Provider;
	TArray<TPair<int32, FGameplayTag>> ReachablePairs;
	FAsyncPDALoaderCompletionWaiter Waiter;
	TArray<FPendingScopeCheck> PendingChecks;
	int32 WarmUpIndex = 0;
	bool bWarmUpRequestSent = false;
	double StartTime = 0.0;
	EPhase Phase = EPhase::Setup;
};

// Provider가 선언한 모든 reachable Scope·Timing을 실제 요청해 cook·bundle 누락 검증
bool FAsyncLoadCoverageTest::RunTest(const FString& Parameters)
{
	const UAsyncLoadTestSettings* Settings = GetDefault<UAsyncLoadTestSettings>();
	if (!Settings || Settings->TestMapPath.IsNull())
	{
		AddError(TEXT("[LoadCoverage] UAsyncLoadTestSettings::TestMapPath가 설정돼있지 않습니다 — Project Settings > Plugins > Async Load Test Settings에서 지정하세요."));
		return true;
	}

	for (const FString& Pattern : Settings->CommonNoisePatterns)
	{
		AddExpectedErrorPlain(Pattern, EAutomationExpectedErrorFlags::Contains, -1);
	}
	for (const FString& Pattern : Settings->KnownTimingNoisePatterns)
	{
		AddExpectedErrorPlain(Pattern, EAutomationExpectedErrorFlags::Contains, -1);
	}

	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Settings->TestMapPath.GetLongPackageName()));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FAsyncLoadCoverageCheckCommand(this, Settings));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
