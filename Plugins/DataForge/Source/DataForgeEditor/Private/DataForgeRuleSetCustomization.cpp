#include "DataForgeRuleSetCustomization.h"

#include "AssetRegistry/AssetData.h"
#include "DataForgeAssetLayoutAuthoring.h"
#include "DataForgeAssetLayoutProfile.h"
#include "DataForgeEditorService.h"
#include "DataForgeRuleSet.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Misc/MessageDialog.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DataForgeRuleSetCustomization"

TSharedRef<IDetailCustomization> FDataForgeRuleSetCustomization::MakeInstance()
{
	return MakeShared<FDataForgeRuleSetCustomization>();
}

void FDataForgeRuleSetCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailLayout = &DetailBuilder;
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() != 1)
	{
		return;
	}
	RuleSet = Cast<UDataForgeRuleSet>(Objects[0].Get());
	if (!RuleSet.IsValid())
	{
		return;
	}
	if (RuleSet->GetPackage() == GetTransientPackage())
	{
		return;
	}

	DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UDataForgeRuleSet, ProfileOrigin))->MarkHiddenByCustomization();
	SelectedProfile = RuleSet->ProfileOrigin.Profile.LoadSynchronous();
	StagedParameterValues = RuleSet->ProfileOrigin.ParameterValues;
	if (SelectedProfile.IsValid())
	{
		for (const FDataForgeProfileParameter& Parameter : SelectedProfile->Parameters)
		{
			if (!StagedParameterValues.Contains(Parameter.Name))
			{
				StagedParameterValues.Add(Parameter.Name, Parameter.DefaultValue);
			}
		}
	}

	IDetailCategoryBuilder& Layout = DetailBuilder.EditCategory(TEXT("Asset Layout Profile"), LOCTEXT("AssetLayout", "Asset Layout Profile"), ECategoryPriority::Important);
	Layout.AddCustomRow(LOCTEXT("ProfileSearch", "Asset Layout Profile"))
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("Profile", "Profile"))
		.ToolTipText(LOCTEXT("ProfileTooltip", "Select a central layout convention. It is materialized into concrete Asset Rules and is not a compiler dependency."))
	]
	.ValueContent()
	.MinDesiredWidth(300.0f)
	[
		SNew(SObjectPropertyEntryBox)
		.AllowedClass(UDataForgeAssetLayoutProfile::StaticClass())
		.ObjectPath(this, &FDataForgeRuleSetCustomization::GetSelectedProfilePath)
		.OnObjectChanged(this, &FDataForgeRuleSetCustomization::SelectProfile)
	];

	Layout.AddCustomRow(LOCTEXT("ProfileStatusSearch", "Profile Status Overrides Custom"))
	.WholeRowContent()
	[
		SNew(STextBlock)
		.Text(this, &FDataForgeRuleSetCustomization::GetLayoutStatusText)
		.AutoWrapText(true)
	];

	Layout.AddCustomRow(LOCTEXT("ParametersSearch", "Profile Parameters"))
	.WholeRowContent()
	[
		SAssignNew(ParameterRows, SVerticalBox)
	];
	RebuildParameterRows();

	Layout.AddCustomRow(LOCTEXT("RuleStatusSearch", "Profile Override Custom Rule Status"))
	.WholeRowContent()
	[
		SAssignNew(RuleStatusRows, SVerticalBox)
	];
	RebuildRuleStatusRows();

	Layout.AddCustomRow(LOCTEXT("ProfileActionsSearch", "Materialize Rebase Preview Apply Detach"))
	.WholeRowContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(2)
		[
			SNew(SButton)
			.Text_Lambda([WeakRuleSet = RuleSet]()
			{
				return WeakRuleSet.IsValid() && WeakRuleSet->ProfileOrigin.IsSet()
					? LOCTEXT("PreviewRebase", "Preview Profile Rebase")
					: LOCTEXT("Materialize", "Materialize Profile");
			})
			.ToolTipText(LOCTEXT("MaterializeTooltip", "Initial use materializes immediately. Existing Profile use calculates a mutation-free Rebase Candidate and preserves local field overrides."))
			.IsEnabled_Lambda([this]() { return SelectedProfile.IsValid(); })
			.OnClicked_Lambda([this]() { return RuleSet.IsValid() && RuleSet->ProfileOrigin.IsSet() ? PreviewProfileRebase() : MaterializeProfile(); })
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2)
		[
			SNew(SButton)
			.Text(LOCTEXT("ApplyProfileRebase", "Apply Rebase Candidate"))
			.ToolTipText(LOCTEXT("ApplyProfileRebaseTooltip", "Apply the last Profile Rebase Preview. If the RuleSet changed after Preview, apply is rejected."))
			.IsEnabled_Lambda([this]() { return PendingRebase.IsSet() && PendingRebase->bSuccess; })
			.OnClicked(this, &FDataForgeRuleSetCustomization::ApplyProfileRebase)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2)
		[
			SNew(SButton)
			.Text(LOCTEXT("Detach", "Detach Profile"))
			.ToolTipText(LOCTEXT("DetachTooltip", "Remove Profile provenance and keep all concrete Asset Rules as manual rules."))
			.IsEnabled_Lambda([WeakRuleSet = RuleSet]() { return WeakRuleSet.IsValid() && WeakRuleSet->ProfileOrigin.IsSet(); })
			.OnClicked(this, &FDataForgeRuleSetCustomization::DetachProfile)
		]
	];

	IDetailCategoryBuilder& Actions = DetailBuilder.EditCategory(TEXT("DataForge Actions"), LOCTEXT("Actions", "DataForge Actions"), ECategoryPriority::Important);
	Actions.AddCustomRow(LOCTEXT("ActionsSearch", "Probe Lint Preview Apply"))
	.WholeRowContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(2)
		[
			SNew(SButton)
			.Text(LOCTEXT("Probe", "Probe"))
			.ToolTipText(LOCTEXT("ProbeTooltip", "Read columns and sample rows without changing content."))
			.OnClicked_Lambda([WeakRuleSet = RuleSet]()
			{
				if (WeakRuleSet.IsValid())
				{
					FDataForgeEditorService::Probe(*WeakRuleSet.Get());
				}
				return FReply::Handled();
			})
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2)
		[
			SNew(SButton)
			.Text(LOCTEXT("LintPreview", "Lint + Preview"))
			.ToolTipText(LOCTEXT("PreviewTooltip", "Compile rules and calculate a mutation-free diff."))
			.OnClicked_Lambda([WeakRuleSet = RuleSet]()
			{
				if (WeakRuleSet.IsValid())
				{
					FDataForgeEditorService::Preview(*WeakRuleSet.Get());
				}
				return FReply::Handled();
			})
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2)
		[
			SNew(SButton)
			.Text(LOCTEXT("Apply", "Apply Last Preview"))
			.ToolTipText(LOCTEXT("ApplyTooltip", "Apply only if the source and rules still match the last successful preview."))
			.OnClicked_Lambda([WeakRuleSet = RuleSet]()
			{
				if (WeakRuleSet.IsValid())
				{
					FDataForgeEditorService::Apply(*WeakRuleSet.Get());
				}
				return FReply::Handled();
			})
		]
	];

	Actions.AddCustomRow(LOCTEXT("Workflow", "Workflow"))
	.WholeRowContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("WorkflowText", "Workflow: Probe -> Primary Key / Output -> Asset Layout Profile or Manual Rules -> Generated Outputs -> Bindings -> Lint + Preview -> Apply"))
		.AutoWrapText(true)
	];
}

