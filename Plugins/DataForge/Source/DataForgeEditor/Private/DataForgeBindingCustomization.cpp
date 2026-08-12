#include "DataForgeBindingCustomization.h"

#include "DataForgeRuleSet.h"
#include "DataForgeEditorService.h"
#include "DataForgeTypes.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "UObject/UnrealType.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DataForgeBindingCustomization"

TSharedRef<IPropertyTypeCustomization> FDataForgeBindingCustomization::MakeInstance()
{
	return MakeShared<FDataForgeBindingCustomization>();
}

void FDataForgeBindingCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructHandle = StructPropertyHandle;
	PropertyUtilities = StructCustomizationUtils.GetPropertyUtilities();
	HeaderRow
	.NameContent()[StructPropertyHandle->CreatePropertyNameWidget()]
	.ValueContent()[StructPropertyHandle->CreatePropertyValueWidget()];
}

void FDataForgeBindingCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TargetHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDataForgeBindingRule, Target));
	SourceHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDataForgeBindingRule, Source));
	TargetOutputHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDataForgeBindingRule, TargetOutput));
	TargetPropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDataForgeBindingRule, TargetProperty));

	uint32 ChildCount = 0;
	StructPropertyHandle->GetNumChildren(ChildCount);
	for (uint32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
	{
		TSharedPtr<IPropertyHandle> Child = StructPropertyHandle->GetChildHandle(ChildIndex);
		if (!Child.IsValid())
		{
			continue;
		}
		const FName ChildName = Child->GetProperty()->GetFName();
		if (ChildName == GET_MEMBER_NAME_CHECKED(FDataForgeBindingRule, SourceOutput)
			|| ChildName == GET_MEMBER_NAME_CHECKED(FDataForgeBindingRule, TargetOutput))
		{
			const TSharedPtr<IPropertyHandle> OutputHandle = Child;
			StructBuilder.AddCustomRow(LOCTEXT("OutputSearch", "Generated Output Pick"))
			.Visibility(ChildName == GET_MEMBER_NAME_CHECKED(FDataForgeBindingRule, SourceOutput)
				? TAttribute<EVisibility>::CreateSP(this, &FDataForgeBindingCustomization::GetSourceOutputVisibility)
				: TAttribute<EVisibility>::CreateSP(this, &FDataForgeBindingCustomization::GetTargetOutputVisibility))
			.NameContent()
			[
				Child->CreatePropertyNameWidget()
			]
			.ValueContent()
			.MinDesiredWidth(320.0f)
			[
				SNew(SComboButton)
				.ButtonContent()
				[
					SNew(STextBlock).Text(this, &FDataForgeBindingCustomization::GetSelectedOutputText, OutputHandle)
				]
				.OnGetMenuContent(this, &FDataForgeBindingCustomization::BuildOutputMenu, OutputHandle)
			];
			continue;
		}
		if (ChildName != GET_MEMBER_NAME_CHECKED(FDataForgeBindingRule, TargetProperty))
		{
			StructBuilder.AddProperty(Child.ToSharedRef());
			continue;
		}

		StructBuilder.AddCustomRow(LOCTEXT("TargetPropertySearch", "Target Property Pick Property"))
		.NameContent()
		[
			Child->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(320.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				Child->CreatePropertyValueWidget()
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
			[
				SNew(SComboButton)
				.ButtonContent()
				[
					SNew(STextBlock).Text(LOCTEXT("Pick", "Pick"))
				]
				.OnGetMenuContent(this, &FDataForgeBindingCustomization::BuildPropertyMenu)
			]
		];
		StructBuilder.AddCustomRow(LOCTEXT("ConversionSearch", "Conversion Compatibility Type Suggestion"))
		.NameContent()
		[
			SNew(STextBlock).Text(LOCTEXT("Conversion", "Conversion"))
		]
		.ValueContent()
		.MinDesiredWidth(320.0f)
		[
			SNew(STextBlock).Text(this, &FDataForgeBindingCustomization::GetConversionSuggestionText).AutoWrapText(true)
		];
	}
}

TSharedRef<SWidget> FDataForgeBindingCustomization::BuildOutputMenu(TSharedPtr<IPropertyHandle> OutputHandle) const
{
	FMenuBuilder MenuBuilder(true, nullptr);
	const UDataForgeRuleSet* RuleSet = FindRuleSet();
	if (!OutputHandle.IsValid() || !RuleSet || RuleSet->GeneratedOutputs.IsEmpty())
	{
		MenuBuilder.AddMenuEntry(LOCTEXT("NoGeneratedOutputs", "No Generated Outputs configured"), FText::GetEmpty(), FSlateIcon(), FUIAction());
		return MenuBuilder.MakeWidget();
	}

	for (const FDataForgeGeneratedAssetOutputRule& Output : RuleSet->GeneratedOutputs)
	{
		if (Output.OutputName.IsNone())
		{
			continue;
		}
		const FName OutputName = Output.OutputName;
		MenuBuilder.AddMenuEntry(
			FText::FromName(OutputName),
			FText::Format(LOCTEXT("GeneratedOutputTooltip", "Use Generated Output '{0}'."), FText::FromName(OutputName)),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([OutputHandle, Utilities = PropertyUtilities, OutputName]()
			{
				OutputHandle->SetValue(OutputName);
				if (Utilities.IsValid())
				{
					Utilities->ForceRefresh();
				}
			})));
	}
	return MenuBuilder.MakeWidget();
}

