#pragma once

#include "IPropertyTypeCustomization.h"

class IPropertyHandle;
class IPropertyUtilities;

// Chimera Stage Schedule의 catalog 항목을 LoadGroupId 선택 중심으로 표시
class FCMAsyncLoadScheduleEntryCustomization final : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(
		TSharedRef<IPropertyHandle> StructPropertyHandle,
		FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& Utils) override;

	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> StructPropertyHandle,
		IDetailChildrenBuilder& StructBuilder,
		IPropertyTypeCustomizationUtils& Utils) override;

private:
	void HandleGroupSelected(TSharedPtr<FName> SelectedGroup, ESelectInfo::Type SelectInfo);
	FText GetAssetIdText() const;
	FText GetSelectedGroupText() const;
	bool ResolveChimeraSchedules(TSharedRef<IPropertyHandle> StructPropertyHandle);

	TSharedPtr<IPropertyHandle> EntryHandle;
	TSharedPtr<IPropertyHandle> GroupIdHandle;
	TSharedPtr<IPropertyUtilities> PropertyUtilities;
	TArray<TWeakObjectPtr<class UCMStageLoadSchedule>> Schedules;
	TArray<TSharedPtr<FName>> GroupOptions;
};