void FDataForgeRuleSetCustomization::SelectProfile(const FAssetData& AssetData)
{
	PendingRebase.Reset();
	SelectedProfile = Cast<UDataForgeAssetLayoutProfile>(AssetData.GetAsset());
	StagedParameterValues.Reset();
	if (SelectedProfile.IsValid())
	{
		const bool bSameMaterializedProfile = RuleSet.IsValid()
			&& RuleSet->ProfileOrigin.ProfileId == SelectedProfile->ProfileId;
		for (const FDataForgeProfileParameter& Parameter : SelectedProfile->Parameters)
		{
			const FString* Previous = bSameMaterializedProfile ? RuleSet->ProfileOrigin.ParameterValues.Find(Parameter.Name) : nullptr;
			StagedParameterValues.Add(Parameter.Name, Previous ? *Previous : Parameter.DefaultValue);
		}
	}
	RebuildParameterRows();
}

void FDataForgeRuleSetCustomization::RebuildParameterRows()
{
	if (!ParameterRows.IsValid()) return;
	ParameterRows->ClearChildren();
	if (!SelectedProfile.IsValid())
	{
		ParameterRows->AddSlot().AutoHeight().Padding(2)
		[
			SNew(STextBlock).Text(LOCTEXT("SelectProfileHint", "Select a Profile to configure its parameters."))
		];
		return;
	}

	for (const FDataForgeProfileParameter& Parameter : SelectedProfile->Parameters)
	{
		const FString InitialValue = StagedParameterValues.FindRef(Parameter.Name);
		const FString TypeName = StaticEnum<EDataForgeProfileParameterType>()->GetNameStringByValue(static_cast<int64>(Parameter.Type));
		ParameterRows->AddSlot().AutoHeight().Padding(2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.35f).VAlign(VAlign_Center).Padding(2)
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("ParameterLabel", "{0} ({1}){2}"), FText::FromName(Parameter.Name), FText::FromString(TypeName), Parameter.bRequired ? FText::FromString(TEXT(" *")) : FText::GetEmpty()))
				.ToolTipText(FText::FromString(Parameter.Description))
			]
			+ SHorizontalBox::Slot().FillWidth(0.65f).Padding(2)
			[
				SNew(SEditableTextBox)
				.Text(FText::FromString(InitialValue))
				.HintText(FText::FromString(Parameter.DefaultValue))
				.OnTextCommitted_Lambda([this, ParameterName = Parameter.Name](const FText& Text, ETextCommit::Type)
				{
					StagedParameterValues.Add(ParameterName, Text.ToString());
				})
			]
		];
	}
}

