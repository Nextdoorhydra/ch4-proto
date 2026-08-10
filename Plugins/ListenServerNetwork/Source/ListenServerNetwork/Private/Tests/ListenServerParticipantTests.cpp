#if WITH_DEV_AUTOMATION_TESTS

#include "ListenServerNetworkTypes.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FListenServerParticipantChangeDetectionTest,
	"ListenServerNetwork.Participants.ChangeDetection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FListenServerParticipantChangeDetectionTest::RunTest(const FString& Parameters)
{
	FListenServerParticipant Participant;
	Participant.PlayerId = 1;
	Participant.DisplayName = TEXT("Host");
	Participant.PlatformUserId = TEXT("Steam.1001");
	Participant.PingMilliseconds = 20;
	Participant.bIsHost = true;
	Participant.bIsLocalPlayer = true;

	const TArray<FListenServerParticipant> Snapshot = { Participant };
	TArray<FListenServerParticipant> ChangedSnapshot = Snapshot;
	TestTrue(TEXT("Identical participant snapshots compare equal"), Snapshot == ChangedSnapshot);

	ChangedSnapshot[0].DisplayName = TEXT("Renamed Host");
	TestFalse(TEXT("Display-name changes are detected"), Snapshot == ChangedSnapshot);

	ChangedSnapshot = Snapshot;
	ChangedSnapshot[0].PingMilliseconds = 35;
	TestFalse(TEXT("Ping changes are detected"), Snapshot == ChangedSnapshot);

	ChangedSnapshot = Snapshot;
	ChangedSnapshot.AddDefaulted();
	TestFalse(TEXT("Join and leave changes are detected"), Snapshot == ChangedSnapshot);

	ChangedSnapshot = Snapshot;
	ChangedSnapshot[0].bIsHost = false;
	TestFalse(TEXT("Host changes are detected"), Snapshot == ChangedSnapshot);

	return true;
}

#endif
