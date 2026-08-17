# DataForge

DataForge is an editor-only Unreal Engine plugin that turns canonical parsed rows and declarative binding rules into reviewed DataTable, DataAsset, and PrimaryDataAsset updates. Parsers and providers are isolated behind registered Source Adapters, so Unreal materialization does not depend on whether rows came from raw CSV, JSON, an existing parser, or another service.

## Supported vertical slice

- Capability-id Source Adapter registry with a canonical `FDataForgeDataSet` boundary
- CSV files and flat/normalized JSON files (absolute paths or paths relative to the project directory)
- Google Sheet normalized caches through a selectable `GoogleSheetConfig` Source Asset
- Foreign-key left joins across CSV, JSON, and Google Sheet Cache inputs through one `Multi Source` RuleSet
- Project adapters that supply already-parsed rows without changing the compiler
- Explicit primary keys and required columns
- Reflection bindings to scalar, text, name, enum, bool, array, object, and soft-object row properties through Unreal's property importer
- Nested struct property paths such as `Stats.Attack`
- Exact project asset resolution through tokenized Asset Rules
- Per-record DataAsset and PrimaryDataAsset generation through Managed Asset Rules
- Bindings from canonical source values/external assets into generated asset properties
- Generated output references bound back into DataTable soft-object properties
- Field-level ownership: only declared generated-asset properties are overwritten
- Persistent ownership metadata and managed orphan detection across managed folders
- Ownership-identity-based managed asset move/rename when a rule changes the desired package path
- Dedicated RuleSet Editor with side-by-side rule details, source rows, and managed path plan
- Staged nine-step Rule Creation Wizard: Source, Probe, Schema, Output, Asset Layout, Asset Rules, Generated Outputs, Bindings, Preview
- One-click Automatic Setup for primary-key, folder-layout, reflected slot, association, and exact-name binding inference
- Exact-root reuse of existing Folder Source Config, Layout Recipe, and Naming Policy definitions
- Structured Automatic Setup review with evidence, create/reuse paths, assignment cardinality, diagnostics, and Preview effects
- Explicit developer approval of an Automatic Setup Preview before Finish & Apply
- Reflection Property Picker for DataTable and generated-output targets
- Exact-name Auto Map for editable DataTable row properties
- Automatic exact-name inference when the Wizard enters Bindings, including same-name Generated Output soft references
- Generated Output dropdowns for binding source/target selection
- Output deletion detection that reopens the owning RuleSet's Creation Wizard
- Probe-driven conversion suggestions for text, integer, number, bool, enum, array, and object-reference targets
- Drag-and-drop Binding Graph with safe connection gating
- Semantic RuleSet Diff keyed by RuleId, OutputName, and binding target
- Explicit cross-RuleSet dependencies with deterministic dependency-first execution order and cycle detection
- Project Overview tab showing every RuleSet, source/provider, output path, and dependency alongside the current execution order
- Mutation-free Probe and Preview
- Create/move/update/unchanged/orphan diff for DataTable rows and managed assets
- Apply gated by the last successful Preview
- Source, rule, and target drift checks before Apply
- Explicit, separately confirmed orphan cleanup that never deletes External or unowned assets
- Recovery manifest containing the previous DataTable JSON, move paths, and old/new managed property values
- Deterministic JSON/YAML RuleSet snapshots for source-control review
- Headless single-RuleSet or project-wide validation/Preview/Apply/Cleanup/snapshot commandlet support
- CI provenance logging for plugin version, RuleSet version, source revision, and materialization summary
- Naming Policy, folder Layout Recipe, and Binding Preset assets for reusable semantic asset conventions
- Asset Registry Folder associations with explicit cardinality and managed merge behavior
- Evidence-ranked Rename Audit for selected assets or recursive Content Browser folders
- Collision-safe batch rename and Recovery Center restoration backed by persisted manifests

Managed asset identity is `(RuleSetId, RecordId, Role)`. When that identity still exists but its Asset Rule produces a different path, Preview reports a Move and Apply renames the owned asset. Apply always preserves true orphans. Deletion is available only through the separately confirmed **Cleanup Root Orphans** action or the `-CleanupOrphans` commandlet mode; both re-preview/revalidate ownership and drift before deleting. External and unowned assets are never moved or deleted.

## Editor workflow

