#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/GameInstance.h"
#include "UI/NKMUIExtensionData.h"
#include "UI/Subsystem/NKMUIAssetResidencySubsystem.h"
#include "UI/Subsystem/NKMUIManagerSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNKMUIManagerSubsystemContractTest,
	"NKMUI.Manager.Contracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNKMUIManagerSubsystemContractTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UNKMUIManagerSubsystem* Manager = NewObject<UNKMUIManagerSubsystem>(GameInstance);
	TestNotNull(TEXT("Manager can be constructed for contract testing"), Manager);
	if (!Manager)
	{
		return false;
	}

	TestTrue(
		TEXT("Leaf manager creates when no more concrete subclass exists"),
		Manager->ShouldCreateSubsystem(GameInstance));

	UNKMUIAssetResidencySubsystem* Residency =
		NewObject<UNKMUIAssetResidencySubsystem>(GameInstance);
	TestNotNull(TEXT("Asset residency subsystem can be constructed"), Residency);
	if (!Residency)
	{
		return false;
	}

	bool bInvalidLeaseCompleted = false;
	ENKMUIAssetLoadResult InvalidLeaseResult = ENKMUIAssetLoadResult::Succeeded;
	UNKMUIAssetLease* InvalidLease = Residency->AcquireAssetsAsync(
		TArray<TSoftObjectPtr<UObject>>(),
		FNKMUIAssetLeaseCompletedNative::CreateLambda(
			[&bInvalidLeaseCompleted, &InvalidLeaseResult](
				ENKMUIAssetLoadResult Result,
				UNKMUIAssetLease*)
			{
				bInvalidLeaseCompleted = true;
				InvalidLeaseResult = Result;
			}));
	TestNotNull(TEXT("Invalid residency request still returns a releasable lease"), InvalidLease);
	TestTrue(TEXT("Invalid residency request completes synchronously"), bInvalidLeaseCompleted);
	TestEqual(
		TEXT("Invalid residency request reports InvalidRequest"),
		InvalidLeaseResult,
		ENKMUIAssetLoadResult::InvalidRequest);
	TestEqual(
		TEXT("Invalid residency lease records failure"),
		InvalidLease->GetState(),
		ENKMUIAssetLeaseState::Failed);
	InvalidLease->Release();
	TestEqual(
		TEXT("Released residency lease records release"),
		InvalidLease->GetState(),
		ENKMUIAssetLeaseState::Released);

	bool bInvalidRequestCompleted = false;
	ENKMUIAsyncResult InvalidRequestResult = ENKMUIAsyncResult::Succeeded;
	Manager->RegisterExtensionsFromData(
		TSoftObjectPtr<UNKMUIExtensionData>(),
		FNKMUIExtensionRegistrationCompleted::CreateLambda(
			[&bInvalidRequestCompleted, &InvalidRequestResult](ENKMUIAsyncResult Result)
			{
				bInvalidRequestCompleted = true;
				InvalidRequestResult = Result;
			}));
	TestTrue(TEXT("Invalid extension request completes synchronously"), bInvalidRequestCompleted);
	TestEqual(
		TEXT("Invalid extension request reports InvalidRequest"),
		InvalidRequestResult,
		ENKMUIAsyncResult::InvalidRequest);
	TestEqual(
		TEXT("Invalid extension unregister reports InvalidRequest"),
		Manager->UnregisterExtensionsFromData(TSoftObjectPtr<UNKMUIExtensionData>(), true),
		ENKMUIAsyncResult::InvalidRequest);

	bool bVisibilityChanged = false;
	bool bReceivedVisibility = true;
	Manager->OnGameplayHUDVisibilityChanged().AddLambda(
		[&bVisibilityChanged, &bReceivedVisibility](bool bVisible)
		{
			bVisibilityChanged = true;
			bReceivedVisibility = bVisible;
		});
	Manager->SetGameplayHUDVisible(false);
	TestTrue(TEXT("HUD visibility change is broadcast"), bVisibilityChanged);
	TestFalse(TEXT("HUD visibility broadcast carries the new value"), bReceivedVisibility);
	TestFalse(TEXT("HUD visibility state is retained for late layouts"), Manager->IsGameplayHUDVisible());

	UNKMUIExtensionData* ReleasedExtensionData = NewObject<UNKMUIExtensionData>(Manager);
	bool bReleasedRequestCompleted = false;
	ENKMUIAsyncResult ReleasedRequestResult = ENKMUIAsyncResult::Succeeded;
	Manager->RegisterExtensionsFromData(
		TSoftObjectPtr<UNKMUIExtensionData>(ReleasedExtensionData),
		FNKMUIExtensionRegistrationCompleted::CreateLambda(
			[&bReleasedRequestCompleted, &ReleasedRequestResult](ENKMUIAsyncResult Result)
			{
				bReleasedRequestCompleted = true;
				ReleasedRequestResult = Result;
			}));
	TestFalse(
		TEXT("Loaded extension remains pending until a policy can register it"),
		bReleasedRequestCompleted);
	TestEqual(
		TEXT("Explicit extension release succeeds"),
		Manager->UnregisterExtensionsFromData(
			TSoftObjectPtr<UNKMUIExtensionData>(ReleasedExtensionData),
			true),
		ENKMUIAsyncResult::Succeeded);
	TestTrue(TEXT("Explicit extension release completes its pending request"), bReleasedRequestCompleted);
	TestEqual(
		TEXT("Explicit extension release reports cancellation"),
		ReleasedRequestResult,
		ENKMUIAsyncResult::Cancelled);

	UNKMUIExtensionData* ExtensionData = NewObject<UNKMUIExtensionData>(Manager);
	bool bPendingRequestCompleted = false;
	ENKMUIAsyncResult PendingRequestResult = ENKMUIAsyncResult::Succeeded;
	Manager->RegisterExtensionsFromData(
		TSoftObjectPtr<UNKMUIExtensionData>(ExtensionData),
		FNKMUIExtensionRegistrationCompleted::CreateLambda(
			[&bPendingRequestCompleted, &PendingRequestResult](ENKMUIAsyncResult Result)
			{
				bPendingRequestCompleted = true;
				PendingRequestResult = Result;
			}));
	TestFalse(
		TEXT("Loaded extension waits until a policy can register it"),
		bPendingRequestCompleted);

	Manager->ResetPolicy();
	TestTrue(TEXT("Reset completes pending extension requests"), bPendingRequestCompleted);
	TestEqual(
		TEXT("Reset reports cancellation"),
		PendingRequestResult,
		ENKMUIAsyncResult::Cancelled);

	return true;
}

#endif
