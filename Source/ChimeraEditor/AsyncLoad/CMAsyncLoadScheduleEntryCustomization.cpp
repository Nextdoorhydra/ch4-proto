#include "AsyncLoad/CMAsyncLoadScheduleEntryCustomization.h"

#include "AsyncLoad/CMStageLoadSchedule.h"
#include "AsyncLoadScheduleEntry.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CMAsyncLoadScheduleEntryCustomization"

// PropertyEditor가 customization 인스턴스를 생성할 때 사용
TSharedRef<IPropertyTypeCustomization> FCMAsyncLoadScheduleEntryCustomization::MakeInstance()
{
	return MakeShared<FCMAsyncLoadScheduleEntryCustomization>();
}

// 접힌 catalog 항목의 제목에 PrimaryAssetId 표시
void FCMAsyncLoadScheduleEntryCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& Utils)
{
	EntryHandle = StructPropertyHandle;
	HeaderRow.NameContent()[StructPropertyHandle->CreatePropertyNameWidget()]
	.ValueContent().MinDesiredWidth(320.0f)
	[
		SNew(STextBlock).Text(this, &FCMAsyncLoadScheduleEntryCustomization::GetAssetIdText)
	];
}

// Chimera Schedule에서는 Scope·Timing 대신 Load Group 드롭다운만 노출
void FCMAsyncLoadScheduleEntryCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder,
	IPropertyTypeCustomizationUtils& Utils)
{
	PropertyUtilities = Utils.GetPropertyUtilities();
	GroupIdHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAsyncLoadScheduleEntry, GroupId));

	if (!ResolveChimeraSchedules(StructPropertyHandle))
	{
		uint32 ChildCount = 0;
		StructPropertyHandle->GetNumChildren(ChildCount);
		for (uint32 Index = 0; Index < ChildCount; ++Index)
		{
			if (TSharedPtr<IPropertyHandle> Child = StructPropertyHandle->GetChildHandle(Index))
			{
				StructBuilder.AddProperty(Child.ToSharedRef());
			}
		}
		return;
	}

	StructBuilder.AddCustomRow(LOCTEXT("AssetIdSearch", "Asset Id"))
	.NameContent()[SNew(STextBlock).Text(LOCTEXT("AssetIdLabel", "Asset Id"))]
	.ValueContent().MinDesiredWidth(320.0f)
	[
		SNew(STextBlock).Text(this, &FCMAsyncLoadScheduleEntryCustomization::GetAssetIdText)
	];

	GroupOptions.Add(MakeShared<FName>(NAME_None));
	TSet<FName> UniqueGroupIds;
	for (const TWeakObjectPtr<UCMStageLoadSchedule>& Schedule : Schedules)
	{
		if (!Schedule.IsValid())
		{
			continue;
		}
		for (const FCMStageLoadGroupDefinition& Group : Schedule->LoadGroups)
		{
			if (!Group.LoadGroupId.IsNone())
			{
				UniqueGroupIds.Add(Group.LoadGroupId);
			}
		}
	}

	TArray<FName> SortedGroupIds = UniqueGroupIds.Array();
	SortedGroupIds.Sort(FNameLexicalLess());
	for (const FName GroupId : SortedGroupIds)
	{
		GroupOptions.Add(MakeShared<FName>(GroupId));
	}

	StructBuilder.AddCustomRow(LOCTEXT("LoadGroupSearch", "Load Group"))
	.NameContent()[SNew(STextBlock).Text(LOCTEXT("LoadGroupLabel", "Load Group"))]
	.ValueContent().MinDesiredWidth(320.0f)
	[
		SNew(SComboBox<TSharedPtr<FName>>)
		.OptionsSource(&GroupOptions)
		.OnGenerateWidget_Lambda([](const TSharedPtr<FName>& Item)
		{
			return SNew(STextBlock).Text(
				!Item.IsValid() || Item->IsNone()
					? LOCTEXT("UnassignedGroup", "Unassigned")
					: FText::FromName(*Item));
		})
		.OnSelectionChanged(this, &FCMAsyncLoadScheduleEntryCustomization::HandleGroupSelected)
		[
			SNew(STextBlock).Text(this, &FCMAsyncLoadScheduleEntryCustomization::GetSelectedGroupText)
		]
	];
}

// catalog Entry에 저장된 PrimaryAssetId를 사람이 읽을 수 있는 문자열로 표시
FText FCMAsyncLoadScheduleEntryCustomization::GetAssetIdText() const
{
	if (!EntryHandle.IsValid())
	{
		return LOCTEXT("UnknownAssetId", "Unknown");
	}

	TArray<void*> RawData;
	EntryHandle->AccessRawData(RawData);
	if (RawData.Num() != 1 || !RawData[0])
	{
		return LOCTEXT("MultipleAssetIds", "Multiple Values");
	}

	const FAsyncLoadScheduleEntry* Entry =
		static_cast<const FAsyncLoadScheduleEntry*>(RawData[0]);
	return Entry->AssetId.IsValid()
		? FText::FromString(Entry->AssetId.ToString())
		: LOCTEXT("InvalidAssetId", "Unregistered Asset");
}

// 선택한 LoadGroupId 저장 후 Scope와 Timing을 즉시 다시 계산
void FCMAsyncLoadScheduleEntryCustomization::HandleGroupSelected(
	TSharedPtr<FName> SelectedGroup,
	ESelectInfo::Type SelectInfo)
{
	if (!SelectedGroup.IsValid() || !GroupIdHandle.IsValid())
	{
		return;
	}

	GroupIdHandle->SetValue(*SelectedGroup);
	for (const TWeakObjectPtr<UCMStageLoadSchedule>& Schedule : Schedules)
	{
		if (Schedule.IsValid())
		{
			Schedule->RebuildLoadGroupScopes();
		}
	}
	if (PropertyUtilities.IsValid())
	{
		PropertyUtilities->ForceRefresh();
	}
}

// 현재 catalog 항목에 저장된 LoadGroupId 표시
FText FCMAsyncLoadScheduleEntryCustomization::GetSelectedGroupText() const
{
	FName GroupId;
	if (!GroupIdHandle.IsValid() || GroupIdHandle->GetValue(GroupId) != FPropertyAccess::Success || GroupId.IsNone())
	{
		return LOCTEXT("UnassignedGroup", "Unassigned");
	}
	return FText::FromName(GroupId);
}

// Details의 outer object가 Chimera Stage Schedule인지 확인하고 옵션 원본 수집
bool FCMAsyncLoadScheduleEntryCustomization::ResolveChimeraSchedules(
	TSharedRef<IPropertyHandle> StructPropertyHandle)
{
	TArray<UObject*> OuterObjects;
	StructPropertyHandle->GetOuterObjects(OuterObjects);
	for (UObject* OuterObject : OuterObjects)
	{
		if (UCMStageLoadSchedule* Schedule = Cast<UCMStageLoadSchedule>(OuterObject))
		{
			Schedules.Add(Schedule);
		}
	}
	return !Schedules.IsEmpty();
}

#undef LOCTEXT_NAMESPACE