1. In the Content Browser, create **Misc > DataForge RuleSet**.
2. Double-click it to open the dedicated **DataForge RuleSet Editor**.
3. Select **Creation Wizard**. The wizard edits a transient draft, not the RuleSet asset.
4. Choose `CSV`, `JSON`, or `Google Sheet Cache` from the adapter dropdown. Use the file browser or Source Asset picker; do not type an Adapter ID.
5. For the recommended path, set Asset Search Root, Row Struct, generated PDA/DA class, DataTable path, generated folder, and definition folder under **Automatic Setup**, then select **Analyze & Build Draft**. Naming Policy is optional when one compatible definition can be discovered.
6. Complete Source → Probe → Schema → Output → Asset Layout → Asset Rules → Generated Outputs → Bindings → Preview. Advanced steps remain editable after automatic analysis.
7. Run **Preview**, inspect inference evidence, create/reuse paths, assignment relationships, diagnostics, and concrete row/asset effects, then check the explicit review approval.
8. Select **Finish & Apply**. The Wizard promotes reviewed definitions, commits the staged rules, runs a fresh Preview, and immediately Applies when validation succeeds.
9. Use the **Pick** menu beside `TargetProperty` to choose writable properties through reflection instead of typing paths. Choose `Source Output` and `Target Output` from the Generated Outputs dropdown.
10. Review the adjacent **Conversion** result. `Risky` and `Unsupported` results require an explicit rule correction.
11. Open **Binding Graph**, run **Probe + Refresh**, then drag source fields/outputs onto target properties. The graph creates only `Direct` or `Convertible` bindings.
12. For existing project assets, add an External Asset Rule and use a `ResolvedAsset` binding. For DA/PDA generation, add a Managed Asset Rule and Generated Output.
13. Open **Semantic Diff** to review structured changes from its captured baseline. Reordering keyed rule arrays alone does not produce noise.
14. Add prerequisite RuleSets to `Dependencies`, then open **Project Overview** to review dependency-first execution order and every project RuleSet's source/output/dependent files. Empty, missing, or cyclic dependencies fail compilation.
15. Select **Preview Dependency Graph** and then **Apply Dependency Graph**. Each RuleSet is re-previewed immediately before its dependency-ordered Apply; source, rule, dependency, DataTable, or managed-asset drift blocks that RuleSet.
16. If Preview reports managed orphans, review them and use **Cleanup Root Orphans** as a separate destructive action. Apply does not remove them.

Auto Map adds only missing exact-name targets. It maps source columns to editable row properties and defined Generated Outputs to same-name soft-object row properties; it never overwrites an existing target binding.

A successful Probe immediately reveals a dedicated Primary Key dropdown and it remains visible through Schema; Automatic Setup also reveals it on Source after its internal Probe. Only the latest Probe columns are listed. The column does not need to be named `Id`; a selected `CharacterCode`, `BodyKey`, or other unique source column supplies the subject value represented as `<ID>` in Naming Policy examples. Manual Asset Rule tokens remain literal and case-sensitive, so that example would use `{CharacterCode}`. Asset names do not synthesize missing source columns.

Automatic Setup never guesses among tied reusable definitions. An exact Asset Search Root match reuses the existing `Folder Source Config → Asset Layout Recipe → Naming Policy` chain. Multiple exact matches or multiple policy candidates block analysis and require an explicit Naming Policy override. Any draft edit or repeated Preview invalidates the previous approval.

Reopening Automatic Setup restores the committed RuleSet's asset-search root, generated output folder/class, definition folder, and Naming Policy where they can be recovered from its Generated Output, Binding Preset, and Folder Source chain. The reusable Chimera default policy is `/Game/Chimera/DataForge/Naming/NP_ChimeraDefault`; it covers common Blueprint, UI, texture, material, mesh, animation, Niagara, audio, table, data-asset, sequence, input, Gameplay Ability/Effect, enum, struct, and font prefixes.

The Source UI shows only the field consumed by the selected adapter: CSV/JSON use `File`, Google Sheet Cache uses `Source Asset`, and Multi Source uses `Inputs`. Deleting a generated DataTable or DataForge-owned DA/PDA in the Content Browser reopens the owning RuleSet's Creation Wizard on the next editor tick so the output can be reviewed and recreated.

DataForge diagnostics are recorded in the **DataForge Message Log** and the active tool's status/review area without automatically opening or focusing the Message Log window. A compatible existing PDA/DA at a generated path is adopted by default only when it has no DataForge ownership metadata; Preview reports `DF1219`, Apply updates only declared bindings, and assets owned by another RuleSet remain blocked. Disable **Adopt Compatible Unowned Asset** in a Generated Output's Advanced properties when strict create-only ownership is required.

## Folder inventory, rename audit, and recovery

