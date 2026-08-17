# NKM Localization — Phase 3

`NKMLocalizationEditor`는 `ko` 원문 JSON/CSV를 검증하고 generated String Table Asset을 생성하거나 동기화한다.

## 입력 형식

다중 table 문서는 `Docs/NKMLocalizationPipelineDesign.md`의 schema를 사용한다. 간단한 key-value JSON도 지원한다.

```json
{
  "Shop.BuyAction": "구매",
  "Shop.SellAction": "판매"
}
```

CSV는 Unreal String Table과 동일한 필수 header를 사용한다. 나머지 column은 entry metadata가 된다.

```csv
Key,SourceString,Comment
Shop.BuyAction,구매,상점 구매 버튼
Shop.SellAction,판매,상점 판매 버튼
```

간단한 JSON과 CSV는 source 안에 table 정보가 없으므로 command line에서 `TableId`와 `AssetPath`를 지정해야 한다.

## 검증

```powershell
UnrealEditor-Cmd.exe NetKarma.uproject `
  -run=NKMLocalization `
  -Mode=Validate `
  -Input=Content/Localization/Source/MainMenu.json `
  -unattended -nop4 -NullRHI
```

## Asset 동기화

```powershell
UnrealEditor-Cmd.exe NetKarma.uproject `
  -run=NKMLocalization `
  -Mode=SyncAsset `
  -Input=Content/Localization/Source/MainMenu.json `
  -unattended -nop4 -NullRHI
```

structured JSON은 source 자체의 `id`와 `assetPath`를 사용하므로 두 table argument를 생략한다.

동기화 로그의 `+`, `~`, `-`는 각각 add, update, remove 수이다. 같은 source를 다시 실행했을 때 모두 0이어야 한다. validation error가 있으면 asset을 변경하지 않고 exit code 1을 반환한다.

## Runtime reference 주의사항

`NKM.UI`는 localization namespace 겸 project alias이다. Asset 기반 String Table의 실제 runtime table ID는 다음 object path 형식이다.

```text
/Game/NetKarma/Localization/StringTables/ST_NKM_UI.ST_NKM_UI
```

Runtime의 `FNKMTextRef`가 `NKM.UI::Shop.BuyAction`을 위 object-path ID로 resolve한 뒤 `FText::FromStringTable`을 호출한다. alias를 직접 `FText::FromStringTable`에 전달하면 안 된다.

기본 alias mapping은 deterministic하다. `NKM.Items`의 asset은 반드시 `ST_NKM_Items`여야 한다. 다른 asset 이름이 필요한 경우 **Project Settings > Game > NKM Localization > Table Alias Overrides**에 명시적으로 등록해야 하며, source validation이 alias와 `assetPath`의 불일치를 차단한다.

게임 시작 시 `UNKMLocalizationSubsystem`이 `/Game/NetKarma/Localization/StringTables`의 table을 비동기로 preload한다. `FNKMTextRef::Resolve()`는 find-only 정책이므로 `Are String Tables Ready` 이후 사용해야 하며 gameplay 중 동기 load fallback은 없다.

## 경로 설정

**Project Settings > Game > NKM Localization > Paths**에서 다음 값을 변경할 수 있다.

- `String Table Asset Root`: generated `.uasset` root. 기본값 `/Game/NetKarma/Localization/StringTables`
- `Authoring Source Root`: JSON source root. Unreal 표준 위치 `Content/Localization/Source`로 고정된다.
- `Localization Target Root`: manifest/archive/PO/LocRes root. Unreal 표준 위치 `Content/Localization/{LocalizationTargetName}`로 고정된다.

변경 후 Dashboard의 `Apply Project Settings`를 누른다. 이 작업은 runtime/Dashboard가 즉시 읽는 설정뿐 아니라 cook directory, `Game` target exclude, `NKMText` gather include와 target source/destination config도 함께 갱신한다. CI에서는 `-Mode=ApplyPaths`를 사용할 수 있다.

## Main Menu 예제

제공된 `Content/Localization/Source/MainMenu.json`은 `NKM.UI.MainMenu` table에 다음 key를 정의한다.

- `MainMenu.StartGame` → `게임 시작`
- `MainMenu.Options` → `옵션`
- `MainMenu.Quit` → `나가기`

Dashboard에서 파일을 Load한 뒤 `Preview Diff`와 `Save + Sync + Gather/Export`를 실행한다. 생성 asset은 `/Game/NetKarma/Localization/StringTables/ST_NKM_UI_MainMenu`이다. UI ViewModel 또는 DataTable에서는 `FNKMTextRef`에 `TableId=NKM.UI.MainMenu`, `Key=MainMenu.StartGame`처럼 저장하고 `Resolve NKM Text Ref`로 `FText`를 얻는다.