void FDataForgeRuleSetCustomization::RebuildRuleStatusRows()
{
	if (!RuleStatusRows.IsValid() || !RuleSet.IsValid()) return;
	RuleStatusRows->ClearChildren();
	const FDataForgeAssetLayoutAnalysis Analysis = FDataForgeAssetLayoutAuthoring::Analyze(*RuleSet.Get());
	if (Analysis.Rules.IsEmpty())
	{
		RuleStatusRows->AddSlot().AutoHeight().Padding(2)
		[
			SNew(STextBlock).Text(LOCTEXT("NoLayoutRules", "No Asset Rules are currently defined."))
		];
		return;
	}
	for (const FDataForgeAssetLayoutRuleAnalysis& Rule : Analysis.Rules)
	{
		FString Status;
		switch (Rule.Status)
		{
		case EDataForgeAssetLayoutRuleStatus::Profile: Status = TEXT("Profile"); break;
		case EDataForgeAssetLayoutRuleStatus::Override: Status = FString::Printf(TEXT("Override (%d fields)"), Rule.OverrideFieldCount); break;
		case EDataForgeAssetLayoutRuleStatus::Missing: Status = TEXT("Missing"); break;
		default: Status = TEXT("Custom"); break;
		}
		RuleStatusRows->AddSlot().AutoHeight().Padding(2)
		[
			SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%s  —  %s"), *Rule.RuleId.ToString(), *Status)))
		];
	}
}

FReply FDataForgeRuleSetCustomization::MaterializeProfile()
{
	UDataForgeRuleSet* EditedRuleSet = RuleSet.Get();
	UDataForgeAssetLayoutProfile* Profile = SelectedProfile.Get();
	if (!EditedRuleSet || !Profile) return FReply::Handled();

	FDataForgeDataSet DataSet;
	const FDataForgeResult ProbeResult = FDataForgeEditorService::Probe(*EditedRuleSet, &DataSet);
	if (!ProbeResult.bSuccess)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("ProbeRequired", "Source Probe failed. Fix the source before materializing the Profile."));
		return FReply::Handled();
	}

	const FDataForgeResult Result = FDataForgeAssetLayoutAuthoring::Materialize(
		*EditedRuleSet,
		*Profile,
		StagedParameterValues,
		DataSet.Columns);
	FDataForgeEditorService::LogResult(*EditedRuleSet, Result, !Result.bSuccess);
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Result.Summary));
	if (Result.bSuccess && DetailLayout)
	{
		DetailLayout->ForceRefreshDetails();
	}
	return FReply::Handled();
}

