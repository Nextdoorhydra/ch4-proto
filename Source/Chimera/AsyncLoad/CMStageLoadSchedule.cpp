#include "AsyncLoad/CMStageLoadSchedule.h"
#include "AsyncLoad/CMStageLoadLog.h"
#include "AsyncLoad/CMStageLoadTags.h"

#if WITH_EDITOR
#include "Logging/LogMacros.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

#if WITH_EDITOR
// PDA catalog 갱신과 그룹 동기화를 한 번의 에디터 작업으로 실행
void UCMStageLoadSchedule::RefreshAndRebuildCatalog()
{
	RefreshPDACatalog();
	RebuildLoadGroupScopes();
}

// LoadGroupId를 안정적으로 정렬해 Scope를 만들고 catalog 항목의 Timing까지 자동 반영
void UCMStageLoadSchedule::RebuildLoadGroupScopes()
{
	TArray<FCMStageLoadGroupDefinition*> SortedGroups;
	TSet<FName> SeenGroupIds;
	bool bHasInvalidGroup = false;

	for (FCMStageLoadGroupDefinition& Group : LoadGroups)
	{
		Group.Scope = INDEX_NONE;
		if (Group.LoadGroupId.IsNone() || SeenGroupIds.Contains(Group.LoadGroupId))
		{
			bHasInvalidGroup = true;
			continue;
		}

		SeenGroupIds.Add(Group.LoadGroupId);
		SortedGroups.Add(&Group);
	}

	SortedGroups.Sort([](const FCMStageLoadGroupDefinition& A, const FCMStageLoadGroupDefinition& B)
	{
		return A.LoadGroupId.LexicalLess(B.LoadGroupId);
	});

	TMap<FName, FCMStageLoadGroupDefinition*> GroupsById;
	for (int32 Index = 0; Index < SortedGroups.Num(); ++Index)
	{
		FCMStageLoadGroupDefinition* Group = SortedGroups[Index];
		Group->Scope = Index;
		GroupsById.Add(Group->LoadGroupId, Group);
	}

	for (FAsyncLoadScheduleCategory& Category : Categories)
	{
		for (FAsyncLoadScheduleEntry& Entry : Category.Entries)
		{
			// 기존 수동 Scope·Timing 데이터가 정확히 한 그룹과 일치하면 최초 1회 GroupId로 이관한다.
			if (const FCMStageLoadGroupDefinition* const* Group = GroupsById.Find(Entry.GroupId))
			{
				Entry.Scope = (*Group)->Scope;
				Entry.Timing = CMStageLoadTags::Stage;
			}
			else
			{
				Entry.Scope = INDEX_NONE;
				Entry.Timing = FGameplayTag();
			}
		}
	}

	if (bHasInvalidGroup)
	{
		UE_LOG(LogChimeraStageLoad, Error, TEXT("Schedule=%s has an empty or duplicate LoadGroupId; Scope was not generated."), *GetPathName());
	}

	MarkPackageDirty();
}
#endif

// LoadGroupId에 해당하는 그룹 정의 검색
const FCMStageLoadGroupDefinition* UCMStageLoadSchedule::FindLoadGroup(FName LoadGroupId) const
{
	return LoadGroups.FindByPredicate([LoadGroupId](const FCMStageLoadGroupDefinition& Group)
	{
		return Group.LoadGroupId == LoadGroupId;
	});
}

// 같은 TimingTag 그룹을 LoadOrder와 LoadGroupId 순서로 안정적으로 정렬
TArray<const FCMStageLoadGroupDefinition*> UCMStageLoadSchedule::GetOrderedAutomaticGroups() const
{
	TArray<const FCMStageLoadGroupDefinition*> Result;
	for (const FCMStageLoadGroupDefinition& Group : LoadGroups)
	{
		if (Group.LoadPolicy != ECMStageLoadPolicy::OnDemand)
		{
			Result.Add(&Group);
		}
	}

	Result.Sort([](const FCMStageLoadGroupDefinition& A, const FCMStageLoadGroupDefinition& B)
	{
		if (A.LoadPolicy != B.LoadPolicy)
		{
			return A.LoadPolicy == ECMStageLoadPolicy::BeforeStageStart;
		}
		return A.LoadOrder == B.LoadOrder
			? A.LoadGroupId.LexicalLess(B.LoadGroupId)
			: A.LoadOrder < B.LoadOrder;
	});
	return Result;
}

