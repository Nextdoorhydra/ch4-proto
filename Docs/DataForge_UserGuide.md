# DataForge 에디터 사용법

대상: 콘텐츠 제작자, 테크니컬 디자이너, Unreal 개발자

예제 위치: `/Game/DataForgeExamples`

## 1. 가장 빠른 시작

프로젝트에 보존된 예제를 먼저 확인한다.

1. Content Browser에서 `DataForgeExamples/Rules/RS_CsvItemExample`을 연다.
2. Source의 Adapter가 `Multi Source (Join)`인지 확인한다.
3. `Inputs[0]`은 `Items.csv`, `Join Column`은 `Id`다.
4. `Inputs[1]`은 `Prices.csv`, `Join Column`은 `ItemId`다.
5. Preview를 실행하고 오류가 없는지 확인한다.
6. Apply를 실행한다.
7. `DataForgeExamples/Output/DT_CsvItems`에서 두 행을 확인한다.
8. `Generated/Weapons/DA_1001`을 열어 `T_Sword`가 Icon에 할당됐는지 확인한다.

예제 원본:

- `Content/DataForgeExamples/Source/Items.csv`
- `Content/DataForgeExamples/Source/Prices.csv`

예제 전체를 다시 만들고 검증하려면 Session Frontend의 Automation에서 `DataForge.Examples.PersistentCsvAssets`를 실행한다.

## 2. 간단한 CSV → DataTable RuleSet

### RuleSet 생성

1. Content Browser에서 우클릭한다.
2. `Miscellaneous > DataForge RuleSet`을 선택한다.
3. 생성된 에셋을 더블클릭한다.
4. 처음 설정할 때는 `Creation Wizard`를 연다.

### Source 단계

1. Adapter에서 `CSV`를 고른다.
2. File 오른쪽의 `...` 버튼으로 CSV를 선택한다.
3. `Parameters`는 비워 둔다. 기본 CSV 어댑터는 사용하지 않는다.

선택한 Adapter에 필요한 입력만 표시된다. CSV/JSON은 `File`만, Google Sheet Cache는 `Source Asset`만 표시하며 Multi Source는 `Inputs`만 표시한다.

프로젝트 상대 경로와 절대 경로를 모두 지원한다. 팀 공유에는 다음과 같은 프로젝트 상대 경로를 권장한다.

```text
Content/DataForgeExamples/Source/Items.csv
```

### Probe 단계

1. Probe를 실행한다.
2. 감지된 컬럼과 샘플 행을 확인한다.
3. 첫 행이 header인지 확인한다.

### Schema 단계

1. `Primary Key`에 행을 유일하게 식별하는 컬럼을 선택한다.
2. 자동 추출된 `Required Columns`를 검토한다.

Primary Key 값은 DataTable RowName이 된다. 빈 값과 중복 값은 허용되지 않는다.

### Output 단계

1. Row Struct를 선택한다.
2. Output Asset Path의 Content Browser 선택 기능으로 출력 위치를 지정한다.
3. 일반적인 사용에서는 `Create If Missing`과 `Save After Apply`를 활성화한다.

### Bindings 단계

Source column과 RowStruct property를 연결한다.

```text
SourceValue DisplayName → DataTableRow.DisplayName
SourceValue Price       → DataTableRow.Price
```

Bindings 단계에 진입하면 source column과 이름이 같은 editable RowStruct property가 자동으로 추가된다. 기존 수동 바인딩은 유지하고 누락된 대상만 추가하므로 Back/Next 또는 Auto Map을 반복해도 중복되지 않는다.

Generated Output이 설정되어 있고 그 Output Name과 같은 이름의 DataTable soft-object property가 있으면 해당 참조 바인딩도 자동으로 추가된다. Binding의 `Source Output`과 `Target Output`은 현재 `Generated Outputs` 목록에서 드롭다운으로 선택할 수 있다. 복잡한 대상은 Pick 또는 Binding Graph를 사용한다.

### Preview와 Finish

1. Wizard의 Preview 단계에서 오류와 변경 수를 확인한다.
2. `Finish & Apply`로 RuleSet 설정을 저장한다.
3. Wizard가 새 Preview를 실행하고 성공하면 즉시 Apply하여 DataTable과 Generated Output을 생성하거나 갱신한다.

Preview 또는 Apply가 실패하면 Wizard가 열린 상태로 오류를 표시한다. DataTable 또는 DataForge가 소유한 PDA/DA를 Content Browser에서 삭제하면 해당 RuleSet의 Creation Wizard가 다시 열리므로 출력 경로와 규칙을 확인한 뒤 `Finish & Apply`로 재생성할 수 있다.

