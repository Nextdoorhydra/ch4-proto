#include "DataForgeSourceCustomization.h"

#include "DataForgeTypes.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DataForgeSourceCustomization"

TSharedRef<IPropertyTypeCustomization> FDataForgeSourceCustomization::MakeInstance()
{
	return MakeShared<FDataForgeSourceCustomization>();
}

void FDataForgeSourceCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils&)
{
	HeaderRow.NameContent()[StructPropertyHandle->CreatePropertyNameWidget()]
		.ValueContent()[StructPropertyHandle->CreatePropertyValueWidget()];
}

void FDataForgeSourceCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& Utils)
{
	PropertyUtilities = Utils.GetPropertyUtilities();
	AdapterHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDataForgeSourceConfig, AdapterId));
	for (const FDataForgeSourceDescriptor& Descriptor : FDataForgeSourceAdapterRegistry::Get().DescribeAll())
	{
		AdapterOptions.Add(MakeShared<FDataForgeSourceDescriptor>(Descriptor));
	}

	StructBuilder.AddCustomRow(LOCTEXT("AdapterSearch", "Source Adapter"))
	.NameContent()[AdapterHandle->CreatePropertyNameWidget()]
	.ValueContent().MinDesiredWidth(320.0f)
	[
		SNew(SComboBox<TSharedPtr<FDataForgeSourceDescriptor>>)
		.OptionsSource(&AdapterOptions)
		.OnGenerateWidget_Lambda([](TSharedPtr<FDataForgeSourceDescriptor> Item)
		{
			return SNew(STextBlock).Text(Item.IsValid() ? Item->DisplayName : FText::GetEmpty());
		})
		.OnSelectionChanged(this, &FDataForgeSourceCustomization::OnAdapterSelected)
		[
			SNew(STextBlock).Text(this, &FDataForgeSourceCustomization::GetSelectedAdapterText)
		]
	];
	StructBuilder.AddCustomRow(LOCTEXT("AdapterDescriptionSearch", "Adapter Description"))
	.WholeRowContent()[SNew(STextBlock).Text(this, &FDataForgeSourceCustomization::GetAdapterDescription).AutoWrapText(true)];

	uint32 ChildCount = 0;
	FName CurrentAdapter;
	AdapterHandle->GetValue(CurrentAdapter);
	StructPropertyHandle->GetNumChildren(ChildCount);
	for (uint32 Index = 0; Index < ChildCount; ++Index)
	{
		TSharedPtr<IPropertyHandle> Child = StructPropertyHandle->GetChildHandle(Index);
		if (Child.IsValid() && Child->GetProperty()->GetFName() != GET_MEMBER_NAME_CHECKED(FDataForgeSourceConfig, AdapterId))
		{
			const FName ChildName = Child->GetProperty()->GetFName();
			if (CurrentAdapter == TEXT("MultiSource") && (ChildName == GET_MEMBER_NAME_CHECKED(FDataForgeSourceConfig, File) || ChildName == GET_MEMBER_NAME_CHECKED(FDataForgeSourceConfig, SourceAsset))) continue;
			if (CurrentAdapter == TEXT("GoogleSheetCache") && ChildName == GET_MEMBER_NAME_CHECKED(FDataForgeSourceConfig, File)) continue;
			if (CurrentAdapter != TEXT("GoogleSheetCache") && ChildName == GET_MEMBER_NAME_CHECKED(FDataForgeSourceConfig, SourceAsset)) continue;
			if (CurrentAdapter != TEXT("MultiSource") && ChildName == GET_MEMBER_NAME_CHECKED(FDataForgeSourceConfig, Inputs)) continue;
			StructBuilder.AddProperty(Child.ToSharedRef());
		}
	}
}

void FDataForgeSourceCustomization::OnAdapterSelected(TSharedPtr<FDataForgeSourceDescriptor> Item, ESelectInfo::Type)
{
	if (Item.IsValid() && AdapterHandle.IsValid())
	{
		AdapterHandle->SetValue(Item->AdapterId);
		if (PropertyUtilities.IsValid()) PropertyUtilities->ForceRefresh();
	}
}

TSharedRef<IPropertyTypeCustomization> FDataForgeSourceInputCustomization::MakeInstance()
{
	return MakeShared<FDataForgeSourceInputCustomization>();
}

void FDataForgeSourceInputCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils&)
{
	HeaderRow.NameContent()[StructPropertyHandle->CreatePropertyNameWidget()]
		.ValueContent()[StructPropertyHandle->CreatePropertyValueWidget()];
}

void FDataForgeSourceInputCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& Utils)
{
	PropertyUtilities = Utils.GetPropertyUtilities();
	AdapterHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDataForgeSourceInput, AdapterId));
	for (const FDataForgeSourceDescriptor& Descriptor : FDataForgeSourceAdapterRegistry::Get().DescribeAll())
	{
		if (Descriptor.AdapterId != TEXT("MultiSource")) AdapterOptions.Add(MakeShared<FDataForgeSourceDescriptor>(Descriptor));
	}
	StructBuilder.AddCustomRow(LOCTEXT("InputAdapterSearch", "Input Adapter"))
	.NameContent()[AdapterHandle->CreatePropertyNameWidget()]
	.ValueContent().MinDesiredWidth(320.0f)
	[
		SNew(SComboBox<TSharedPtr<FDataForgeSourceDescriptor>>)
		.OptionsSource(&AdapterOptions)
		.OnGenerateWidget_Lambda([](TSharedPtr<FDataForgeSourceDescriptor> Item)
		{
			return SNew(STextBlock).Text(Item.IsValid() ? Item->DisplayName : FText::GetEmpty());
		})
		.OnSelectionChanged(this, &FDataForgeSourceInputCustomization::OnAdapterSelected)
		[SNew(STextBlock).Text(this, &FDataForgeSourceInputCustomization::GetSelectedAdapterText)]
	];
	FName CurrentAdapter;
	AdapterHandle->GetValue(CurrentAdapter);
	uint32 ChildCount = 0;
	StructPropertyHandle->GetNumChildren(ChildCount);
	for (uint32 Index = 0; Index < ChildCount; ++Index)
	{
		TSharedPtr<IPropertyHandle> Child = StructPropertyHandle->GetChildHandle(Index);
		if (!Child.IsValid() || Child->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FDataForgeSourceInput, AdapterId)) continue;
		const FName ChildName = Child->GetProperty()->GetFName();
		if (CurrentAdapter == TEXT("GoogleSheetCache") && ChildName == GET_MEMBER_NAME_CHECKED(FDataForgeSourceInput, File)) continue;
		if (CurrentAdapter != TEXT("GoogleSheetCache") && ChildName == GET_MEMBER_NAME_CHECKED(FDataForgeSourceInput, SourceAsset)) continue;
		StructBuilder.AddProperty(Child.ToSharedRef());
	}
}

void FDataForgeSourceInputCustomization::OnAdapterSelected(TSharedPtr<FDataForgeSourceDescriptor> Item, ESelectInfo::Type)
{
	if (Item.IsValid() && AdapterHandle.IsValid())
	{
		AdapterHandle->SetValue(Item->AdapterId);
		if (PropertyUtilities.IsValid()) PropertyUtilities->ForceRefresh();
	}
}

FText FDataForgeSourceInputCustomization::GetSelectedAdapterText() const
{
	FName AdapterId;
	if (AdapterHandle.IsValid()) AdapterHandle->GetValue(AdapterId);
	for (const TSharedPtr<FDataForgeSourceDescriptor>& Option : AdapterOptions)
	{
		if (Option.IsValid() && Option->AdapterId == AdapterId) return Option->DisplayName;
	}
	return FText::FromName(AdapterId);
}

FText FDataForgeSourceCustomization::GetSelectedAdapterText() const
{
	FName AdapterId;
	if (AdapterHandle.IsValid()) AdapterHandle->GetValue(AdapterId);
	for (const TSharedPtr<FDataForgeSourceDescriptor>& Option : AdapterOptions)
	{
		if (Option.IsValid() && Option->AdapterId == AdapterId) return Option->DisplayName;
	}
	return FText::FromName(AdapterId);
}

FText FDataForgeSourceCustomization::GetAdapterDescription() const
{
	FName AdapterId;
	if (AdapterHandle.IsValid()) AdapterHandle->GetValue(AdapterId);
	for (const TSharedPtr<FDataForgeSourceDescriptor>& Option : AdapterOptions)
	{
		if (Option.IsValid() && Option->AdapterId == AdapterId) return FText::FromString(Option->Description);
	}
	return LOCTEXT("Unavailable", "The selected adapter is not currently registered.");
}

#undef LOCTEXT_NAMESPACE
