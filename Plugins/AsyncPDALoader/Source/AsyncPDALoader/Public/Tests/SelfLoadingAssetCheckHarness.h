#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/UObjectIterator.h"

// 중앙 PDA 로더 밖에서 자체 로드하는 자산을 검사한다.
class FSelfLoadingAssetCheckHarness
{
public:
	void RegisterCheck(FString DebugName, TFunction<bool()> IsConfigured, TFunction<UObject*()> GetCached, float TimeoutSeconds = 5.f)
	{
		FEntry Entry;
		Entry.DebugName = MoveTemp(DebugName);
		Entry.TimeoutSeconds = TimeoutSeconds;
		Entry.IsConfigured = MoveTemp(IsConfigured);
		Entry.GetCached = MoveTemp(GetCached);
		Entries.Add(MoveTemp(Entry));
	}

	// timeout 측정을 시작한다.
	void Start()
	{
		StartTime = FPlatformTime::Seconds();
	}

	int32 GetRegisteredCount() const { return Entries.Num(); }
	int32 GetConfiguredCount() const { return ConfiguredCount; }
	int32 GetSucceededCount() const { return SucceededCount; }
	int32 GetSkippedCount() const { return SkippedCount; }

	// 모든 항목이 성공, skip 또는 timeout 처리되면 true를 반환한다.
	bool Poll(FAutomationTestBase& Test)
	{
		const double Elapsed = FPlatformTime::Seconds() - StartTime;

		bool bAllResolved = true;
		for (FEntry& Entry : Entries)
		{
			if (Entry.bResolved)
			{
				continue;
			}

			if (!Entry.bConfigurationEvaluated)
			{
				Entry.bConfigurationEvaluated = true;
				Entry.bConfigured = Entry.IsConfigured();
				if (Entry.bConfigured)
				{
					++ConfiguredCount;
				}
				else
				{
					++SkippedCount;
					Entry.bResolved = true;
					continue;
				}
			}

			if (Entry.GetCached())
			{
				++SucceededCount;
				Entry.bResolved = true;
				continue;
			}

			if (Elapsed > Entry.TimeoutSeconds)
			{
				Test.AddError(FString::Printf(
					TEXT("[SelfLoadingAssets] %s가 설정돼있는데 타임아웃(%.1fs)까지 캐시되지 않았습니다."),
					*Entry.DebugName, Entry.TimeoutSeconds));
				Entry.bResolved = true;
			}
			else
			{
				bAllResolved = false; // 아직 기다리는 중.
			}
		}

		return bAllResolved;
	}

	// 설정된 첫 번째 TClass 서브클래스를 반환한다.
	template<typename TClass>
	static UClass* FindConfiguredSubclass(TFunctionRef<bool(const TClass*)> IsConfiguredPredicate)
	{
		for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
		{
			UClass* Class = *ClassIt;
			if (!Class->IsChildOf(TClass::StaticClass()))
			{
				continue;
			}
			if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
			{
				continue;
			}

			const TClass* CDO = Class->GetDefaultObject<TClass>();
			if (CDO && IsConfiguredPredicate(CDO))
			{
				return Class;
			}
		}
		return nullptr;
	}

private:
	struct FEntry
	{
		FString DebugName;
		float TimeoutSeconds = 5.f;
		TFunction<bool()> IsConfigured;
		TFunction<UObject*()> GetCached;
		bool bConfigurationEvaluated = false;
		bool bConfigured = false;
		bool bResolved = false;
	};

	TArray<FEntry> Entries;
	double StartTime = 0.0;
	int32 ConfiguredCount = 0;
	int32 SucceededCount = 0;
	int32 SkippedCount = 0;
};

#endif // WITH_DEV_AUTOMATION_TESTS