## 3. 여러 소스 외래키 병합

Adapter에서 `Multi Source (Join)`을 선택하고 Inputs 배열에 소스를 추가한다.

### 기본 규칙

- `Inputs[0]`: DataTable 행을 결정하는 primary source
- `Inputs[1..N]`: primary source에 붙는 secondary source
- 병합 방식: left join
- 각 입력은 CSV, JSON, Google Sheet Cache 중 하나를 선택 가능

예제:

```text
Inputs[0]
  Adapter: CSV
  File: Items.csv
  Join Column: Id

Inputs[1]
  Adapter: CSV
  File: Prices.csv
  Join Column: ItemId

Inputs[2]
  Adapter: JSON
  File: Balance.json
  Join Column: Code
  Column Prefix: Balance_
```

입력은 2개로 제한되지 않는다. `+` 버튼으로 여러 secondary source를 추가할 수 있다. 단, 모든 secondary source는 `Inputs[0]`의 키에 직접 연결된다.

### Column Prefix

secondary source에서 가져온 비키 컬럼의 이름 앞에 붙는 문자열이다.

```text
원본 컬럼: Attack
Column Prefix: Balance_
최종 컬럼: Balance_Attack
```

두 소스에 `Category`, `Name`처럼 같은 컬럼이 있고 의미나 값이 다를 때 사용한다. 같은 이름의 컬럼 값이 다르며 prefix도 없으면 Preview가 충돌 오류를 낸다. `Inputs[0]`에는 prefix가 적용되지 않는다.

### Parameters

어댑터 전용 확장 옵션을 위한 key/value map이다. 현재 CSV, JSON, Google Sheet Cache에는 필수 parameter가 없으므로 비워 둔다. 프로젝트 전용 어댑터가 delimiter, root path 등의 옵션을 정의할 때 사용한다.

## 4. 외부 Texture 자동 할당

외부 에셋은 `External` Asset Rule로 정확한 package path를 계산한다.

CSV:

```csv
Id,TextureId,Category
1001,Sword,Weapons
```

Asset Rule:

```text
Rule Id: Texture
Ownership: External
Base Folder: /Game/DataForgeExamples/Textures
Subfolder Pattern: {Category}
Asset Name Pattern: T_{TextureId}
```

계산되는 에셋:

```text
/Game/DataForgeExamples/Textures/Weapons/T_Sword
```

Generated Output의 Texture property에 다음 binding을 추가한다.

```text
Source: ResolvedAsset
Source Column: TextureId
Asset Rule Id: Texture
Target: GeneratedOutput
Target Output: da
Target Property: Icon
```

`{}` 안에는 source의 정확한 컬럼 이름을 넣는다. 대소문자가 다르거나 컬럼이 없으면 토큰을 해석하지 못해 Preview가 실패한다.

## 5. DA와 PDA 자동 생성

먼저 `Managed` Asset Rule을 만든다.

```text
Rule Id: DataAsset
Ownership: Managed
Base Folder: /Game/GameData/Items
Subfolder Pattern: {Category}
Asset Name Pattern: DA_{Id}
```

Generated Output을 추가한다.

```text
Output Name: da
Type: DataAsset
Asset Class: 원하는 UDataAsset 파생 클래스
Asset Rule Id: DataAsset
```

PDA도 동일하게 추가하되 Type을 `PrimaryDataAsset`으로 선택한다.

생성 에셋의 property에는 `Target = GeneratedOutput` binding을 사용한다. 생성된 DA/PDA를 DataTable row에 연결하려면 `Source = GeneratedOutput`을 사용한다.

```text
GeneratedOutput da → DataTableRow.DataAsset
```

Preview를 안전하게 유지하기 위해 DataTable의 생성 에셋 참조 property는 soft object reference를 권장한다.

## 6. Google Sheet 사용

### GoogleSheetConfig

1. Google Sheet URL과 Range를 입력한다.
2. `Save Normalized Json`을 활성화한다.
3. 즉시 RuleSet까지 갱신하려면 `Auto Apply DataForge`를 활성화한다.
4. 기존 DataParser를 사용하지 않을 경우 DataParser를 비워 두거나 `Skip Data Parser`를 활성화한다.
5. Fetch를 실행한다.

Fetch 성공 시 normalized JSON이 `Saved/GoogleSheetLoader`에 저장된다.

### DataForge RuleSet

