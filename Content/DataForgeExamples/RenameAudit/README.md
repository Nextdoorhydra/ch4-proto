# DataForge Rename Audit example

This preserved example demonstrates folder-based semantic matching, naming advice,
batch audit, explicit rename, and recovery.

## What the example contains

- `Source/Characters.csv`: the source of truth (`Hero`, `Villain`).
- `Definitions/NP_RenameAuditExample`: `T_<ProjectPrefix><Subject><Role>` naming policy.
- `Definitions/ALR_RenameAuditExample`: reads the subject and asset kind from folders.
- `Definitions/FSC_RenameAuditExample`: scans the `Inventory` folder recursively.
- `Definitions/BP_RenameAuditExample`: declares the `Portrait` texture slot.
- `Definitions/RS_RenameAuditExample`: connects CSV records to the folder inventory.
- `Inventory/Hero/Texture/Legacy_HeroPortrait`: intentionally needs a rename.
- `Inventory/Villain/Texture/T_CMVillainPortrait`: already complies with the rule.

The generated Hero destination is
`/Game/DataForgeExamples/RenameAudit/Inventory/Hero/Texture/T_CMHeroPortrait`.

## Create or validate the preserved assets

Run the Unreal automation test:

`DataForge.Examples.PersistentRenameAuditAssets`

The test creates missing example assets, preserves an already-renamed Hero asset,
and verifies that both records resolve to one deterministic candidate.

## Try the editor workflow

1. In Content Browser, right-click `RenameAudit/Inventory`.
2. Select **DataForge Audit Folder**.
3. Confirm that `Legacy_HeroPortrait` is recommended as `T_CMHeroPortrait`.
4. Confirm that `T_CMVillainPortrait` is shown as already compliant.
5. Select the Hero proposal and apply the rename.
6. Open **Tools > DataForge Recovery Center** to inspect the recovery manifest.
7. Restore the manifest to return Hero to `Legacy_HeroPortrait` and repeat the test.

No DataTable or generated DA/PDA is required for this audit-only example. The
RuleSet uses the CSV solely as the source of record identities and the folder
adapter as the source of candidate assets.