Folder-driven association uses a **Naming Policy**, **Asset Layout Recipe**, **Folder Source Config**, and **Binding Preset**. Add the Folder Source Config as an `Asset Registry Folder` Association Source, then reference its Source ID from Binding Preset slots. Folder inventory rows remain canonical `FDataForgeDataSet` rows, so CSV, JSON, Google cache, and folder associations use the same compiler boundary.

For convention repair, select one or more assets and choose **DataForge Rename Audit...**, or right-click a folder and choose **DataForge Audit Folder...**. DataForge ranks candidates by existing Association Manifest, exact current name, Subject folder, and Naming Policy parsing. Only a unique highest score is marked `Recommended`; an evidence-free or tied candidate is never auto-selected.

Checked candidates are revalidated as one batch. DataForge blocks duplicate source selections, duplicate destinations, stale RuleSets/source rows, and occupied packages. Before AssetTools runs, it writes `Saved/DataForge/Recovery/Rename_*.json`. Open **Tools > DataForge Recovery Center...** to inspect persisted batches and restore a collision-free successful or incomplete-rollback batch. Restore writes its own `Restore_*.json` before moving assets and attempts to roll back partial restore failures.

## Google Sheet cache workflow

1. In the `GoogleSheetConfig`, enable **Save Normalized Json**. `DataParser` and its DataTable are optional; leave the parser empty for cache-only DataForge use. Existing parser workflows can enable **Skip Data Parser** explicitly.
2. Run the config's existing **Fetch** action. This refreshes `Saved/GoogleSheetLoader/<config-path>.json`.
3. In the DataForge RuleSet choose **Google Sheet Cache** and select that config in **Source Asset**.
4. Run Probe/Preview/Apply. The adapter re-reads the cache each time and hashes its full content; a cache change after Preview is detected as source drift and blocks Apply until Preview is rerun.

Fetch remains the explicit network trigger. When automatic application is enabled, DataForge still performs a fresh validation Preview internally before it mutates project assets.

As of 1.4, new GoogleSheetConfig assets enable **Save Normalized Json** and **Auto Apply DataForge** by default. A successful Fetch broadcasts the cache update, discovers every direct or Multi Source RuleSet referencing that config, runs a fresh Preview, and immediately Applies when validation succeeds. Preview/foreign-key/schema failures block only the affected RuleSet and are appended to the Google Sheet status. Existing config assets keep their serialized option values, so enable both options once when upgrading an existing asset.

For MCP-assisted initial creation, call the project command through the canonical `system_control.console_command` capability:

```text
DataForge.MCP.CreateRuleSetFromGoogleParser Config=/Game/Data/GS_Items
```

The command reuses the parser's TargetTable when available, probes the normalized cache, infers schema and bindings, previews, applies, saves, and enables Google auto-apply. It refuses to overwrite an existing RuleSet; subsequent maintenance belongs in the RuleSet Editor. Cache-only parsers must also provide `RowStruct=` and `Output=`. See `Docs/DataForge_MCP_Guide.md` and the project `AGENTS.md` for the MCP contract.

Requests that also define Managed PDA/DA outputs, external texture/mesh lookups, or explicit bindings use a validated JSON spec instead of fragile repeated console arguments:

```text
DataForge.MCP.CreateRuleSetFromGoogleParser Spec=Saved/DataForge/McpRequests/items.json
```

The same atomic bootstrap parses the complete spec, probes the source, adds exact-name bindings, runs Preview, and only then creates/applies the RuleSet. The Korean copy-ready prompt template and JSON schema are in `Docs/DataForge_MCP_Guide.md`.

Structured specs may select an Asset Layout Profile by exact asset path or by an exact `Purpose`/`Tags` discovery key. Discovery succeeds only for one candidate; ambiguous results are reported instead of guessed. Profile `${Parameter}` values materialize into concrete RuleSet Asset Rules after Probe validates every remaining `{Column}` token. Existing `assetRules` remain compatible and become Custom rules when combined with a Profile.

## Multi Source foreign-key join

Choose **Multi Source (Join)** and add two or more Inputs. Input 1 is the primary row set. Its `Join Column` is the primary key for the join; every later input's `Join Column` is the foreign-key field matched to it. Each input independently selects CSV, JSON, or Google Sheet Cache and its file/config asset.

```text
Input 1 CSV:   Id, Name              Join Column: Id
Input 2 JSON:  ItemId, Price         Join Column: ItemId
Input 3 Sheet: Code, Category        Join Column: Code

Result: Id, Name, Price, Category
```

