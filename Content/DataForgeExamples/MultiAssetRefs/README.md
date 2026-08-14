# DataForge ID-only folder inference example

This preserved example keeps only record identity in CSV. Texture and Material
asset ids are not duplicated in tabular data. DataForge infers every relationship
from the record id, folder layout, Unreal asset class, naming policy, and binding
preset slots.

## Source of truth

`Source/Products.csv` contains only:

```csv
Id,DisplayName
Armor,Armor Set
Robot,Robot Set
```

`Id` is both the DataTable primary key and the subject key used to match the
folder inventory.

## Inventory

```text
Inventory/
├─ Armor/
│  ├─ Texture/
│  │  ├─ T_CMArmorTexture_1
│  │  └─ T_CMArmorTexture_2
│  └─ Material/
│     ├─ M_CMArmorMaterial_1
│     └─ M_CMArmorMaterial_2
└─ Robot/
   ├─ Texture/
   │  ├─ T_CMRobotTexture_1
   │  └─ T_CMRobotTexture_2
   └─ Material/
      └─ M_CMRobotMaterial_1
```

The Folder Source emits canonical rows such as:

```text
Subject=Armor, AssetKind=Texture, Role=Texture, Numbering=1
Subject=Armor, AssetKind=Material, Role=Material, Numbering=2
```

## Definition chain

- `Definitions/NP_MultiAssetRefs`: recognizes `T_CM...` and `M_CM...` names.
- `Definitions/ALR_MultiAssetRefs`: reads Subject from folder segment 0 and Kind
  from segment 1.
- `Definitions/FSC_MultiAssetRefs`: scans `Inventory` recursively.
- `Definitions/BP_MultiAssetRefs`: declares the `Textures` and `Materials` Many
  slots on the generated PDA.
- `Rules/RS_MultiAssetRefs`: joins `Products.Id` to inventory `Subject` and
  generates the DataTable and PDAs.

Both slots use `Replace Managed`. The folder inventory is authoritative for these
arrays, so stale references from the previous CSV-driven example or manual edits
are replaced by the current inferred set.

## Outputs

- `Output/DT_MultiAssetRefs`: each row references its generated PDA.
- `Generated/PDA_Armor`: two inferred textures and two inferred materials.
- `Generated/PDA_Robot`: two inferred textures and one inferred material.

Adding another convention-compliant asset under `Inventory/Armor/Texture` changes
the folder inventory revision and the next reconcile appends it to `PDA_Armor`
without editing the CSV.

## Rebuild and verify

Run:

`DataForge.Examples.PersistentMultiAssetReferences`

The test creates missing definitions and inventory assets, materializes the
Binding Preset, applies the RuleSet, and verifies the inferred arrays on both
generated PDAs.
