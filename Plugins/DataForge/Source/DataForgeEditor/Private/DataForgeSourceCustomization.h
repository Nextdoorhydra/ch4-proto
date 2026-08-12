#pragma once

#include "IPropertyTypeCustomization.h"
#include "DataForgePipeline.h"

class IPropertyHandle;
class IPropertyUtilities;
template <typename OptionType> class SComboBox;

class FDataForgeSourceCustomization final : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& Utils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& Utils) override;

private:
	void OnAdapterSelected(TSharedPtr<FDataForgeSourceDescriptor> Item, ESelectInfo::Type);
	FText GetSelectedAdapterText() const;
	FText GetAdapterDescription() const;

	TSharedPtr<IPropertyHandle> AdapterHandle;
	TSharedPtr<IPropertyUtilities> PropertyUtilities;
	TArray<TSharedPtr<FDataForgeSourceDescriptor>> AdapterOptions;
};

class FDataForgeSourceInputCustomization final : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& Utils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& Utils) override;

private:
	void OnAdapterSelected(TSharedPtr<FDataForgeSourceDescriptor> Item, ESelectInfo::Type);
	FText GetSelectedAdapterText() const;
	TSharedPtr<IPropertyHandle> AdapterHandle;
	TSharedPtr<IPropertyUtilities> PropertyUtilities;
	TArray<TSharedPtr<FDataForgeSourceDescriptor>> AdapterOptions;
};