The join is a left join: only Input 1 creates DataTable rows. Duplicate/empty keys and conflicting non-key values fail Preview. Missing foreign rows and unused foreign keys produce warnings. Set `Column Prefix` on a secondary input when two sources intentionally use the same non-key column name. All input revisions are combined, so a change in any source invalidates the previous Preview.

For many JSON files, keep one RuleSet per independently owned output. When files share one schema and should feed one DataTable, register a project folder/aggregate adapter that merges them into one `FDataForgeDataSet`; then only one RuleSet is needed. When each file must produce a separate DataTable, separate RuleSets preserve ownership, review, and drift tracking, while the inferred-schema Wizard minimizes per-file setup.

## Source adapter contract

`UDataForgeRuleSet::Source.AdapterId` is resolved through `FDataForgeSourceAdapterRegistry`. The compiler only consumes `FDataForgeDataSet`; it does not instantiate CSV, JSON, or project parser types.

```cpp
class FProjectParsedDataAdapter final : public IDataForgeSourceAdapter
{
public:
    virtual FDataForgeSourceDescriptor Describe() const override;
    virtual bool Probe(const FDataForgeSourceConfig&, FDataForgeDataSet&, TArray<FDataForgeDiagnostic>&) const override;
    virtual bool Fetch(const FDataForgeSourceConfig&, FDataForgeDataSet&, TArray<FDataForgeDiagnostic>&) const override;
};

void FProjectEditorModule::StartupModule()
{
    FDataForgeSourceAdapterRegistry::Get().Register(MakeShared<FProjectParsedDataAdapter>());
}
```

An adapter may parse its own raw file, call an existing parser and translate its parsed rows, read a service, or expose cached parsed data selected through `SourceAsset`. It must return stable columns, rows, source-row locations, and a deterministic `SourceRevision`. Register/unregister adapters during the owning editor module's startup/shutdown.

Example external icon rule:

```text
CSV columns: ItemId, Category, IconId

Asset Rule
  RuleId: Icon
  Ownership: External
  BaseFolder: /Game/Art/Items
  SubfolderPattern: {Category}/{ItemId}/Textures
  AssetNamePattern: T_{IconId}_Icon

Binding
  Source: ResolvedAsset
  SourceColumn: IconId
  AssetRuleId: Icon
  TargetProperty: Icon
```

The rule resolves `/Game/Art/Items/{Category}/{ItemId}/Textures/T_{IconId}_Icon` exactly through the Asset Registry. A missing asset is an error; DataForge never chooses an arbitrary search result.

Example generated PrimaryDataAsset:

```text
Managed Asset Rule
  RuleId: ItemData
  Ownership: Managed
  BaseFolder: /Game/GameData/Items
  SubfolderPattern: {Category}
  AssetNamePattern: PDA_Item_{ItemId}

Generated Output
  OutputName: data
  Type: PrimaryDataAsset
  AssetClass: UItemDataAsset
  AssetRuleId: ItemData

Binding 1
  Source: ResolvedAsset
  SourceColumn: IconId
  AssetRuleId: Icon
  Target: GeneratedOutput
  TargetOutput: data
  TargetProperty: Icon

Binding 2
  Source: GeneratedOutput
  SourceOutput: data
  Target: DataTableRow
  TargetProperty: DataAsset
```

`Binding 2` requires `DataAsset` to be a soft-object property. This preserves mutation-free Preview even when the generated asset does not exist yet.

Generated packages store these metadata values:

```text
DataForge.Managed = true
DataForge.RuleSetId
DataForge.RecordId
DataForge.Role
DataForge.RuleVersion
```

An existing asset without matching ownership metadata is treated as a collision and is never overwritten.

For the existing `FCMBodyTableRow` flow, use `RowName` as the primary key, map `RowName`, `ID`, and `BodyType` to the same-named row properties, and target `/Game/Data/Body/DT_BodyDataTable`. You can either replace `UBodyDataParser` with the built-in CSV/JSON adapter or retain it behind a project adapter that translates its parsed rows into `FDataForgeDataSet`.

## Commandlet

Preview/validate only:

```text
UnrealEditor-Cmd.exe Chimera.uproject -run=DataForge -RuleSet=/Game/DataForge/Rules/RS_ItemCatalog -ValidateOnly
```

Validate every project RuleSet in dependency order:

```text
UnrealEditor-Cmd.exe Chimera.uproject -run=DataForge -All -ValidateOnly
```

Fail CI when validation succeeds but the desired state differs from current content:

```text
UnrealEditor-Cmd.exe Chimera.uproject -run=DataForge -All -ValidateOnly -FailOnChanges
```