1. Adapter를 `Google Sheet Cache`로 선택한다.
2. Source Asset에 해당 `GoogleSheetConfig`를 지정한다.
3. Probe로 컬럼을 확인한다.
4. 나머지 Schema, Output, Binding을 설정한다.

`Auto Apply DataForge`가 활성화되어 있으면 Fetch 성공 후 해당 config를 참조하는 모든 RuleSet이 새 Preview를 거쳐 즉시 Apply된다. 검증에 실패한 RuleSet은 적용되지 않고 상태 메시지에 결과가 추가된다.

Google Sheet Cache는 MultiSource의 primary 또는 secondary input으로도 사용할 수 있다.

## 7. 갱신 방식

| Source | 갱신 방법 |
|---|---|
| Google Sheet Cache | Config Fetch 성공 시 선택적으로 자동 Preview/Apply |
| CSV | 파일 수정 후 RuleSet Preview/Apply 또는 commandlet 실행 |
| JSON | 파일 수정 후 RuleSet Preview/Apply 또는 commandlet 실행 |

CSV와 JSON은 현재 파일 변경 watcher가 없다. 그러나 Preview 후 파일이 바뀌면 source drift로 Apply가 차단되므로 다시 Preview해야 한다.

## 8. Preview 결과 읽기

요약은 DataTable row와 Managed asset 변경을 구분한다.

```text
Rows C:2 U:0 =:0 O:0
Assets C:4 M:0 U:0 =:0 O:0
```

- `C`: Create
- `M`: Move
- `U`: Update
- `=`: Unchanged
- `O`: Orphan

Error가 하나라도 있으면 Apply할 수 없다. Warning은 missing foreign row나 사용되지 않은 foreign key처럼 검토가 필요한 상태다.

## 9. 흔한 오류

### Join Column missing

지정한 키 이름이 실제 header와 정확히 같은지 확인한다. 공백과 대소문자를 확인한다.

### Duplicate join key

동일 입력 안에 같은 join key가 두 번 존재한다. CSV/JSON에서 중복 행을 제거한다.

### Column conflict

두 source가 같은 컬럼 이름에 다른 값을 제공한다. secondary input에 `Column Prefix`를 지정하거나 source schema를 정리한다.

### External asset missing

Base Folder, Subfolder Pattern, Asset Name Pattern으로 계산된 경로에 에셋이 없다. Preview의 desired path와 Content Browser의 실제 package path를 비교한다.

### Apply requires Preview

RuleSet이나 source가 바뀌었다. 새 Preview를 실행한 뒤 Apply한다.

### Existing asset ownership collision

Managed 출력 경로에 DataForge가 소유하지 않는 에셋이 있다. 기존 에셋을 다른 경로로 옮기거나 Managed Asset Rule을 변경한다. DataForge는 임의로 덮어쓰지 않는다.

## 10. 여러 RuleSet 관리

Project Overview에서 다음을 한눈에 확인한다.

- RuleSet 경로
- Source file 또는 provider asset
- DataTable 출력
- RuleSet dependencies
- dependency-first 실행 순서

선행 데이터가 필요한 경우 `Dependencies`에 다른 RuleSet을 추가하고 `Preview Dependency Graph`, `Apply Dependency Graph`를 사용한다. 순환 dependency는 허용되지 않는다.

## 11. CI와 commandlet

전체 검증:

```text
UnrealEditor-Cmd.exe Chimera.uproject -run=DataForge -All -ValidateOnly
```

예상하지 않은 변경이 있으면 CI 실패:

```text
UnrealEditor-Cmd.exe Chimera.uproject -run=DataForge -All -ValidateOnly -FailOnChanges
```

단일 RuleSet Apply:

```text
UnrealEditor-Cmd.exe Chimera.uproject -run=DataForge \
  -RuleSet=/Game/DataForgeExamples/Rules/RS_CsvItemExample -Apply
```

Snapshot export와 검증:

```text
UnrealEditor-Cmd.exe Chimera.uproject -run=DataForge -All -ExportSnapshots
UnrealEditor-Cmd.exe Chimera.uproject -run=DataForge -All -VerifySnapshots
```

## 12. 권장 운영 규칙

- Source file과 RuleSet, snapshot, 생성 결과를 같은 변경 단위로 검토한다.
- Apply 전에 Preview diff를 확인한다.
- External과 Managed 경로를 분리한다.
- 여러 source의 동명 컬럼은 의미가 다르면 prefix를 사용한다.
- Managed orphan은 Apply에서 자동 삭제하지 말고 별도 Cleanup으로 검토한다.
- Recovery Manifest만 복구 수단으로 믿지 말고 source control을 유지한다.
