#include "DataForgeContentPathCustomization.h"

#include "ContentBrowserModule.h"
#include "DataForgeTypes.h"
#include "DetailWidgetRow.h"
#include "IContentBrowserSingleton.h"
#include "IDetailChildrenBuilder.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DataForgeContentPathCustomization"

namespace
{
	void AddChildrenWithPathPicker(TSharedRef<IPropertyHandle> StructHandle, IDetailChildrenBuilder& Builder, FName PathName,
		TSharedPtr<IPropertyHandle>& OutPathHandle, TSharedPtr<SComboButton>& OutButton, const FOnGetContent& GetMenu)
	{
		uint32 Count = 0;
		StructHandle->GetNumChildren(Count);
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			TSharedPtr<IPropertyHandle> Child = StructHandle->GetChildHandle(Index);
			if (!Child.IsValid()) continue;
			if (Child->GetProperty()->GetFName() != PathName)
			{
				Builder.AddProperty(Child.ToSharedRef());
				continue;
			}
			OutPathHandle = Child;
			Builder.AddCustomRow(LOCTEXT("ContentPathSearch", "Content Browser Path"))
			.NameContent()[Child->CreatePropertyNameWidget()]
			.ValueContent().MinDesiredWidth(360.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f)[Child->CreatePropertyValueWidget()]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
				[
					SAssignNew(OutButton, SComboButton).OnGetMenuContent(GetMenu)
					.ButtonContent()[SNew(STextBlock).Text(LOCTEXT("Browse", "Browse"))]
				]
			];
		}
	}

	TSharedRef<SWidget> BuildPicker(const TSharedPtr<IPropertyHandle>& Handle, const FOnPathSelected& OnSelected)
	{
		FString Current;
		if (Handle.IsValid()) Handle->GetValue(Current);
		FPathPickerConfig Config;
		Config.DefaultPath = FPackageName::GetLongPackagePath(Current);
		if (Config.DefaultPath.IsEmpty()) Config.DefaultPath = TEXT("/Game");
		Config.OnPathSelected = OnSelected;
		Config.bAllowClassesFolder = false;
		Config.bAddDefaultPath = false;
		Config.bAllowContextMenu = false;
		return SNew(SBox).WidthOverride(360.0f).HeightOverride(480.0f)
		[
			FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().CreatePathPicker(Config)
		];
	}
}

TSharedRef<IPropertyTypeCustomization> FDataForgeOutputCustomization::MakeInstance() { return MakeShared<FDataForgeOutputCustomization>(); }
void FDataForgeOutputCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> Handle, FDetailWidgetRow& Row, IPropertyTypeCustomizationUtils&) { Row.NameContent()[Handle->CreatePropertyNameWidget()].ValueContent()[Handle->CreatePropertyValueWidget()]; }
void FDataForgeOutputCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> Handle, IDetailChildrenBuilder& Builder, IPropertyTypeCustomizationUtils&)
{
	AddChildrenWithPathPicker(Handle, Builder, GET_MEMBER_NAME_CHECKED(FDataForgeDataTableOutputRule, AssetPath), PathHandle, PathButton,
		FOnGetContent::CreateSP(this, &FDataForgeOutputCustomization::MakePathPicker));
}
TSharedRef<SWidget> FDataForgeOutputCustomization::MakePathPicker() { return BuildPicker(PathHandle, FOnPathSelected::CreateSP(this, &FDataForgeOutputCustomization::OnPathSelected)); }
void FDataForgeOutputCustomization::OnPathSelected(const FString& Folder)
{
	FString Current;
	PathHandle->GetValue(Current);
	FString Name = FPackageName::GetLongPackageAssetName(Current);
	if (Name.IsEmpty()) Name = TEXT("DT_Output");
	PathHandle->SetValue(Folder / Name);
	if (PathButton.IsValid()) PathButton->SetIsOpen(false);
}

TSharedRef<IPropertyTypeCustomization> FDataForgeAssetRuleCustomization::MakeInstance() { return MakeShared<FDataForgeAssetRuleCustomization>(); }
void FDataForgeAssetRuleCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> Handle, FDetailWidgetRow& Row, IPropertyTypeCustomizationUtils&) { Row.NameContent()[Handle->CreatePropertyNameWidget()].ValueContent()[Handle->CreatePropertyValueWidget()]; }
void FDataForgeAssetRuleCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> Handle, IDetailChildrenBuilder& Builder, IPropertyTypeCustomizationUtils&)
{
	AddChildrenWithPathPicker(Handle, Builder, GET_MEMBER_NAME_CHECKED(FDataForgeAssetRule, BaseFolder), PathHandle, PathButton,
		FOnGetContent::CreateSP(this, &FDataForgeAssetRuleCustomization::MakePathPicker));
}
TSharedRef<SWidget> FDataForgeAssetRuleCustomization::MakePathPicker() { return BuildPicker(PathHandle, FOnPathSelected::CreateSP(this, &FDataForgeAssetRuleCustomization::OnPathSelected)); }
void FDataForgeAssetRuleCustomization::OnPathSelected(const FString& Folder)
{
	PathHandle->SetValue(Folder);
	if (PathButton.IsValid()) PathButton->SetIsOpen(false);
}

#undef LOCTEXT_NAMESPACE