## Editor dashboard

Editor의 **Tools > NKM Localization**에서 다음 작업을 한 화면에서 수행한다.

- Source: JSON/CSV load, key/source/comment 편집, dirty-state 보호, validation, semantic diff preview, 삭제 확인, canonical JSON 저장, String Table sync 및 Gather/PO export
- Gameplay CSV audit: `DisplayName`, `Description`, `*Text`, `*TextKey`, `*TextRef` column을 자동 탐지하고 unknown key를 보고
- Translation: culture별 translated/stale/missing/invalid 수와 LocRes 상태 확인, Gather/Export, Import/Compile, Verify
- Preview: culture row의 Preview 버튼으로 실제 String Table sample을 확인하고 Restore Language로 복귀

CSV source는 grid에서 확인할 수 있지만 저장 authority는 structured JSON만 허용한다. CSV를 편집한 뒤 `Save JSON`을 누르면 원본 CSV를 덮어쓰지 않는다.

## Gameplay data와 localized text binding

현행 production 규칙은 다음과 같다.

- Google Sheet/CSV: stable ID, 수치, enum, 관계, asset path 등 gameplay data
- structured JSON: native source text와 영속적인 String Table identity
- Project Settings의 binding profile: `Stable ID + Field`를 `TableId + Key`로 결정하는 중앙 규칙

Item/Augment parser는 Google Sheet의 `Name`/`DisplayName`/`Description` column을 읽지 않는다. `Items`, `Augments` profile로 `FNKMTextRef`를 생성하며 기본 key는 각각 `Item.{Id}.{Field}`, `Augment.{Id}.{Field}`이다. 따라서 시트에 번역 key를 수동 입력하거나 별도 row-level mapping sheet를 유지하지 않는다.

**Project Settings > Game > NKM Localization > Bindings**에서 profile별 table, key pattern, field를 관리한다. ID는 gameplay row의 기존 stable ID를 그대로 사용하며 display text를 ID로 사용하면 안 된다.

각 profile은 record provider도 정의한다. 범용 CSV provider는 `Record Id Column`과 선택적인 `Record Source File`을 사용한다. Google Sheet처럼 CSV가 Unreal asset/parser로 이미 반영되는 프로젝트는 `Record Source Asset`, `Record Source Object Property`, `Record Map Property`, `Record Id Property`로 reflected `TMap` provider를 설정한다. Localization plugin은 Google Sheet 타입에 직접 의존하지 않는다.

신규 gameplay row를 추가한 뒤에는 Dashboard에서 authoritative JSON을 Load하고 `Binding Profile`을 선택한 다음 `Reconcile`을 실행한다. 필요하면 임의 CSV를 선택할 수 있고, 비워 두면 profile provider를 사용한다. Reconcile은 `Stable ID × Fields`로 key와 metadata를 생성하지만 기존 source를 덮어쓰거나 orphan을 삭제하지 않는다. 새 행의 `Native Source`를 입력하기 전에는 Save/Gather validation이 실패한다. `Coverage`는 gameplay record와 JSON의 누락, metadata 불일치, orphan을 검사한다.

CI에서는 다음 명령으로 CSV를 직접 대조할 수 있다.

```powershell
UnrealEditor-Cmd.exe Project.uproject `
  -run=NKMLocalization -Mode=ValidateCoverage `
  -BindingProfile=Items -Input=Data/Items.csv `
  -Source=Content/Project/Localization/Source/Items.json `
  -unattended -nop4 -NullRHI
```

`GatherExport`와 `Verify`는 provider가 구성된 모든 binding profile의 coverage를 자동 검증한다. Runtime에서는 full `TableId::Key`를 조립하지 않고 `Resolve Bound Text(BindingProfile, RecordId, FieldName)`을 사용한다.

기존 parser asset의 원문을 처음 이관할 때만 **Tools > NKM Localization > Migrate Bound Texts**를 실행한다. 이 작업은 profile의 reflection 정보로 `Items.json`/`Augments.json`을 생성하고 String Table을 동기화한다. 기존 JSON은 기본적으로 덮어쓰지 않는다. 이관 이후 source of truth는 JSON이며 Google Sheet를 다시 sync해도 text가 바뀌지 않는다.

CI의 1회 migration 명령은 다음과 같다.

```powershell
UnrealEditor-Cmd.exe NetKarma.uproject `
  -run=NKMLocalization -Mode=MigrateBindings `
  -unattended -nop4 -NullRHI
```

일반 DataTable에서 직접 localized reference를 받을 때는 raw `FString` 대신 `FNKMTextRef` property와 `TableId::Key`를 사용한다.

