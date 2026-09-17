#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AsyncLoadTestSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAsyncLoadBootEarlyAccessTest,
	"NetKarma.Async.BootEarlyAccessCheck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// 부팅 맵을 실행해 preload 전에 발생하는 GetCachedAsset() 접근을 검사한다.
// KnownTimingNoisePatterns는 조기 접근 오류를 숨길 수 있어 적용하지 않는다.
// 프로젝트 부팅 경로에서 필수 PDA를 preload 전에 조회하지 않는지 검증
bool FAsyncLoadBootEarlyAccessTest::RunTest(const FString& Parameters)
{
	const UAsyncLoadTestSettings* Settings = GetDefault<UAsyncLoadTestSettings>();
	if (!Settings || Settings->TestMapPath.IsNull())
	{
		AddError(TEXT("[BootEarlyAccessCheck] UAsyncLoadTestSettings::TestMapPath가 설정돼있지 않습니다 — Project Settings > Plugins > Async Load Test Settings에서 지정하세요."));
		return true;
	}

	for (const FString& Pattern : Settings->CommonNoisePatterns)
	{
		AddExpectedErrorPlain(Pattern, EAutomationExpectedErrorFlags::Contains, -1);
	}

	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Settings->TestMapPath.GetLongPackageName()));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
