#include "NKMLocalizationCommandlet.h"

#include "NKMLocalizationEditorLog.h"
#include "NKMLocalizationPipelineRunner.h"
#include "NKMLocalizationPathConfigurator.h"
#include "NKMLocalizationSettings.h"
#include "NKMLocalizationSourceReader.h"
#include "NKMLocalizationValidator.h"
#include "NKMGameplayTextCsvAuditor.h"
#include "NKMStringTableSynchronizer.h"
#include "NKMTextBindingMigrator.h"
#include "NKMTextBindingCoverageValidator.h"
#include "NKMTextBindingReconciler.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace
{
	void LogDiagnostics(const FNKMLocalizationResult& Result)
	{
		for (const FNKMLocalizationDiagnostic& Diagnostic : Result.Diagnostics)
		{
			if (Diagnostic.Severity == ENKMLocalizationDiagnosticSeverity::Error)
			{
				UE_LOG(LogNKMLocalization, Error, TEXT("%s"), *Diagnostic.ToString());
			}
			else
			{
				UE_LOG(LogNKMLocalization, Warning, TEXT("%s"), *Diagnostic.ToString());
			}
		}
	}
}

UNKMLocalizationCommandlet::UNKMLocalizationCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UNKMLocalizationCommandlet::Main(const FString& Params)
{
	FString Mode;
	FParse::Value(*Params, TEXT("Mode="), Mode);

	if (Mode.IsEmpty())
	{
		UE_LOG(LogNKMLocalization, Error, TEXT("Usage: -run=NKMLocalization -Mode=Validate|SyncAsset|SyncSource|GatherExport|ImportCompile|Verify|AuditCsv|ApplyPaths|MigrateBindings|ReconcileBindings|ValidateCoverage [-BindingProfile=<name>] [-Input=<record.csv>] [-Source=<localization.json>] [-Report=<json>]"));
		return 1;
	}

	const UNKMLocalizationSettings* Settings = GetDefault<UNKMLocalizationSettings>();
	FString RequestedTarget;
	FParse::Value(*Params, TEXT("Target="), RequestedTarget);
	if (!RequestedTarget.IsEmpty() && !RequestedTarget.Equals(Settings->LocalizationTargetName, ESearchCase::CaseSensitive))
	{
		UE_LOG(LogNKMLocalization, Error, TEXT("Requested target '%s' does not match Project Settings target '%s'."), *RequestedTarget, *Settings->LocalizationTargetName);
		return 1;
	}

	if (Mode.Equals(TEXT("ApplyPaths"), ESearchCase::IgnoreCase))
	{
		FNKMLocalizationResult Result;
		const bool bSucceeded = FNKMLocalizationPathConfigurator::Apply(Result);
		LogDiagnostics(Result);
		return bSucceeded ? 0 : 1;
	}

	if (Mode.Equals(TEXT("MigrateBindings"), ESearchCase::IgnoreCase))
	{
		FNKMLocalizationResult Result;
		const bool bSucceeded = FNKMTextBindingMigrator::MigrateConfiguredProfiles(
			FParse::Param(*Params, TEXT("ForceBindingSourceOverwrite")),
			Result);
		LogDiagnostics(Result);
		return bSucceeded ? 0 : 1;
	}

	if (Mode.Equals(TEXT("ReconcileBindings"), ESearchCase::IgnoreCase)
		|| Mode.Equals(TEXT("ValidateCoverage"), ESearchCase::IgnoreCase))
	{
		FString BindingProfileName;
		FString Input;
		FString Source;
		FParse::Value(*Params, TEXT("BindingProfile="), BindingProfileName);
		FParse::Value(*Params, TEXT("Input="), Input);
		FParse::Value(*Params, TEXT("Source="), Source);
		const FNKMTextBindingProfile* BindingProfile = Settings->FindTextBindingProfile(FName(*BindingProfileName));
		FNKMLocalizationResult Result;
		if (!BindingProfile)
		{
			Result.AddError(TEXT("NKMLOC_BINDING_PROFILE"), FString::Printf(TEXT("Unknown binding profile '%s'."), *BindingProfileName));
			LogDiagnostics(Result);
			return 1;
		}

		if (Source.IsEmpty())
		{
			Source = Settings->GetNormalizedAuthoringSourceRoot() / BindingProfile->AuthoringSourceFile;
		}
		if (FPaths::IsRelative(Source)) Source = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), Source);

		FNKMLocalizationSourceContext Context;
		Context.Target = Settings->LocalizationTargetName;
		Context.NativeCulture = Settings->NativeCulture;
		FNKMLocalizationDocument Document;
		if (!FNKMLocalizationSourceReader::LoadFile(Source, Context, Document, Result)
			|| !FNKMLocalizationValidator::Validate(Document, Result))
		{
			LogDiagnostics(Result);
			return 1;
		}

		TUniquePtr<INKMLocalizationRecordProvider> Provider = FNKMLocalizationRecordProviderFactory::Create(*BindingProfile, Input, Result);
		if (!Provider)
		{
			LogDiagnostics(Result);
			return 1;
		}

		if (Mode.Equals(TEXT("ReconcileBindings"), ESearchCase::IgnoreCase))
		{
			FNKMTextBindingReconcileReport ReconcileReport;
			const bool bSucceeded = FNKMTextBindingReconciler::Reconcile(*BindingProfile, *Provider, Document, ReconcileReport, Result);
			LogDiagnostics(Result);
			UE_LOG(LogNKMLocalization, Display, TEXT("Binding reconcile '%s': records=%d added=%d metadata=%d orphans=%d. Use the editor dashboard to enter native text and save."), *BindingProfileName, ReconcileReport.RecordCount, ReconcileReport.AddedKeys.Num(), ReconcileReport.UpdatedMetadataKeys.Num(), ReconcileReport.OrphanKeys.Num());
			return bSucceeded ? 0 : 1;
		}

		FNKMTextBindingCoverageReport CoverageReport;
		const bool bSucceeded = FNKMTextBindingCoverageValidator::Validate(*BindingProfile, *Provider, Document, CoverageReport, Result);
		LogDiagnostics(Result);
		UE_LOG(LogNKMLocalization, Display, TEXT("Binding coverage '%s': records=%d expected=%d missing=%d invalid=%d orphans=%d."), *BindingProfileName, CoverageReport.RecordCount, CoverageReport.ExpectedKeyCount, CoverageReport.MissingKeys.Num(), CoverageReport.InvalidMetadataKeys.Num(), CoverageReport.OrphanKeys.Num());
		return bSucceeded ? 0 : 1;
	}

	if (Mode.Equals(TEXT("AuditCsv"), ESearchCase::IgnoreCase))
	{
		FString Input;
		FString Source;
		FParse::Value(*Params, TEXT("Input="), Input);
		FParse::Value(*Params, TEXT("Source="), Source);
		if (Input.IsEmpty() || Source.IsEmpty())
		{
			UE_LOG(LogNKMLocalization, Error, TEXT("AuditCsv requires -Input=<gameplay.csv> and -Source=<localization.json|csv>."));
			return 1;
		}
		if (FPaths::IsRelative(Input)) Input = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), Input);
		if (FPaths::IsRelative(Source)) Source = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), Source);

		FNKMLocalizationSourceContext SourceContext;
		SourceContext.Target = Settings->LocalizationTargetName;
		SourceContext.NativeCulture = Settings->NativeCulture;
		FParse::Value(*Params, TEXT("TableId="), SourceContext.TableId);
		FParse::Value(*Params, TEXT("AssetPath="), SourceContext.AssetPath);
		FNKMLocalizationDocument Document;
		FNKMLocalizationResult Result;
		if (!FNKMLocalizationSourceReader::LoadFile(Source, SourceContext, Document, Result)
			|| !FNKMLocalizationValidator::Validate(Document, Result))
		{
			LogDiagnostics(Result);
			return 1;
		}

		FString DefaultTableValue;
		FParse::Value(*Params, TEXT("DefaultTableId="), DefaultTableValue);
		FString ColumnsValue;
		FParse::Value(*Params, TEXT("Columns="), ColumnsValue);
		TArray<FString> Columns;
		ColumnsValue.ParseIntoArray(Columns, TEXT(","), true);
		for (FString& Column : Columns) Column.TrimStartAndEndInline();

		FNKMGameplayTextCsvAuditReport AuditReport;
		const bool bSucceeded = FNKMGameplayTextCsvAuditor::AuditFile(
			Input,
			Document,
			FName(*DefaultTableValue),
			Columns,
			AuditReport,
			Result);
		for (const FNKMGameplayTextCsvIssue& Issue : AuditReport.Issues)
		{
			UE_LOG(LogNKMLocalization, Error, TEXT("%s [row=%d column=%s value=%s]: %s"), *Issue.Code, Issue.Row, *Issue.Column, *Issue.Value, *Issue.Message);
		}

		FString ReportPath;
		if (!FParse::Value(*Params, TEXT("Report="), ReportPath))
		{
			ReportPath = FPaths::ProjectSavedDir() / FString::Printf(TEXT("NKMLocalization/%sCsvAudit.json"), *Settings->LocalizationTargetName);
		}
		else if (FPaths::IsRelative(ReportPath))
		{
			ReportPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), ReportPath);
		}
		if (!FNKMGameplayTextCsvAuditor::WriteReport(ReportPath, AuditReport))
		{
			Result.AddError(TEXT("NKMLOC_AUDIT_REPORT"), FString::Printf(TEXT("Failed to write CSV audit report '%s'."), *ReportPath));
		}
		LogDiagnostics(Result);
		UE_LOG(LogNKMLocalization, Display, TEXT("Gameplay CSV audit checked %d reference(s), found %d issue(s)."), AuditReport.CheckedReferenceCount, AuditReport.Issues.Num());
		return bSucceeded && !Result.HasErrors() ? 0 : 1;
	}

	const bool bPipelineOnlyMode = Mode.Equals(TEXT("GatherExport"), ESearchCase::IgnoreCase)
		|| Mode.Equals(TEXT("ImportCompile"), ESearchCase::IgnoreCase)
		|| Mode.Equals(TEXT("Verify"), ESearchCase::IgnoreCase);
	if (bPipelineOnlyMode)
	{
		FNKMLocalizationResult Result;
		if (!FNKMLocalizationPathConfigurator::Apply(Result))
		{
			LogDiagnostics(Result);
			return 1;
		}
		if ((Mode.Equals(TEXT("GatherExport"), ESearchCase::IgnoreCase)
			|| Mode.Equals(TEXT("Verify"), ESearchCase::IgnoreCase))
			&& !FNKMTextBindingCoverageValidator::ValidateConfiguredProfiles(Result))
		{
			LogDiagnostics(Result);
			return 1;
		}
		FNKMLocalizationPipelineReport Report;
		const bool bSucceeded = FNKMLocalizationPipelineRunner::Run(
			Mode,
			FParse::Param(*Params, TEXT("ForcePOOverwrite")),
			Result,
			Report);

		FString ReportPath;
		if (!FParse::Value(*Params, TEXT("Report="), ReportPath))
		{
			ReportPath = FPaths::ProjectSavedDir() / FString::Printf(TEXT("NKMLocalization/%sReport.json"), *Settings->LocalizationTargetName);
		}
		else if (FPaths::IsRelative(ReportPath))
		{
			ReportPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), ReportPath);
		}

		if (!FNKMLocalizationPipelineRunner::WriteReport(ReportPath, Report, Result))
		{
			Result.AddError(TEXT("NKMLOC-REPORT-WRITE"), FString::Printf(TEXT("Failed to write pipeline report '%s'."), *ReportPath));
		}

		LogDiagnostics(Result);
		UE_LOG(LogNKMLocalization, Display, TEXT("%s %s %s. Report: %s"), *Settings->LocalizationTargetName, *Mode, bSucceeded ? TEXT("succeeded") : TEXT("failed"), *ReportPath);
		return bSucceeded && !Result.HasErrors() ? 0 : 1;
	}

	FString Input;
	FParse::Value(*Params, TEXT("Input="), Input);
	if (Input.IsEmpty())
	{
		UE_LOG(LogNKMLocalization, Error, TEXT("Mode '%s' requires -Input=<json|csv>."), *Mode);
		return 1;
	}

	FNKMLocalizationSourceContext Context;
	Context.Target = Settings->LocalizationTargetName;
	Context.NativeCulture = Settings->NativeCulture;
	FParse::Value(*Params, TEXT("TableId="), Context.TableId);
	FParse::Value(*Params, TEXT("AssetPath="), Context.AssetPath);

	if (FPaths::IsRelative(Input))
	{
		Input = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), Input);
	}

	FNKMLocalizationResult Result;
	FNKMLocalizationDocument Document;
	if (!FNKMLocalizationSourceReader::LoadFile(Input, Context, Document, Result)
		|| !FNKMLocalizationValidator::Validate(Document, Result))
	{
		LogDiagnostics(Result);
		return 1;
	}

	if (Mode.Equals(TEXT("Validate"), ESearchCase::IgnoreCase))
	{
		UE_LOG(LogNKMLocalization, Display, TEXT("Validated %d table(s) from %s."), Document.Tables.Num(), *Input);
		LogDiagnostics(Result);
		return 0;
	}

	if (Mode.Equals(TEXT("SyncAsset"), ESearchCase::IgnoreCase)
		|| Mode.Equals(TEXT("SyncSource"), ESearchCase::IgnoreCase))
	{
		if (!FNKMLocalizationPathConfigurator::Apply(Result))
		{
			LogDiagnostics(Result);
			return 1;
		}
		if (!FNKMTextBindingCoverageValidator::ValidateConfiguredProfiles(Result))
		{
			LogDiagnostics(Result);
			return 1;
		}
		TArray<FNKMStringTableDiff> Diffs;
		if (!FNKMStringTableSynchronizer::Sync(Document, Diffs, Result))
		{
			LogDiagnostics(Result);
			return 1;
		}

		for (const FNKMStringTableDiff& Diff : Diffs)
		{
			UE_LOG(
				LogNKMLocalization,
				Display,
				TEXT("%s -> %s: +%d ~%d -%d namespace=%s"),
				*Diff.TableId,
				*Diff.AssetPath,
				Diff.Added,
				Diff.Updated,
				Diff.Removed,
				Diff.bNamespaceChanged ? TEXT("changed") : TEXT("unchanged"));
		}

		if (Mode.Equals(TEXT("SyncAsset"), ESearchCase::IgnoreCase))
		{
			LogDiagnostics(Result);
			return 0;
		}

		FNKMLocalizationPipelineReport Report;
		const bool bSucceeded = FNKMLocalizationPipelineRunner::Run(
			TEXT("GatherExport"),
			FParse::Param(*Params, TEXT("ForcePOOverwrite")),
			Result,
			Report);
		Report.Mode = TEXT("SyncSource");

		FString ReportPath;
		if (!FParse::Value(*Params, TEXT("Report="), ReportPath))
		{
			ReportPath = FPaths::ProjectSavedDir() / FString::Printf(TEXT("NKMLocalization/%sReport.json"), *Settings->LocalizationTargetName);
		}
		else if (FPaths::IsRelative(ReportPath))
		{
			ReportPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), ReportPath);
		}

		if (!FNKMLocalizationPipelineRunner::WriteReport(ReportPath, Report, Result))
		{
			Result.AddError(TEXT("NKMLOC-REPORT-WRITE"), FString::Printf(TEXT("Failed to write pipeline report '%s'."), *ReportPath));
		}

		LogDiagnostics(Result);
		return bSucceeded && !Result.HasErrors() ? 0 : 1;
	}

	UE_LOG(LogNKMLocalization, Error, TEXT("Unsupported Mode '%s'."), *Mode);
	return 1;
}
