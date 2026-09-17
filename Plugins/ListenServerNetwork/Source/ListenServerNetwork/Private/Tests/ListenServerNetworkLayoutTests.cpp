#if WITH_DEV_AUTOMATION_TESTS

#include "ListenServerNetworkPolicy.h"
#include "ListenServerNetworkSettings.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FListenServerLayoutTest, "ListenServerNetwork.Layout.Structures", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FListenServerLayoutTest::RunTest(const FString& Parameters)
{
	AddInfo(FString::Printf(TEXT("FOperationContextPod sizeof=%llu alignof=%llu"), sizeof(ListenServerNetworkPolicy::FOperationContextPod), alignof(ListenServerNetworkPolicy::FOperationContextPod)));
	AddInfo(FString::Printf(TEXT("FSearchCandidatePod sizeof=%llu alignof=%llu"), sizeof(ListenServerNetworkPolicy::FSearchCandidatePod), alignof(ListenServerNetworkPolicy::FSearchCandidatePod)));
	AddInfo(FString::Printf(TEXT("FListenServerSearchResultHandle sizeof=%llu alignof=%llu"), sizeof(FListenServerSearchResultHandle), alignof(FListenServerSearchResultHandle)));
	AddInfo(FString::Printf(TEXT("FListenServerSessionAttribute sizeof=%llu alignof=%llu"), sizeof(FListenServerSessionAttribute), alignof(FListenServerSessionAttribute)));
	TestTrue(TEXT("Operation context remains compact"), sizeof(ListenServerNetworkPolicy::FOperationContextPod) <= 24);
	TestTrue(TEXT("Search candidate remains compact"), sizeof(ListenServerNetworkPolicy::FSearchCandidatePod) <= 16);
	TestEqual(TEXT("Search handle stores two int32 values"), sizeof(FListenServerSearchResultHandle), static_cast<SIZE_T>(8));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FListenServerSettingsDefaultsTest, "ListenServerNetwork.Policy.SettingsDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FListenServerSettingsDefaultsTest::RunTest(const FString& Parameters)
{
	const UListenServerNetworkSettings* Settings = GetDefault<UListenServerNetworkSettings>();
	TestFalse(TEXT("Default ProjectKey is non-empty"), Settings->ProjectKey.IsEmpty());
	TestTrue(TEXT("BuildUniqueId is positive"), Settings->BuildUniqueId > 0);
	TestTrue(TEXT("DefaultMaxPlayers is positive"), Settings->DefaultMaxPlayers > 0);
	TestTrue(TEXT("DefaultMaxSearchResults is positive"), Settings->DefaultMaxSearchResults > 0);
	TestTrue(TEXT("Create timeout is positive"), Settings->CreateTimeoutSeconds > 0.0f);
	TestTrue(TEXT("Search timeout is positive"), Settings->SearchTimeoutSeconds > 0.0f);
	TestTrue(TEXT("Join timeout is positive"), Settings->JoinTimeoutSeconds > 0.0f);
	TestTrue(TEXT("Update timeout is positive"), Settings->UpdateTimeoutSeconds > 0.0f);
	TestTrue(TEXT("Destroy timeout is positive"), Settings->DestroyTimeoutSeconds > 0.0f);
	TestTrue(TEXT("Travel timeout is positive"), Settings->TravelTimeoutSeconds > 0.0f);
	return true;
}

#endif
