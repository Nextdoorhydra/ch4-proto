# DataForge CSV persistent example

This folder is a reusable editor example for the complete DataForge workflow.

## Source files

- `Source/Items.csv` is the primary source keyed by `Id`.
- `Source/Prices.csv` is the foreign source keyed by `ItemId`.
- `Rules/RS_CsvItemExample` left-joins both CSV files into one canonical data set.

## Generated and resolved assets

- `Output/DT_CsvItems` is the generated DataTable.
- `Generated/{Category}/DA_{Id}` and `PDA_{Id}` are managed outputs.
- `Textures/{Category}/T_{TextureId}` is an external asset lookup rule.

The `{Category}`, `{Id}`, and `{TextureId}` values come directly from the joined
CSV row. Edit either CSV and apply `RS_CsvItemExample` again to update the saved
DataTable and managed assets.

## Rebuild the example

Run the Unreal automation test `DataForge.Examples.PersistentCsvAssets`. It
creates or updates the preserved RuleSet and texture assets, applies the RuleSet,
and verifies the join, generated DA/PDA files, and texture assignments.

## Rename Audit and recovery example

`RenameAudit/` is an independent example for the folder-based naming workflow.
Run `DataForge.Examples.PersistentRenameAuditAssets`, then right-click its
`Inventory` folder and choose **DataForge Audit Folder**. The detailed experiment
and expected results are documented in `RenameAudit/README.md`.

## Multiple Texture and Material references

`MultiAssetRefs/` keeps only the record `Id` in CSV. A Naming Policy, Layout
Recipe, Folder Source and Binding Preset infer every Texture/Material association
from `Inventory/{Id}/{Kind}` and convention-compliant asset names. Run
`DataForge.Examples.PersistentMultiAssetReferences` to rebuild and verify it.
