#pragma once

#include "DataForgeAssetLayoutAuthoring.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class SVerticalBox;
class UDataForgeAssetLayoutProfile;
class UDataForgeRuleSet;
struct FAssetData;

class FDataForgeRuleSetCustomization final : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	void SelectProfile(const FAssetData& AssetData);
	void RebuildParameterRows();
	void RebuildRuleStatusRows();
	FReply MaterializeProfile();
	FReply PreviewProfileRebase();
	FReply ApplyProfileRebase();
	FReply DetachProfile();
	FText GetLayoutStatusText() const;
	FString GetSelectedProfilePath() const;

	TWeakObjectPtr<UDataForgeRuleSet> RuleSet;
	TWeakObjectPtr<UDataForgeAssetLayoutProfile> SelectedProfile;
	TMap<FName, FString> StagedParameterValues;
	TOptional<FDataForgeAssetLayoutRebaseCandidate> PendingRebase;
	TSharedPtr<SVerticalBox> ParameterRows;
	TSharedPtr<SVerticalBox> RuleStatusRows;
	IDetailLayoutBuilder* DetailLayout = nullptr;
};