Validate every Asset Layout Profile and fail CI when any Profile-backed RuleSet is outdated:

```text
UnrealEditor-Cmd.exe Chimera.uproject -run=DataForge -All -ValidateOnly -FailOnOutdatedProfiles
```

Profile definition errors, duplicate Profile IDs, and missing/mismatched provenance always fail validation. Profile drift is reported as `DF1643` but only becomes a CI failure when `-FailOnOutdatedProfiles` is specified.

Preview a dependency-ordered rebase for every RuleSet using one Profile, without mutation:

```text
UnrealEditor-Cmd.exe Chimera.uproject -run=DataForge -Profile=/Game/DataForge/Profiles/ALP_Character -Rebase -ValidateOnly
```

Explicitly persist the rebased concrete rules and apply their content:

```text
UnrealEditor-Cmd.exe Chimera.uproject -run=DataForge -Profile=/Game/DataForge/Profiles/ALP_Character -Rebase -Apply
```

Profile Rebase uses the same batch stale checks, global path-collision validation, and dependency ordering as Project Overview. It cannot be combined with `-RuleSet`, `-All`, snapshot, cleanup, or CI failure modes.

Controlled apply (internally previews first):

```text
UnrealEditor-Cmd.exe Chimera.uproject -run=DataForge -RuleSet=/Game/DataForge/Rules/RS_ItemCatalog -Apply
```

Explicit managed-orphan cleanup (internally previews each RuleSet first):

```text
UnrealEditor-Cmd.exe Chimera.uproject -run=DataForge -RuleSet=/Game/DataForge/Rules/RS_ItemCatalog -CleanupOrphans
```

Export deterministic JSON and YAML snapshots after validation:

```text
UnrealEditor-Cmd.exe Chimera.uproject -run=DataForge -All -ExportSnapshots
```

Fail CI when either snapshot is missing or stale:

```text
UnrealEditor-Cmd.exe Chimera.uproject -run=DataForge -All -VerifySnapshots
```

`-RuleSet` includes its transitive prerequisites; `-All` discovers every RuleSet asset. Dependencies and independent roots are ordered deterministically by asset path. `-Apply`, `-CleanupOrphans`, `-ExportSnapshots`, and `-VerifySnapshots` are mutually exclusive, and none can be combined with `-FailOnChanges` or `-FailOnOutdatedProfiles`. `-FailOnChanges` returns exit code `3`, `-FailOnOutdatedProfiles` returns `4`, validation/action errors return `1`, and invalid arguments return `2`.

Graph Apply is dependency ordered but is not a project-wide atomic transaction. Each RuleSet is previewed and applied independently, so a later failure does not roll back an earlier successful prerequisite.

Recovery manifests are written under `Saved/DataForge/Recovery` before content mutation, rename/restore, or orphan deletion. Rename manifests can be restored through Recovery Center when every recorded path is unambiguous and collision-free. DataTable/property and deletion manifests remain audit/reconstruction data rather than binary backups; keep deleted assets in source control or another backup. Unreal's AssetTools rename operation saves moved packages even when `Output.bSaveAfterApply` is false.

## RuleSet snapshots

The RuleSet asset remains the only authoring source. **Export Snapshots** writes review-only projections to:

```text
Config/DataForge/Snapshots/<RuleSet package path>.json
Config/DataForge/Snapshots/<RuleSet package path>.yaml
```

Both formats include every persistent RuleSet field and exclude transient editor status. Snapshot version 3 includes the Profile asset path, stable Profile ID, materialized version/hash, parameter values, rule-template baselines, and local override fields. Parameters, required columns, Profile origins, Asset Rules, generated outputs, bindings, and dependencies are sorted by stable semantic keys, so array reorder noise does not affect review. Commit these files alongside the RuleSet assets and run `-VerifySnapshots` in CI. Edit the UAsset through the RuleSet Editor and export again; snapshots are deliberately not imported as a competing source of truth.

## Authoring review tools

Conversion suggestions are advisory and never rewrite source values or select a coercion automatically. The Binding Graph refuses `Risky`, `Unsupported`, duplicate-target, stale-source, and invalid generated-output connections. Semantic Diff compares configuration rather than UObject serialization order, includes dependencies keyed by RuleSet path, and can capture a new baseline from the current RuleSet.

## Remaining architecture work

The planned naming, folder association, rename continuity, candidate audit, and rename restoration phases are complete. Future work is optional optimization: incremental Asset Registry indexing for very large projects and source-control-provider integration for richer binary recovery.
