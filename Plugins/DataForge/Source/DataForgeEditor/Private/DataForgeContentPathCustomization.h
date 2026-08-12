#pragma once

#include "IPropertyTypeCustomization.h"

class IPropertyHandle;
class SComboButton;

class FDataForgeOutputCustomization final : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& Utils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& Utils) override;
private:
	TSharedRef<SWidget> MakePathPicker();
	void OnPathSelected(const FString& Folder);
	TSharedPtr<IPropertyHandle> PathHandle;
	TSharedPtr<SComboButton> PathButton;
};

class FDataForgeAssetRuleCustomization final : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& Utils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& Utils) override;
private:
	TSharedRef<SWidget> MakePathPicker();
	void OnPathSelected(const FString& Folder);
	TSharedPtr<IPropertyHandle> PathHandle;
	TSharedPtr<SComboButton> PathButton;
};
