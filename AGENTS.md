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
