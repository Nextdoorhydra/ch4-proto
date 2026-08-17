#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/GameInstance.h"
#include "UI/Subsystem/NKMUIAssetResidencySubsystem.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
struct FNKMUIResidencyTestState
{
	TStrongObjectPtr<UGameInstance> GameInstance;
	TStrongObjectPtr<UNKMUIAssetResidencySubsystem> Residency;
	TStrongObjectPtr<UNKMUIAssetLease> FirstLease;
	TStrongObjectPtr<UNKMUIAssetLease> SecondLease;
	ENKMUIAssetLoadResult FirstResult = ENKMUIAssetLoadResult::InvalidRequest;
	ENKMUIAssetLoadResult SecondResult = ENKMUIAssetLoadResult::InvalidRequest;
	int32 CompletionCount = 0;
	double StartTimeSeconds = 0.0;
};
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
	FNKMUIWaitForResidencyLeases,
	TSharedPtr<FNKMUIResidencyTestState>, State,
	FAutomationTestBase*, Test);

bool FNKMUIWaitForResidencyLeases::Update()
{
	if (State->CompletionCount < 2)
	{
		if (FPlatformTime::Seconds() - State->StartTimeSeconds < 10.0)
		{
			return false;
		}

		Test->AddError(TEXT("Timed out while waiting for UI asset residency leases"));
		return true;
	}

	Test->TestEqual(
		TEXT("First lease succeeds"),
		State->FirstResult,
		ENKMUIAssetLoadResult::Succeeded);
	Test->TestEqual(
		TEXT("Second lease succeeds"),
		State->SecondResult,
		ENKMUIAssetLoadResult::Succeeded);

	TArray<UObject*> FirstAssets;
	TArray<UObject*> SecondAssets;
	State->FirstLease->GetLoadedAssets(FirstAssets);
	State->SecondLease->GetLoadedAssets(SecondAssets);
	Test->TestEqual(TEXT("First lease resolves one asset"), FirstAssets.Num(), 1);
	Test->TestEqual(TEXT("Second lease resolves one asset"), SecondAssets.Num(), 1);
	if (FirstAssets.Num() == 1 && SecondAssets.Num() == 1)
	{
		Test->TestTrue(
			TEXT("Independent leases reuse the same UObject"),
			FirstAssets[0] == SecondAssets[0]);
	}

	State->FirstLease->Release();
	SecondAssets.Reset();
	State->SecondLease->GetLoadedAssets(SecondAssets);
	Test->TestEqual(
		TEXT("Releasing one owner keeps the other lease resident"),
		SecondAssets.Num(),
		1);
	State->SecondLease->Release();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNKMUIAssetResidencyContractTest,
	"NKMUI.Assets.Residency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNKMUIAssetResidencyContractTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FNKMUIResidencyTestState> State = MakeShared<FNKMUIResidencyTestState>();
	State->GameInstance.Reset(NewObject<UGameInstance>());
	State->Residency.Reset(
		NewObject<UNKMUIAssetResidencySubsystem>(State->GameInstance.Get()));
	State->StartTimeSeconds = FPlatformTime::Seconds();

	const TSoftObjectPtr<UObject> EngineTexture(
		FSoftObjectPath(TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture")));
	const TArray<TSoftObjectPtr<UObject>> Assets = { EngineTexture };

	State->FirstLease.Reset(State->Residency->AcquireAssetsAsync(
		Assets,
		FNKMUIAssetLeaseCompletedNative::CreateLambda(
			[State](ENKMUIAssetLoadResult Result, UNKMUIAssetLease*)
			{
				State->FirstResult = Result;
				++State->CompletionCount;
			})));
	State->SecondLease.Reset(State->Residency->AcquireAssetsAsync(
		Assets,
		FNKMUIAssetLeaseCompletedNative::CreateLambda(
			[State](ENKMUIAssetLoadResult Result, UNKMUIAssetLease*)
			{
				State->SecondResult = Result;
				++State->CompletionCount;
			})));

	ADD_LATENT_AUTOMATION_COMMAND(FNKMUIWaitForResidencyLeases(State, this));
	return true;
}

#endif
