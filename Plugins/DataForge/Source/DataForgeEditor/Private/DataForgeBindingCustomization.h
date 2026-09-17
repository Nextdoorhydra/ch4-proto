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
	TSharedRef<SWidget> BuildAssetRuleMenu(TSharedPtr<IPropertyHandle> AssetRuleHandle) const;
	FText GetSelectedOutputText(TSharedPtr<IPropertyHandle> OutputHandle) const;
	FText GetSelectedAssetRuleText(TSharedPtr<IPropertyHandle> AssetRuleHandle) const;
	EVisibility GetSourceOutputVisibility() const;
	EVisibility GetTargetOutputVisibility() const;
	EVisibility GetAssetRuleVisibility() const;
	UDataForgeRuleSet* FindRuleSet() const;
	const FDataForgeBindingRule* GetBinding() const;
	FText GetConversionSuggestionText() const;
	void CollectWritableProperties(UStruct* Struct, const FString& Prefix, int32 Depth, TArray<TPair<FString, FString>>& OutProperties) const;

	TSharedPtr<IPropertyHandle> StructHandle;
	TSharedPtr<IPropertyHandle> SourceHandle;
	TSharedPtr<IPropertyHandle> TargetHandle;
	TSharedPtr<IPropertyHandle> TargetOutputHandle;
	TSharedPtr<IPropertyHandle> TargetPropertyHandle;
	TSharedPtr<IPropertyHandle> AssetRuleIdHandle;
	TSharedPtr<IPropertyUtilities> PropertyUtilities;
};

class FDataForgeGeneratedOutputCustomization final : public IPropertyTypeCustomization
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
	TSharedRef<SWidget> BuildManagedAssetRuleMenu() const;
	FText GetSelectedAssetRuleText() const;
	UDataForgeRuleSet* FindRuleSet() const;

	TSharedPtr<IPropertyHandle> StructHandle;
	TSharedPtr<IPropertyHandle> AssetRuleIdHandle;
	TSharedPtr<IPropertyUtilities> PropertyUtilities;
};
