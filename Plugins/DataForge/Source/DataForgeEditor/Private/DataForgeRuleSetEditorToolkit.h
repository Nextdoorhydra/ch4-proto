#pragma once

#include "CoreMinimal.h"
#include "DataForgeTypes.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Widgets/Views/SListView.h"

class IDetailsView;
class IToolkitHost;
class SHeaderRow;
class UDataForgeRuleSet;

class FDataForgeRuleSetEditorToolkit final : public FAssetEditorToolkit
{
public:
	void InitEditor(UDataForgeRuleSet* InRuleSet, EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost);

	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

private:
	TSharedRef<SDockTab> SpawnMainTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnBindingGraphTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnDependencyGraphTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnSemanticDiffTab(const FSpawnTabArgs& Args);
	FReply ProbeSource();
	FReply OpenCreationWizard();
	FReply OpenBindingGraph();
	FReply OpenDependencyGraph();
	FReply OpenSemanticDiff();
	FReply AutoMapExactNames();
	FReply Preview();
	FReply Apply();
	FReply CleanupOrphans();
	FReply ExportSnapshots();
	void RefreshSourceRows();
	void RefreshPlanRows();
	FText GetStatusText() const;

	TWeakObjectPtr<UDataForgeRuleSet> RuleSet;
	FDataForgeDataSet ProbedDataSet;
	FDataForgeApplyPlan PreviewPlan;
	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<SHeaderRow> SourceHeader;
	TSharedPtr<SListView<TSharedPtr<FDataForgeRow>>> SourceList;
	TArray<TSharedPtr<FDataForgeRow>> SourceRows;
	TSharedPtr<SListView<TSharedPtr<FString>>> PlanList;
	TArray<TSharedPtr<FString>> PlanRows;

	static const FName MainTabId;
	static const FName BindingGraphTabId;
	static const FName DependencyGraphTabId;
	static const FName SemanticDiffTabId;
};
