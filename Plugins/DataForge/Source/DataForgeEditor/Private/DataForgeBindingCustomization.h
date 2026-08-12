#pragma once

#include "IPropertyTypeCustomization.h"

class IPropertyHandle;
class IPropertyUtilities;
class SWidget;
class UStruct;
class UDataForgeRuleSet;
struct FDataForgeBindingRule;

class FDataForgeBindingCustomization final : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(
		TSharedRef<IPropertyHandle> StructPropertyHandle,
		FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> StructPropertyHandle,
		IDetailChildrenBuilder& StructBuilder,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:
	TSharedRef<SWidget> BuildPropertyMenu() const;
	TSharedRef<SWidget> BuildOutputMenu(TSharedPtr<IPropertyHandle> OutputHandle) const;
	FText GetSelectedOutputText(TSharedPtr<IPropertyHandle> OutputHandle) const;
	EVisibility GetSourceOutputVisibility() const;
	EVisibility GetTargetOutputVisibility() const;
	UDataForgeRuleSet* FindRuleSet() const;
	const FDataForgeBindingRule* GetBinding() const;
	FText GetConversionSuggestionText() const;
	void CollectWritableProperties(UStruct* Struct, const FString& Prefix, int32 Depth, TArray<TPair<FString, FString>>& OutProperties) const;

	TSharedPtr<IPropertyHandle> StructHandle;
	TSharedPtr<IPropertyHandle> SourceHandle;
	TSharedPtr<IPropertyHandle> TargetHandle;
	TSharedPtr<IPropertyHandle> TargetOutputHandle;
	TSharedPtr<IPropertyHandle> TargetPropertyHandle;
	TSharedPtr<IPropertyUtilities> PropertyUtilities;
};
