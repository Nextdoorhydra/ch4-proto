# Chimera project agent instructions

## DataForge Google parser bootstrap

When a developer asks MCP to create a DataForge RuleSet from an existing Google parser or Google Sheet config, use the project bootstrap command below through the canonical MCP capability `system_control.console_command`. Do not synthesize a RuleSet by chaining generic asset-property edits.

```text
DataForge.MCP.CreateRuleSetFromGoogleParser Config=/Game/Path/GS_Config
```

- `Config` is required and must identify a `GoogleSheetConfig` asset.
- If the config's parser owns a compatible `TargetTable`, its Row Struct and DataTable path are reused automatically.
- Otherwise add both `RowStruct=/Script/Module.RowStruct` and `Output=/Game/Path/DT_Name`.
- Optional arguments are `RuleSet=/Game/Path/RS_Name`, `PrimaryKey=ColumnName`, and `Apply=true|false`.
- The command is initial-creation only. It never overwrites an existing RuleSet. Tell the developer to maintain an existing RuleSet in the DataForge editor.
- A successful run enables normalized JSON and DataForge auto-apply on the Google config, probes cached parsed data, infers schema and exact-name bindings, previews, applies by default, and saves the RuleSet/config.
- If Probe reports a missing cache, instruct the developer to Fetch the Google config once with `Save Normalized Json` enabled, then retry.
- After execution, verify that the returned RuleSet and DataTable object paths exist. Report detected column and binding counts from the `[DataForge MCP]` log line.

Detailed command examples and responsibility boundaries are in `Docs/DataForge_MCP_Guide.md`.

### Structured RuleSet requests

When the request includes Managed PDA/DA outputs, external asset lookup rules, or explicit bindings, convert it to the JSON contract in `Docs/DataForge_MCP_Guide.md`. Write the temporary spec under `Saved/DataForge/McpRequests`, then execute:

```text
DataForge.MCP.CreateRuleSetFromGoogleParser Spec=Saved/DataForge/McpRequests/<request>.json
```

- Resolve referenced config assets, Row Structs, DataAsset classes, and destination paths before writing the spec. Never guess an Unreal object/class path from a display name.
- Preserve the user's source column spelling in `{Column}` tokens and bindings; tokens are case-sensitive.
- Define Asset Rules before Generated Outputs. A Generated Output must select a `Managed` Asset Rule.
- Use `ResolvedAsset` plus an `External` Asset Rule for textures, meshes, and other project-owned assets.
- Add explicit bindings only for relationships that exact-name inference cannot produce. The bootstrap automatically adds exact-name source-to-row, source-to-generated-output, and generated-output-to-row bindings.
- Do not leave the JSON spec as a substitute for verification. After the command, verify the RuleSet, DataTable, and requested generated/external references and report the `[DataForge MCP]` result.

### Asset Layout Profile requests

Prefer an existing `UDataForgeAssetLayoutProfile` when the developer asks to reuse a project layout or naming convention. Keep `assetRules` available for manual/custom rules; the two forms may coexist.

```json
"assetLayoutProfile": {
  "path": "/Game/DataForge/Profiles/ALP_Character",
  "parameters": {
    "FeatureRoot": "Combat"
  }
}
```

- Use an exact `path` whenever the developer names or supplies a Profile. Never infer an Unreal path from its display name.
- If only a domain is known, use `purpose` instead of `path`, for example `"purpose":"Character"`. The command searches exact `Purpose` and `Tags` metadata.
- Do not choose among multiple discovery results. The command returns every candidate and fails; ask the developer to select one, then rerun with its exact path.
- If discovery returns no candidate, request manual Asset Rules or creation of a Profile. Do not invent project layout conventions.
- Profile parameters must be strings and use their declared names exactly. Profile `${Parameter}` tokens are resolved during bootstrap; source `{Column}` tokens remain case-sensitive and are validated against Probe.
- The bootstrap materializes Profile rules into the transient draft, runs Preview, and only then creates the RuleSet. The compiler continues to consume concrete RuleSet rules rather than the Profile asset.