DataTable의 자동 property import에는 table context가 없으므로 반드시 `TableId::Key`를 사용한다. 별도 import profile이 default table을 제공하는 adapter와 audit command에서는 key-only cell도 허용한다. Blueprint에서는 `Resolve NKM Text Ref`로 현재 culture의 `FText`를 얻는다.

다중 table source의 key-only audit은 default table을 명시하지 않으면 실패한다. 빈 reference, 짧은 row, case-only duplicate header, reference가 하나도 없는 CSV 역시 실패한다.

```powershell
UnrealEditor-Cmd.exe NetKarma.uproject `
  -run=NKMLocalization -Mode=AuditCsv `
  -Input=Data/Items.csv `
  -Source=Content/Localization/Source/Items.json `
  '-DefaultTableId=NKM.Items' `
  -Report=Artifacts/NKMText-CsvAudit.json `
  -unattended -nop4 -NullRHI
```

## Unreal localization pipeline

원문 asset 동기화부터 PO export까지 한 번에 실행한다.

```powershell
UnrealEditor-Cmd.exe NetKarma.uproject `
  -run=NKMLocalization `
  -Mode=SyncSource `
  -Input=Content/Localization/Source/MainMenu.json `
  -Report=Artifacts/NKMText-SyncSource.json `
  -unattended -nop4 -NullRHI
```

번역자가 culture별 `Content/Localization/NKMText/{culture}/NKMText.po`를 편집한 뒤에는 Import와 Compile을 실행한다.

```powershell
UnrealEditor-Cmd.exe NetKarma.uproject `
  -run=NKMLocalization `
  -Mode=ImportCompile `
  -Report=Artifacts/NKMText-ImportCompile.json `
  -unattended -nop4 -NullRHI
```

`GatherExport`는 마지막 성공한 Import/Export 이후 PO가 바뀌었으면 `NKMLOC-PO-UNIMPORTED`와 exit code 1로 중단한다. 먼저 `ImportCompile`을 실행해야 번역이 보존된다. `-ForcePOOverwrite`는 PO 편집을 의도적으로 버릴 때만 사용하는 비상 옵션이다.

CI에서는 다음 검증을 사용한다. `-Report`를 생략하면 `Saved/NKMLocalization/NKMTextReport.json`에 JSON 결과가 기록된다.

```powershell
UnrealEditor-Cmd.exe NetKarma.uproject `
  -run=NKMLocalization `
  -Mode=Verify `
  -Report=Artifacts/NKMText-Verify.json `
  -unattended -nop4 -NullRHI
```

`Verify`는 non-empty manifest, 빈 conflict report, 모든 culture의 archive/PO/LocRes, locmeta, PO ledger 일치를 검사한다.

## Translation Editor

`NKMText`는 Unreal 표준 `Content/Localization/NKMText` target을 공유한다. 따라서 **Tools > Localization Dashboard**와 **NKM Localization > Translation**은 동일한 manifest, archive, PO, LocRes를 사용한다. 표준 Dashboard는 target/culture 관리와 번역 편집에, NKM Dashboard는 JSON String Table 생성·binding coverage 검증과 culture별 전체 건수/진행률 확인에 사용한다.

culture 행의 **Preview**는 Editor UI language를 변경하지 않고 UE의 Game Localization Preview만 설정한다. PIE의 게임 옵션 언어 선택도 같은 preview API를 사용하며, packaged game에서는 실제 current culture를 저장한다.

## 지원 언어 관리

**Project Settings > Game > NKM Localization > Cultures**가 native/support culture의 단일 설정 지점이다. `Native Culture`는 `Supported Cultures`의 첫 항목이어야 한다. 변경 후 **Apply Project Settings**를 실행하면 네 개의 `NKMText_*.ini`, NKMText dashboard target, packaging의 `CulturesToStage`와 ICU preset이 함께 갱신된다. C++ 수정은 필요 없다. 새 PO를 번역한 뒤 `ImportCompile`과 `Verify`를 실행한다.

community mod가 새 culture LocRes를 제공하려면 해당 culture가 packaged build의 `Supported Cultures`에 포함되어 ICU data와 함께 stage되어야 한다. 임의 culture를 사후 추가하려면 base build도 그 culture를 지원하도록 다시 패키징해야 한다.

## 다른 Unreal 프로젝트에서 재사용

**Project Settings > Game > NKM Localization > Identity**의 `Localization Target Name`, `Table Id Prefix`, `String Table Asset Prefix`가 target/artifact/table naming의 기준이다. pipeline, validator, Translation Editor, PO ledger는 이 값을 사용하며 `NKMText`, `NKM.*`, `ST_NKM_*`를 요구하지 않는다. target 이름을 변경하고 `Apply Project Settings`를 실행하면 기존 applied target config를 새 이름으로 복사하여 project config를 갱신한다.
