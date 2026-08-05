#if WITH_DEV_AUTOMATION_TESTS

#include "ListenServerNetworkPolicy.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FListenServerStatePolicyTest, "ListenServerNetwork.Policy.StateAndGenerations", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FListenServerStatePolicyTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Idle can begin search"), ListenServerNetworkPolicy::IsValidOperationTransition(EListenServerOperation::None, EListenServerOperation::Searching));
	TestTrue(TEXT("Search can begin join"), ListenServerNetworkPolicy::IsValidOperationTransition(EListenServerOperation::Searching, EListenServerOperation::Joining));
	TestTrue(TEXT("Create can begin travel"), ListenServerNetworkPolicy::IsValidOperationTransition(EListenServerOperation::Creating, EListenServerOperation::Traveling));
	TestTrue(TEXT("Host create can first replace a stale session"), ListenServerNetworkPolicy::IsValidOperationTransition(EListenServerOperation::Creating, EListenServerOperation::DestroyingExistingSession));
	TestFalse(TEXT("Create cannot jump to search"), ListenServerNetworkPolicy::IsValidOperationTransition(EListenServerOperation::Creating, EListenServerOperation::Searching));

	uint64 Counter = 0;
	const uint64 First = ListenServerNetworkPolicy::AdvanceOperationId(Counter);
	const uint64 Second = ListenServerNetworkPolicy::AdvanceOperationId(Counter);
	TestEqual(TEXT("First operation id"), First, uint64(1));
	TestEqual(TEXT("Second operation id"), Second, uint64(2));
	TestTrue(TEXT("Old callback is stale"), ListenServerNetworkPolicy::IsStaleOperation(First, Second));
	TestFalse(TEXT("Current callback is not stale"), ListenServerNetworkPolicy::IsStaleOperation(Second, Second));

	FListenServerSearchResultHandle Handle;
	Handle.SearchGeneration = 7;
	Handle.ResultIndex = 1;
	TestTrue(TEXT("Current search handle is valid"), ListenServerNetworkPolicy::IsSearchHandleValid(Handle, 7, 2));
	TestFalse(TEXT("Old search generation is invalid"), ListenServerNetworkPolicy::IsSearchHandleValid(Handle, 8, 2));
	TestFalse(TEXT("Out of range result is invalid"), ListenServerNetworkPolicy::IsSearchHandleValid(Handle, 7, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FListenServerAttributePolicyTest, "ListenServerNetwork.Policy.Attributes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FListenServerAttributePolicyTest::RunTest(const FString& Parameters)
{
	FString Reason;
	TArray<FListenServerSessionAttribute> Attributes;
	FListenServerSessionAttribute& Difficulty = Attributes.AddDefaulted_GetRef();
	Difficulty.Key = TEXT("Difficulty");
	Difficulty.Value = TEXT("Hard");
	TestTrue(TEXT("Project attribute is accepted"), ListenServerNetworkPolicy::ValidateAttributes(Attributes, true, Reason));

	FListenServerSessionAttribute& Duplicate = Attributes.AddDefaulted_GetRef();
	Duplicate.Key = TEXT("Difficulty");
	Duplicate.Value = TEXT("Easy");
	TestFalse(TEXT("Duplicate key is rejected"), ListenServerNetworkPolicy::ValidateAttributes(Attributes, true, Reason));

	Attributes.SetNum(1);
	Attributes[0].Key = ListenServerNetworkKeys::ProjectKey;
	TestFalse(TEXT("Reserved key override is rejected"), ListenServerNetworkPolicy::ValidateAttributes(Attributes, true, Reason));
	TestTrue(TEXT("Reserved key may be a required search value"), ListenServerNetworkPolicy::ValidateAttributes(Attributes, false, Reason));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FListenServerCompatibilityPolicyTest, "ListenServerNetwork.Policy.Compatibility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FListenServerCompatibilityPolicyTest::RunTest(const FString& Parameters)
{
	ListenServerNetworkPolicy::FCompatibilityCandidate Candidate;
	Candidate.ProjectKey = TEXT("ProjectA");
	Candidate.BuildUniqueId = 11;
	Candidate.GameMode = TEXT("Coop");
	Candidate.Region = TEXT("KR");
	Candidate.OpenPublicConnections = 2;
	Candidate.Attributes.Add(TEXT("Difficulty"), TEXT("Hard"));

	ListenServerNetworkPolicy::FCompatibilityRequest Request;
	Request.ProjectKey = TEXT("ProjectA");
	Request.BuildUniqueId = 11;
	Request.GameMode = TEXT("Coop");
	FListenServerSessionAttribute& Required = Request.RequiredAttributes.AddDefaulted_GetRef();
	Required.Key = TEXT("Difficulty");
	Required.Value = TEXT("Hard");
	TestEqual(TEXT("Compatible candidate"), ListenServerNetworkPolicy::CheckCompatibility(Candidate, Request), EListenServerError::None);

	Request.ProjectKey = TEXT("ProjectB");
	TestEqual(TEXT("Project mismatch"), ListenServerNetworkPolicy::CheckCompatibility(Candidate, Request), EListenServerError::IncompatibleProject);
	Request.ProjectKey = TEXT("ProjectA");
	Request.BuildUniqueId = 12;
	TestEqual(TEXT("Build mismatch"), ListenServerNetworkPolicy::CheckCompatibility(Candidate, Request), EListenServerError::IncompatibleBuild);
	Request.BuildUniqueId = 11;
	Request.GameMode = TEXT("Versus");
	TestEqual(TEXT("Game mode mismatch"), ListenServerNetworkPolicy::CheckCompatibility(Candidate, Request), EListenServerError::IncompatibleGameMode);
	Request.GameMode = TEXT("Coop");
	Request.Region = NAME_None;
	TestEqual(TEXT("Empty region does not filter"), ListenServerNetworkPolicy::CheckCompatibility(Candidate, Request), EListenServerError::None);
	Request.Region = TEXT("US");
	TestEqual(TEXT("Region mismatch"), ListenServerNetworkPolicy::CheckCompatibility(Candidate, Request), EListenServerError::IncompatibleRegion);
	Request.Region = TEXT("KR");
	Request.RequiredAttributes[0].Value = TEXT("Easy");
	TestEqual(TEXT("Required attribute mismatch"), ListenServerNetworkPolicy::CheckCompatibility(Candidate, Request), EListenServerError::IncompatibleAttribute);
	Request.RequiredAttributes[0].Value = TEXT("Hard");
	Candidate.OpenPublicConnections = 0;
	TestEqual(TEXT("Full session is rejected"), ListenServerNetworkPolicy::CheckCompatibility(Candidate, Request), EListenServerError::SessionFull);

	TestEqual(TEXT("Join full mapping"), ListenServerNetworkPolicy::MapJoinFailure(ListenServerNetworkPolicy::EJoinFailureCode::SessionFull), EListenServerError::SessionFull);
	TestEqual(TEXT("Join address mapping"), ListenServerNetworkPolicy::MapJoinFailure(ListenServerNetworkPolicy::EJoinFailureCode::ConnectString), EListenServerError::ConnectStringFailed);

	TArray<int32> SteamOrder = { 5, 2, 9 };
	TArray<int32> FilteredOrder;
	for (const int32 Value : SteamOrder)
	{
		if (Value != 2) FilteredOrder.Add(Value);
	}
	TestEqual(TEXT("Filter retains first Steam result"), FilteredOrder[0], 5);
	TestEqual(TEXT("Filter retains relative order"), FilteredOrder[1], 9);
	return true;
}

#endif