// Schedule 데이터에 정의된 모든 유효 Scope·Timing 조합 반환
TArray<TPair<int32, FGameplayTag>> UCMStageLoadSchedule::GetReachableScopeTimingPairs() const
{
	TArray<TPair<int32, FGameplayTag>> Result;
	for (const FCMStageLoadGroupDefinition& Group : LoadGroups)
	{
		if (!Group.LoadGroupId.IsNone() && Group.Scope != INDEX_NONE)
		{
			Result.AddUnique(TPair<int32, FGameplayTag>(Group.Scope, CMStageLoadTags::Stage));
		}
	}
	return Result;
}

// catalog 새로고침에서 Schedule 타입 제외
TSet<FName> UCMStageLoadSchedule::GetExcludedPrimaryAssetTypes() const
{
	return { TEXT("CMStageLoadSchedule") };
}

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCMStageLoadScheduleScopeAutomationTest,
	"Chimera.AsyncLoad.StageScheduleScopeAutomation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// LoadGroupId 정렬로 Scope가 결정되고 catalog Scope·Timing이 자동 동기화되는지 검증
bool FCMStageLoadScheduleScopeAutomationTest::RunTest(const FString& Parameters)
{
	UCMStageLoadSchedule* Schedule = NewObject<UCMStageLoadSchedule>();
	FCMStageLoadGroupDefinition& Lab = Schedule->LoadGroups.AddDefaulted_GetRef();
	Lab.LoadGroupId = TEXT("Stage01.Laboratory");
	Lab.LoadPolicy = ECMStageLoadPolicy::Sequential;
	FCMStageLoadGroupDefinition& Entry = Schedule->LoadGroups.AddDefaulted_GetRef();
	Entry.LoadGroupId = TEXT("Stage01.Entry");
	Entry.LoadPolicy = ECMStageLoadPolicy::BeforeStageStart;
	Entry.LoadOrder = 100;
	FCMStageLoadGroupDefinition& Branch = Schedule->LoadGroups.AddDefaulted_GetRef();
	Branch.LoadGroupId = TEXT("Stage01.OptionalBranch");
	Branch.LoadPolicy = ECMStageLoadPolicy::OnDemand;
	Branch.LoadOrder = 1;

	FAsyncLoadScheduleCategory& Category = Schedule->Categories.AddDefaulted_GetRef();
	FAsyncLoadScheduleEntry& AssetEntry = Category.Entries.AddDefaulted_GetRef();
	AssetEntry.AssetId = FPrimaryAssetId(FPrimaryAssetType(TEXT("TestData")), TEXT("LabObstacle"));
	AssetEntry.GroupId = Lab.LoadGroupId;

	Schedule->RebuildLoadGroupScopes();
	TestEqual(TEXT("Entry receives the first deterministic scope"), Entry.Scope, 0);
	TestEqual(TEXT("Laboratory receives the second deterministic scope"), Lab.Scope, 1);
	TestEqual(TEXT("Catalog scope follows selected LoadGroupId"), AssetEntry.Scope, Lab.Scope);
	TestTrue(TEXT("Catalog uses the internal stage timing"), AssetEntry.Timing == CMStageLoadTags::Stage);
	const TArray<const FCMStageLoadGroupDefinition*> AutomaticGroups = Schedule->GetOrderedAutomaticGroups();
	TestEqual(TEXT("OnDemand groups are excluded from the automatic queue"), AutomaticGroups.Num(), 2);
	TestEqual(TEXT("BeforeStageStart groups run before sequential groups"), AutomaticGroups[0]->LoadGroupId, Entry.LoadGroupId);

	Schedule->LoadGroups.Swap(0, 1);
	Schedule->RebuildLoadGroupScopes();
	TestEqual(TEXT("Array reorder does not change Entry scope"), Schedule->FindLoadGroup(TEXT("Stage01.Entry"))->Scope, 0);
	TestEqual(TEXT("Array reorder does not change Laboratory scope"), Schedule->FindLoadGroup(TEXT("Stage01.Laboratory"))->Scope, 1);
	return true;
}

#endif