FReply FDataForgeRuleSetCustomization::PreviewProfileRebase()
{
	UDataForgeRuleSet* EditedRuleSet = RuleSet.Get();
	UDataForgeAssetLayoutProfile* Profile = SelectedProfile.Get();
	if (!EditedRuleSet || !Profile) return FReply::Handled();
	FDataForgeDataSet DataSet;
	const FDataForgeResult ProbeResult = FDataForgeEditorService::Probe(*EditedRuleSet, &DataSet);
	if (!ProbeResult.bSuccess)
	{
		PendingRebase.Reset();
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("RebaseProbeRequired", "Source Probe failed. Fix the source before previewing the Profile rebase."));
		return FReply::Handled();
	}
	PendingRebase = FDataForgeAssetLayoutAuthoring::PreviewRebase(*EditedRuleSet, *Profile, StagedParameterValues, DataSet.Columns);
	FString Message = PendingRebase->MakeSummary();
	for (const FDataForgeDiagnostic& Diagnostic : PendingRebase->Diagnostics)
	{
		Message += FString::Printf(TEXT("\n%s: %s"), *Diagnostic.Code, *Diagnostic.Message);
	}
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Message));
	return FReply::Handled();
}

FReply FDataForgeRuleSetCustomization::ApplyProfileRebase()
{
	UDataForgeRuleSet* EditedRuleSet = RuleSet.Get();
	if (!EditedRuleSet || !PendingRebase.IsSet()) return FReply::Handled();
	const FDataForgeResult Result = FDataForgeAssetLayoutAuthoring::ApplyRebase(*EditedRuleSet, PendingRebase.GetValue());
	FDataForgeEditorService::LogResult(*EditedRuleSet, Result, !Result.bSuccess);
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Result.Summary));
	PendingRebase.Reset();
	if (Result.bSuccess && DetailLayout) DetailLayout->ForceRefreshDetails();
	return FReply::Handled();
}

FReply FDataForgeRuleSetCustomization::DetachProfile()
{
	UDataForgeRuleSet* EditedRuleSet = RuleSet.Get();
	if (!EditedRuleSet || !EditedRuleSet->ProfileOrigin.IsSet()) return FReply::Handled();
	if (FMessageDialog::Open(
		EAppMsgType::YesNo,
		LOCTEXT("DetachConfirmation", "Detach this Profile? All concrete Asset Rules will be preserved as manual rules.")) == EAppReturnType::Yes)
	{
		FDataForgeAssetLayoutAuthoring::Detach(*EditedRuleSet);
		if (DetailLayout) DetailLayout->ForceRefreshDetails();
	}
	return FReply::Handled();
}

FText FDataForgeRuleSetCustomization::GetLayoutStatusText() const
{
	if (!RuleSet.IsValid()) return FText::GetEmpty();
	if (SelectedProfile.IsValid() && RuleSet->ProfileOrigin.IsSet() && SelectedProfile->ProfileId != RuleSet->ProfileOrigin.ProfileId)
	{
		return LOCTEXT("DifferentProfilePending", "A different Profile is selected. Detach the current Profile before materializing the new selection; concrete rules remain unchanged.");
	}
	if (SelectedProfile.IsValid() && !RuleSet->ProfileOrigin.IsSet())
	{
		return LOCTEXT("ProfileReady", "Profile selected and ready to materialize. Source Probe will validate every {Column} token before changing rules.");
	}
	return FText::FromString(FDataForgeAssetLayoutAuthoring::Analyze(*RuleSet.Get()).MakeSummary());
}

FString FDataForgeRuleSetCustomization::GetSelectedProfilePath() const
{
	return SelectedProfile.IsValid() ? SelectedProfile->GetPathName() : FString();
}

#undef LOCTEXT_NAMESPACE
