#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Dom/JsonObject.h"
#include "Engine/DataTable.h"
#include "NKMLocalizationSourceReader.h"
#include "NKMLocalizationSourceWriter.h"
#include "NKMLocalizationPipelineRunner.h"
#include "NKMLocalizationSettings.h"
#include "NKMLocalizationValidator.h"
#include "NKMGameplayTextCsvAuditor.h"
#include "NKMStringTableSynchronizer.h"
#include "NKMTextRef.h"
#include "NKMTextBindingCoverageValidator.h"
#include "NKMTextBindingReconciler.h"
#include "NKMTranslationStatusAnalyzer.h"
#include "Tests/NKMTextRefDataTableTestTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNKMTextBindingSettingsTest,
	"NKM.Localization.Binding.ProjectSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNKMTextBindingSettingsTest::RunTest(const FString& Parameters)
{
	const UNKMLocalizationSettings* Settings = GetDefault<UNKMLocalizationSettings>();
	if (!TestFalse(TEXT("Supported cultures are configured"), Settings->SupportedCultures.IsEmpty()))
	{
		return false;
	}
	TestEqual(TEXT("Native culture is first"), Settings->SupportedCultures[0], Settings->NativeCulture);

	FNKMTextRef ItemDisplayName;
	TestTrue(TEXT("Item binding resolves"), Settings->TryMakeTextReference(
		TEXT("Items"), TEXT("Item_1"), TEXT("DisplayName"), ItemDisplayName));
	TestEqual(TEXT("Item binding table"), ItemDisplayName.TableId, FName(TEXT("NKM.Items")));
	TestEqual(TEXT("Item binding key"), ItemDisplayName.Key, FString(TEXT("Item.Item_1.DisplayName")));

	FNKMTextRef UnknownField;
	TestFalse(TEXT("Unknown binding field is rejected"), Settings->TryMakeTextReference(
		TEXT("Items"), TEXT("Item_1"), TEXT("Unknown"), UnknownField));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNKMLocalizationStructuredJsonTest,
	"NKM.Localization.Source.StructuredJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNKMLocalizationStructuredJsonTest::RunTest(const FString& Parameters)
{
	const FString Source = TEXT(R"JSON(
{
  "schemaVersion": 1,
  "target": "NKMText",
  "nativeCulture": "ko",
  "tables": [{
    "id": "NKM.UI",
    "assetPath": "/Game/NetKarma/Localization/StringTables/ST_NKM_UI",
    "entries": [{
      "key": "Shop.BuyAction",
      "source": "구매",
      "comment": "상점 구매 버튼",
      "tags": ["UI", "Button"]
    }]
  }]
}
)JSON");

	FNKMLocalizationDocument Document;
	FNKMLocalizationResult Result;
	TestTrue(TEXT("Structured JSON parses"), FNKMLocalizationSourceReader::ParseJson(Source, {}, Document, Result));
	TestTrue(TEXT("Structured JSON validates"), FNKMLocalizationValidator::Validate(Document, Result));
	TestFalse(TEXT("Structured JSON has no errors"), Result.HasErrors());
	TestEqual(TEXT("One table parsed"), Document.Tables.Num(), 1);
	TestEqual(TEXT("One entry parsed"), Document.Tables[0].Entries.Num(), 1);
	TestEqual(TEXT("Comment metadata retained"), Document.Tables[0].Entries[0].MetaData.FindRef(TEXT("Comment")), FString(TEXT("상점 구매 버튼")));
	return !Result.HasErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNKMLocalizationSimpleJsonTest,
	"NKM.Localization.Source.SimpleKeyValueJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNKMLocalizationSimpleJsonTest::RunTest(const FString& Parameters)
{
	FNKMLocalizationSourceContext Context;
	Context.TableId = TEXT("NKM.Items");
	Context.AssetPath = TEXT("/Game/NetKarma/Localization/StringTables/ST_NKM_Items");

	FNKMLocalizationDocument Document;
	FNKMLocalizationResult Result;
	TestTrue(
		TEXT("Simple key-value JSON parses"),
		FNKMLocalizationSourceReader::ParseJson(TEXT(R"JSON({"Weapon.Hose.DisplayName":"호스"})JSON"), Context, Document, Result));
	TestTrue(TEXT("Simple key-value JSON validates"), FNKMLocalizationValidator::Validate(Document, Result));
	TestEqual(TEXT("Native culture defaults to ko"), Document.NativeCulture, FString(TEXT("ko")));

	Document.NativeCulture = TEXT("en");
	FNKMLocalizationResult InvalidCultureResult;
	TestFalse(
		TEXT("Non-ko native culture is rejected"),
		FNKMLocalizationValidator::Validate(Document, InvalidCultureResult));

	Document.NativeCulture = TEXT("ko");
	Document.Tables[0].AssetPath = TEXT("/Game/NetKarma/Localization/StringTables/ST_NKM_ItemText");
	FNKMLocalizationResult AliasMismatchResult;
	TestFalse(
		TEXT("Source asset must match runtime alias mapping"),
		FNKMLocalizationValidator::Validate(Document, AliasMismatchResult));

	TArray<FNKMStringTableDiff> InvalidDiffs;
	FNKMLocalizationResult InvalidSyncResult;
	TestFalse(
		TEXT("Synchronizer refuses invalid document before asset access"),
		FNKMStringTableSynchronizer::Sync(Document, InvalidDiffs, InvalidSyncResult));
	TestTrue(TEXT("Rejected sync produces no asset diffs"), InvalidDiffs.IsEmpty());
	return !Result.HasErrors() && InvalidCultureResult.HasErrors() && InvalidSyncResult.HasErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNKMLocalizationCsvAndDuplicateTest,
	"NKM.Localization.Source.CsvAndDuplicateValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNKMLocalizationCsvAndDuplicateTest::RunTest(const FString& Parameters)
{
	FNKMLocalizationSourceContext Context;
	Context.TableId = TEXT("NKM.UI");
	Context.AssetPath = TEXT("/Game/NetKarma/Localization/StringTables/ST_NKM_UI");

	FNKMLocalizationDocument Document;
	FNKMLocalizationResult Result;
	const FString Source = TEXT("Key,SourceString,Comment\nShop.BuyAction,구매,버튼\nshop.buyaction,Buy,Duplicate");
	TestTrue(TEXT("CSV parses before semantic validation"), FNKMLocalizationSourceReader::ParseCsv(Source, Context, Document, Result));
	TestFalse(TEXT("Case-only duplicate is rejected"), FNKMLocalizationValidator::Validate(Document, Result));
	TestTrue(TEXT("Duplicate produces an error"), Result.HasErrors());
	return Result.HasErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNKMStringTableDiffTest,
	"NKM.Localization.StringTable.SemanticDiff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNKMStringTableDiffTest::RunTest(const FString& Parameters)
{
	UStringTable* Existing = NewObject<UStringTable>();
	Existing->GetMutableStringTable()->SetNamespace(TEXT("NKM.UI"));
	Existing->GetMutableStringTable()->SetSourceString(TEXT("Shop.BuyAction"), TEXT("구매"));

	FNKMLocalizationTable Desired;
	Desired.Id = TEXT("NKM.UI");
	Desired.AssetPath = TEXT("/Game/NetKarma/Localization/StringTables/ST_NKM_UI");
	Desired.Entries.Add({TEXT("Shop.BuyAction"), TEXT("구매"), {}});

	const FNKMStringTableDiff NoChange = FNKMStringTableSynchronizer::BuildDiff(Existing, Desired);
	TestFalse(TEXT("Identical table has no semantic changes"), NoChange.HasChanges());

	Desired.Entries[0].SourceString = TEXT("구매하기");
	const FNKMStringTableDiff Changed = FNKMStringTableSynchronizer::BuildDiff(Existing, Desired);
	TestEqual(TEXT("Source edit is one update"), Changed.Updated, 1);

	Desired.Entries[0].MetaData.Add(TEXT("Comment"), TEXT("상점 구매 버튼"));
	FNKMStringTableSynchronizer::ApplyToAsset(*Existing, Desired);
	const FNKMStringTableDiff AfterApply = FNKMStringTableSynchronizer::BuildDiff(Existing, Desired);
	TestFalse(TEXT("Applied table is idempotent"), AfterApply.HasChanges());

	FString AppliedSource;
	TestTrue(
		TEXT("Applied source exists"),
		Existing->GetStringTable()->GetSourceString(TEXT("Shop.BuyAction"), AppliedSource));
	TestEqual(TEXT("Applied source matches"), AppliedSource, FString(TEXT("구매하기")));
	return !NoChange.HasChanges() && Changed.Updated == 1 && !AfterApply.HasChanges();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNKMLocalizationPOLedgerDiffTest,
	"NKM.Localization.Pipeline.POLedgerDiff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNKMLocalizationPOLedgerDiffTest::RunTest(const FString& Parameters)
{
	const TMap<FString, FString> Expected = {
		{TEXT("ko/NKMText.po"), TEXT("aaa")},
		{TEXT("en/NKMText.po"), TEXT("bbb")}};

	TMap<FString, FString> Current = Expected;
	TArray<FString> Changed;
	FNKMLocalizationPOLedger::FindChangedFiles(Expected, Current, Changed);
	TestTrue(TEXT("Identical PO snapshot is clean"), Changed.IsEmpty());

	Current[TEXT("en/NKMText.po")] = TEXT("changed");
	Current.Add(TEXT("ja/NKMText.po"), TEXT("new"));
	Changed.Reset();
	FNKMLocalizationPOLedger::FindChangedFiles(Expected, Current, Changed);
	TestEqual(TEXT("Changed and added PO files are detected"), Changed.Num(), 2);
	TestEqual(TEXT("Changed paths are deterministic"), Changed[0], FString(TEXT("en/NKMText.po")));
	TestEqual(TEXT("Added path is detected"), Changed[1], FString(TEXT("ja/NKMText.po")));

	return Changed.Num() == 2;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNKMTextRefParseTest,
	"NKM.Localization.Gameplay.TextReference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNKMTextRefParseTest::RunTest(const FString& Parameters)
{
	FNKMTextRef FullReference;
	TestTrue(TEXT("Full reference parses"), FNKMTextRef::Parse(TEXT("NKM.Items::Weapon.Hose.DisplayName"), NAME_None, FullReference));
	TestEqual(TEXT("Alias retained"), FullReference.TableId, FName(TEXT("NKM.Items")));
	TestEqual(TEXT("Key retained"), FullReference.Key, FString(TEXT("Weapon.Hose.DisplayName")));

	FNKMTextRef KeyOnlyReference;
	TestTrue(TEXT("Key-only reference parses with profile table"), FNKMTextRef::Parse(TEXT("Weapon.Hose.Description"), TEXT("NKM.Items"), KeyOnlyReference));
	TestFalse(TEXT("Key-only reference without table is rejected"), FNKMTextRef::Parse(TEXT("Weapon.Hose.Description"), NAME_None, KeyOnlyReference));

	FName RuntimeTableId;
	TestTrue(TEXT("Conventional alias resolves"), FNKMTextRef::ResolveTableId(TEXT("NKM.Items"), RuntimeTableId));
	TestEqual(
		TEXT("Conventional object path generated"),
		RuntimeTableId,
		FName(TEXT("/Game/NetKarma/Localization/StringTables/ST_NKM_Items.ST_NKM_Items")));

	const TCHAR* ImportedBuffer = TEXT("\"NKM.UI::Shop.BuyAction\"");
	FNKMTextRef Imported;
	TestTrue(TEXT("CSV property token imports"), Imported.ImportTextItem(ImportedBuffer, PPF_Delimited, nullptr, GWarn));
	TestEqual(TEXT("Imported token round-trips"), Imported.ToString(), FString(TEXT("NKM.UI::Shop.BuyAction")));

	UPackage* ResolvePackage = CreatePackage(TEXT("/Game/NetKarma/Localization/StringTables/ST_NKM_TestResolve"));
	UStringTable* ResolveTable = NewObject<UStringTable>(ResolvePackage, TEXT("ST_NKM_TestResolve"), RF_Public | RF_Standalone | RF_Transient);
	ResolveTable->GetMutableStringTable()->SetNamespace(TEXT("NKM.TestResolve"));
	ResolveTable->GetMutableStringTable()->SetSourceString(TEXT("Sample.Key"), TEXT("실제 해석"));
	FName TestRuntimeTableId;
	TestTrue(TEXT("Test alias resolves"), FNKMTextRef::ResolveTableId(TEXT("NKM.TestResolve"), TestRuntimeTableId));
	FStringTableRegistry::Get().RegisterStringTable(TestRuntimeTableId, ResolveTable->GetMutableStringTable());
	const FNKMTextRef RuntimeReference(TEXT("NKM.TestResolve"), TEXT("Sample.Key"));
	TestEqual(TEXT("Preloaded table resolves without loading"), RuntimeReference.Resolve().ToString(), FString(TEXT("실제 해석")));

	UDataTable* DataTable = NewObject<UDataTable>();
	DataTable->RowStruct = FNKMTextRefDataTableTestRow::StaticStruct();
	const TArray<FString> ImportProblems = DataTable->CreateTableFromCSVString(
		TEXT("Name,DisplayName\nHose,NKM.Items::Weapon.Hose.DisplayName"));
	TestTrue(TEXT("Actual DataTable CSV import has no problem"), ImportProblems.IsEmpty());
	const FNKMTextRefDataTableTestRow* ImportedRow = DataTable->FindRow<FNKMTextRefDataTableTestRow>(TEXT("Hose"), TEXT("NKM localization test"));
	TestNotNull(TEXT("Imported DataTable row exists"), ImportedRow);
	if (ImportedRow)
	{
		TestEqual(TEXT("DataTable imports FNKMTextRef"), ImportedRow->DisplayName.ToString(), FString(TEXT("NKM.Items::Weapon.Hose.DisplayName")));
	}
	FStringTableRegistry::Get().UnregisterStringTable(TestRuntimeTableId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNKMGameplayTextCsvAuditTest,
	"NKM.Localization.Gameplay.CsvMissingKeyAudit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNKMGameplayTextCsvAuditTest::RunTest(const FString& Parameters)
{
	FNKMLocalizationDocument Document;
	Document.Tables.Add({
		TEXT("NKM.Items"),
		TEXT("/Game/NetKarma/Localization/StringTables/ST_NKM_Items"),
		{{TEXT("Weapon.Hose.DisplayName"), TEXT("호스"), {}}}});

	const FString CsvPath = FPaths::CreateTempFilename(FPlatformProcess::UserTempDir(), TEXT("NKMTextAudit"), TEXT(".csv"));
	const FString Csv = TEXT("ItemId,DisplayName,Description\nHose,Weapon.Hose.DisplayName,NKM.Items::Weapon.Hose.Missing");
	TestTrue(TEXT("Fixture CSV saved"), FFileHelper::SaveStringToFile(Csv, *CsvPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

	FNKMGameplayTextCsvAuditReport Audit;
	FNKMLocalizationResult Result;
	TestFalse(
		TEXT("Missing reference fails audit"),
		FNKMGameplayTextCsvAuditor::AuditFile(CsvPath, Document, TEXT("NKM.Items"), {}, Audit, Result));
	TestEqual(TEXT("Two references checked"), Audit.CheckedReferenceCount, 2);
	TestEqual(TEXT("One missing key reported"), Audit.Issues.Num(), 1);
	if (!Audit.Issues.IsEmpty())
	{
		TestEqual(TEXT("Missing key code"), Audit.Issues[0].Code, FString(TEXT("NKMLOC_AUDIT_KEY")));
	}

	IFileManager::Get().Delete(*CsvPath, false, true);
	return Audit.Issues.Num() == 1;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNKMGameplayTextCsvStrictAuditTest,
	"NKM.Localization.Gameplay.CsvStrictAudit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNKMGameplayTextCsvStrictAuditTest::RunTest(const FString& Parameters)
{
	FNKMLocalizationDocument Document;
	Document.Tables.Add({
		TEXT("NKM.Items"),
		TEXT("/Game/NetKarma/Localization/StringTables/ST_NKM_Items"),
		{{TEXT("Weapon.Hose.DisplayName"), TEXT("호스"), {}}}});

	const FString CsvPath = FPaths::CreateTempFilename(FPlatformProcess::UserTempDir(), TEXT("NKMTextStrictAudit"), TEXT(".csv"));
	TestTrue(TEXT("Empty-ref CSV saved"), FFileHelper::SaveStringToFile(
		TEXT("ItemId,DisplayName\nHose,"), *CsvPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	FNKMGameplayTextCsvAuditReport EmptyAudit;
	FNKMLocalizationResult EmptyResult;
	TestFalse(TEXT("Empty required reference fails"), FNKMGameplayTextCsvAuditor::AuditFile(
		CsvPath, Document, TEXT("NKM.Items"), {}, EmptyAudit, EmptyResult));
	TestTrue(TEXT("Empty reference is reported"), EmptyAudit.Issues.ContainsByPredicate(
		[](const FNKMGameplayTextCsvIssue& Issue) { return Issue.Code == TEXT("NKMLOC_AUDIT_EMPTY_REF"); }));

	TestTrue(TEXT("Duplicate-header CSV saved"), FFileHelper::SaveStringToFile(
		TEXT("ItemId,DisplayName,displayname\nHose,NKM.Items::Weapon.Hose.DisplayName,NKM.Items::Weapon.Hose.DisplayName"),
		*CsvPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	FNKMGameplayTextCsvAuditReport DuplicateAudit;
	FNKMLocalizationResult DuplicateResult;
	TestFalse(TEXT("Case-only duplicate header fails"), FNKMGameplayTextCsvAuditor::AuditFile(
		CsvPath, Document, TEXT("NKM.Items"), {}, DuplicateAudit, DuplicateResult));
	TestTrue(TEXT("Duplicate header creates diagnostic"), DuplicateResult.HasErrors());

	IFileManager::Get().Delete(*CsvPath, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNKMLocalizationSourceWriterRoundTripTest,
	"NKM.Localization.Source.WriterRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNKMLocalizationSourceWriterRoundTripTest::RunTest(const FString& Parameters)
{
	FNKMLocalizationDocument Original;
	Original.Target = GetDefault<UNKMLocalizationSettings>()->LocalizationTargetName;
	Original.NativeCulture = GetDefault<UNKMLocalizationSettings>()->NativeCulture;
	Original.Tables.Add({
		TEXT("NKM.UI"),
		TEXT("/Game/NetKarma/Localization/StringTables/ST_NKM_UI"),
		{{TEXT("Shop.BuyAction"), TEXT("구매"), {{TEXT("Comment"), TEXT("구매 버튼")}}}}});
	TSharedRef<FJsonObject> Redirect = MakeShared<FJsonObject>();
	Redirect->SetStringField(TEXT("tableId"), TEXT("NKM.UI"));
	Redirect->SetStringField(TEXT("from"), TEXT("Shop.Buy"));
	Redirect->SetStringField(TEXT("to"), TEXT("Shop.BuyAction"));
	Original.Redirects.Add(MakeShared<FJsonValueObject>(Redirect));

	const FString JsonPath = FPaths::CreateTempFilename(FPlatformProcess::UserTempDir(), TEXT("NKMTextWriter"), TEXT(".json"));
	FNKMLocalizationResult SaveResult;
	TestTrue(TEXT("Canonical JSON saves"), FNKMLocalizationSourceWriter::SaveStructuredJson(JsonPath, Original, SaveResult));

	FNKMLocalizationDocument Loaded;
	FNKMLocalizationResult LoadResult;
	FNKMLocalizationSourceContext Context;
	TestTrue(TEXT("Saved JSON loads"), FNKMLocalizationSourceReader::LoadFile(JsonPath, Context, Loaded, LoadResult));
	TestTrue(TEXT("Saved JSON validates"), FNKMLocalizationValidator::Validate(Loaded, LoadResult));
	TestEqual(TEXT("Round-trip table count"), Loaded.Tables.Num(), 1);
	TestEqual(TEXT("Round-trip source"), Loaded.Tables[0].Entries[0].SourceString, FString(TEXT("구매")));
	TestEqual(TEXT("Redirect records survive round-trip"), Loaded.Redirects.Num(), 1);

	IFileManager::Get().Delete(*JsonPath, false, true);
	return !SaveResult.HasErrors() && !LoadResult.HasErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNKMTextBindingReconcileCoverageTest,
	"NKM.Localization.Binding.ReconcileAndCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNKMTextBindingReconcileCoverageTest::RunTest(const FString& Parameters)
{
	const UNKMLocalizationSettings* Settings = GetDefault<UNKMLocalizationSettings>();
	const FNKMTextBindingProfile* Profile = Settings->FindTextBindingProfile(TEXT("Items"));
	if (!TestNotNull(TEXT("Items profile exists"), Profile))
	{
		return false;
	}

	const FString CsvPath = FPaths::CreateTempFilename(FPlatformProcess::UserTempDir(), TEXT("NKMBindingRecords"), TEXT(".csv"));
	TestTrue(TEXT("Record CSV saved"), FFileHelper::SaveStringToFile(
		TEXT("ItemID,NumericValue\nItem_1,10\nItem_2,20"),
		*CsvPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

	FNKMLocalizationResult ProviderResult;
	TUniquePtr<INKMLocalizationRecordProvider> Provider = FNKMLocalizationRecordProviderFactory::Create(*Profile, CsvPath, ProviderResult);
	if (!TestTrue(TEXT("CSV record provider created"), Provider.IsValid()))
	{
		IFileManager::Get().Delete(*CsvPath, false, true);
		return false;
	}

	FNKMLocalizationDocument Document;
	Document.Target = Settings->LocalizationTargetName;
	Document.NativeCulture = Settings->NativeCulture;
	FNKMLocalizationTable& Table = Document.Tables.AddDefaulted_GetRef();
	Table.Id = Profile->TableId.ToString();
	Table.AssetPath = Settings->MakeConventionalTablePackagePath(Profile->TableId);
	Table.Entries.Add({TEXT("Item.Item_1.DisplayName"), TEXT("One"), {}});
	Table.Entries.Add({TEXT("Item.Item_1.Description"), TEXT("First"), {}});

	FNKMTextBindingReconcileReport ReconcileReport;
	FNKMLocalizationResult ReconcileResult;
	TestTrue(TEXT("Binding reconcile succeeds"), FNKMTextBindingReconciler::Reconcile(*Profile, *Provider, Document, ReconcileReport, ReconcileResult));
	TestEqual(TEXT("Two missing fields generated"), ReconcileReport.AddedKeys.Num(), 2);
	TestEqual(TEXT("Existing entry metadata repaired"), ReconcileReport.UpdatedMetadataKeys.Num(), 2);

	for (FNKMLocalizationEntry& Entry : Table.Entries)
	{
		if (Entry.SourceString.IsEmpty()) Entry.SourceString = TEXT("Pending native text filled");
	}
	FNKMTextBindingCoverageReport CoverageReport;
	FNKMLocalizationResult CoverageResult;
	TestTrue(TEXT("Complete generated bindings pass coverage"), FNKMTextBindingCoverageValidator::Validate(*Profile, *Provider, Document, CoverageReport, CoverageResult));
	TestEqual(TEXT("Four keys expected"), CoverageReport.ExpectedKeyCount, 4);
	TestTrue(TEXT("No keys missing"), CoverageReport.MissingKeys.IsEmpty());

	IFileManager::Get().Delete(*CsvPath, false, true);
	return !ProviderResult.HasErrors() && !ReconcileResult.HasErrors() && !CoverageResult.HasErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNKMTranslationStatusAnalyzerTest,
	"NKM.Localization.Translation.StatusAnalyzer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNKMTranslationStatusAnalyzerTest::RunTest(const FString& Parameters)
{
	const FString PO = TEXT(R"PO(msgid ""
msgstr "Project-Id-Version: NKMText\n"

msgctxt "A"
msgid "Hello {Name}"
msgstr "안녕 {Name}"

#, fuzzy
msgctxt "B"
msgid "Old"
msgstr "이전"

msgctxt "C"
msgid "Missing"
msgstr ""

msgctxt "D"
msgid "Count {Count}"
msgstr "개수 {Other}"
)PO");
	FNKMTranslationStatus Status;
	FString Error;
	TestTrue(TEXT("PO status parses"), FNKMTranslationStatusAnalyzer::AnalyzeString(PO, Status, Error));
	TestEqual(TEXT("One translated"), Status.Translated, 1);
	TestEqual(TEXT("One stale"), Status.Stale, 1);
	TestEqual(TEXT("One missing"), Status.Missing, 1);
	TestEqual(TEXT("One invalid"), Status.Invalid, 1);
	return true;
}

#endif