FText FDataForgeBindingCustomization::GetSelectedOutputText(TSharedPtr<IPropertyHandle> OutputHandle) const
{
	FName OutputName;
	if (!OutputHandle.IsValid() || OutputHandle->GetValue(OutputName) != FPropertyAccess::Success || OutputName.IsNone())
	{
		return LOCTEXT("SelectGeneratedOutput", "Select Generated Output");
	}
	return FText::FromName(OutputName);
}

EVisibility FDataForgeBindingCustomization::GetSourceOutputVisibility() const
{
	uint8 Value = static_cast<uint8>(EDataForgeBindingSource::SourceValue);
	return SourceHandle.IsValid() && SourceHandle->GetValue(Value) == FPropertyAccess::Success
		&& static_cast<EDataForgeBindingSource>(Value) == EDataForgeBindingSource::GeneratedOutput
		? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FDataForgeBindingCustomization::GetTargetOutputVisibility() const
{
	uint8 Value = static_cast<uint8>(EDataForgeBindingTarget::DataTableRow);
	return TargetHandle.IsValid() && TargetHandle->GetValue(Value) == FPropertyAccess::Success
		&& static_cast<EDataForgeBindingTarget>(Value) == EDataForgeBindingTarget::GeneratedOutput
		? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedRef<SWidget> FDataForgeBindingCustomization::BuildPropertyMenu() const
{
	FMenuBuilder MenuBuilder(true, nullptr);
	if (!StructHandle.IsValid() || !TargetHandle.IsValid() || !TargetPropertyHandle.IsValid())
	{
		MenuBuilder.AddMenuEntry(LOCTEXT("Unavailable", "Property picker unavailable"), FText::GetEmpty(), FSlateIcon(), FUIAction());
		return MenuBuilder.MakeWidget();
	}

	UDataForgeRuleSet* RuleSet = FindRuleSet();
	const FDataForgeBindingRule* CurrentBinding = GetBinding();

	uint8 TargetValue = static_cast<uint8>(EDataForgeBindingTarget::DataTableRow);
	TargetHandle->GetValue(TargetValue);
	UStruct* TargetStruct = nullptr;
	if (RuleSet && static_cast<EDataForgeBindingTarget>(TargetValue) == EDataForgeBindingTarget::DataTableRow)
	{
		TargetStruct = RuleSet->Output.RowStruct;
	}
	else if (RuleSet)
	{
		FName TargetOutput;
		TargetOutputHandle->GetValue(TargetOutput);
		if (const FDataForgeGeneratedAssetOutputRule* Output = RuleSet->GeneratedOutputs.FindByPredicate([TargetOutput](const FDataForgeGeneratedAssetOutputRule& Candidate)
		{
			return Candidate.OutputName == TargetOutput;
		}))
		{
			TargetStruct = Output->AssetClass.Get();
		}
	}

	TArray<TPair<FString, FString>> Properties;
	CollectWritableProperties(TargetStruct, FString(), 0, Properties);
	Properties.Sort([](const TPair<FString, FString>& A, const TPair<FString, FString>& B)
	{
		return A.Key < B.Key;
	});

	if (Properties.IsEmpty())
	{
		MenuBuilder.AddMenuEntry(LOCTEXT("NoProperties", "No writable target properties"), FText::GetEmpty(), FSlateIcon(), FUIAction());
		return MenuBuilder.MakeWidget();
	}

	for (const TPair<FString, FString>& Property : Properties)
	{
		const FString Path = Property.Key;
		FText ToolTip = FText::FromString(Property.Value);
		if (RuleSet && CurrentBinding)
		{
			FDataForgeBindingRule Candidate = *CurrentBinding;
			Candidate.TargetProperty = Path;
			ToolTip = FText::FromString(FDataForgeEditorService::GetCachedBindingSuggestion(*RuleSet, Candidate).ToDisplayString());
		}
		MenuBuilder.AddMenuEntry(
			FText::FromString(Property.Key),
			ToolTip,
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([Handle = TargetPropertyHandle, Utilities = PropertyUtilities, Path]()
			{
				Handle->SetValue(Path);
				if (Utilities.IsValid())
				{
					Utilities->ForceRefresh();
				}
			})));
	}
	return MenuBuilder.MakeWidget();
}

UDataForgeRuleSet* FDataForgeBindingCustomization::FindRuleSet() const
{
	if (!StructHandle.IsValid())
	{
		return nullptr;
	}
	TArray<UObject*> OuterObjects;
	StructHandle->GetOuterObjects(OuterObjects);
	for (UObject* Object : OuterObjects)
	{
		if (UDataForgeRuleSet* RuleSet = Cast<UDataForgeRuleSet>(Object))
		{
			return RuleSet;
		}
	}
	return nullptr;
}

const FDataForgeBindingRule* FDataForgeBindingCustomization::GetBinding() const
{
	if (!StructHandle.IsValid())
	{
		return nullptr;
	}
	TArray<void*> RawData;
	StructHandle->AccessRawData(RawData);
	return RawData.Num() == 1 ? static_cast<const FDataForgeBindingRule*>(RawData[0]) : nullptr;
}

FText FDataForgeBindingCustomization::GetConversionSuggestionText() const
{
	const UDataForgeRuleSet* RuleSet = FindRuleSet();
	const FDataForgeBindingRule* Binding = GetBinding();
	if (!RuleSet || !Binding)
	{
		return LOCTEXT("ConversionUnavailable", "Conversion analysis unavailable.");
	}
	return FText::FromString(FDataForgeEditorService::GetCachedBindingSuggestion(*RuleSet, *Binding).ToDisplayString());
}

void FDataForgeBindingCustomization::CollectWritableProperties(
	UStruct* Struct,
	const FString& Prefix,
	int32 Depth,
	TArray<TPair<FString, FString>>& OutProperties) const
{
	if (!Struct || Depth > 4)
	{
		return;
	}
	for (TFieldIterator<FProperty> It(Struct, EFieldIterationFlags::IncludeSuper); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
		{
			continue;
		}

		const FString Path = Prefix.IsEmpty() ? Property->GetName() : Prefix + TEXT(".") + Property->GetName();
		OutProperties.Emplace(Path, Property->GetCPPType());
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			CollectWritableProperties(StructProperty->Struct, Path, Depth + 1, OutProperties);
		}
	}
}

#undef LOCTEXT_NAMESPACE
