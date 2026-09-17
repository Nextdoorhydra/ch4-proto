#if WITH_DEV_AUTOMATION_TESTS

#include "ListenServerNetworkDiagnostics.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FListenServerNetworkConnectionQualityTest,
	"ListenServerNetwork.Diagnostics.ConnectionQuality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FListenServerNetworkConnectionQualityTest::RunTest(const FString& Parameters)
{
	using ListenServerNetworkDiagnostics::EvaluateQuality;

	TestEqual(TEXT("Unavailable ping is unknown"), EvaluateQuality(INDEX_NONE, 0.0f, 0.0f), EListenServerConnectionQuality::Unknown);
	TestEqual(TEXT("Low latency and loss are good"), EvaluateQuality(50, 0.5f, 1.0f), EListenServerConnectionQuality::Good);
	TestEqual(TEXT("100 ms is fair"), EvaluateQuality(100, 0.0f, 0.0f), EListenServerConnectionQuality::Fair);
	TestEqual(TEXT("Two percent loss is fair"), EvaluateQuality(50, 2.0f, 0.0f), EListenServerConnectionQuality::Fair);
	TestEqual(TEXT("200 ms is poor"), EvaluateQuality(200, 0.0f, 0.0f), EListenServerConnectionQuality::Poor);
	TestEqual(TEXT("Five percent outgoing loss is poor"), EvaluateQuality(50, 0.0f, 5.0f), EListenServerConnectionQuality::Poor);
	return true;
}

#endif
