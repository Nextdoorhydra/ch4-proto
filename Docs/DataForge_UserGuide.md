# DataForge 에디터 사용법

대상 버전: DataForge 1.4

## 1. 가장 빠른 사용 절차

1. Content Browser에서 `Miscellaneous > DataForge RuleSet`을 생성한다.
2. RuleSet을 열고 `Creation Wizard`를 선택한다.
3. Source Adapter와 File 또는 Source Asset을 선택한다.
4. `Run Probe`로 column과 sample row를 읽는다.
5. Primary Key와 DataTable Row Struct를 확인한다.
6. Output DataTable의 Content 경로를 선택한다.
7. 필요한 Asset Rule과 Generated Output을 정의한다.
8. 자동 생성된 Bindings를 검토한다.
9. `Run Preview` 후 `Finish & Apply`를 선택한다.

Wizard는 다음 순서로 필요한 설정만 보여준다.

```text
Source → Probe → Schema → Output
       → Asset Rules → Generated Outputs → Bindings → Preview
```

## 2. Source 설정

### CSV 또는 JSON

- Adapter: `CSV` 또는 `JSON`
- File: `...` 버튼으로 선택
- 권장 경로: `Content/.../Source/File.csv`

절대 경로와 프로젝트 상대 경로를 모두 지원한다. Unreal file picker가 반환하는 `../../..` 형식도 프로젝트 기준으로 정규화한다.

### Google Sheet Cache

- Adapter: `Google Sheet Cache`
- Source Asset: `GoogleSheetConfig`
- Config에서 `Save Normalized Json`, `Auto Apply DataForge` 활성화

기존 DataParser와 TargetTable은 선택 사항이다. Cache-only 방식은 Row Struct와 DataTable Output을 RuleSet에서 지정한다. Google Fetch 성공 시 normalized cache를 참조하는 모든 RuleSet이 즉시 새 Preview/Apply를 수행한다.

### Multi Source

첫 번째 Input이 결과 행을 결정하고 이후 Input은 left join된다.

```text
Input 0: Items.csv   Join Column = Id
Input 1: Prices.csv  Join Column = ItemId
Input 2: Sheet Cache Join Column = Code
```

secondary input 수에는 제한이 없다. column 이름이 충돌하면 `Column Prefix`를 사용한다. `Parameters`는 프로젝트 전용 Adapter 옵션이며 기본 CSV/JSON/Google Adapter에서는 비워도 된다.

## 3. Schema와 Output

Probe는 전체 source column을 Required Columns로 제안하고 `RowName`, `Id`, `*Id`, 첫 column 순서로 Primary Key를 추론한다. Primary Key는 비어 있거나 중복될 수 없다.

Output에서는 다음을 선택한다.

- Row Struct: `FTableRowBase` 파생 USTRUCT
- Asset Path: `/Game/Data/DT_Items` 형태의 Content 경로
- Create If Missing: DataTable이 없으면 생성
- Save After Apply: 성공한 Apply 결과 저장

경로는 가능한 한 picker를 사용한다.

## 4. Asset Rule

### External 에셋

Texture나 Mesh처럼 프로젝트가 소유하는 에셋은 조회만 한다.

```text
Rule Id: Icon
Ownership: External
Base Folder: /Game/Data/Texture
Asset Name Pattern: T_{ID}
```

`{ID}`는 현재 row의 `ID` column 값으로 치환된다. 토큰 이름은 대소문자를 구분한다.

### Managed 에셋

DataForge가 DA/PDA를 생성하고 추적할 때 사용한다.

```text
Rule Id: BodyData
Ownership: Managed
Base Folder: /Game/Data/Body
Asset Name Pattern: DA_{ID}
```

Rule ID는 RuleSet 내부에서 고유해야 한다.

## 5. Generated Output과 Bindings

Generated Output에서 Output Name, DA/PDA 종류, 클래스와 Managed Asset Rule을 선택한다. Asset Rule ID는 직접 입력하지 않고 드롭다운에서 선택한다.

Generated Output을 추가하거나 변경하면 다음 바인딩이 자동으로 동기화된다.

- Source column → 같은 이름의 DataTable property
- Source column → 같은 이름의 PDA/DA property
- Generated Output → 같은 이름의 DataTable soft-object property

외부 에셋은 수동 관계가 필요하다.

```text
Source: ResolvedAsset
Source Column: ID
Asset Rule Id: Icon
Target: GeneratedOutput
Target Output: BodyData
Target Property: Icon
```

Source/Target Output과 Asset Rule ID는 현재 RuleSet 목록에서 드롭다운으로 선택할 수 있다.

## 6. 자동 갱신

Editor가 열려 있으면 다음 변경이 RuleSet을 자동 재조정한다.

| 변경 | 동작 |
|---|---|
| CSV/JSON 파일 수정 | 파일 watcher → debounce → Preview/Apply |
| Google Fetch 성공 | 해당 Config 참조 RuleSet 즉시 Preview/Apply |
| RuleSet 수정 | 새 Preview/Apply 예약 |
| DataTable 또는 Managed/External 에셋 수정 | 소유/경로가 관련된 RuleSet 재조정 |
| Row Struct 또는 DA/PDA 클래스 reload | 관련 RuleSet 재조정 |

검증에 실패하면 출력은 변경하지 않고 DataForge Message Log에 diagnostic을 남긴다. DataTable이나 DataForge 소유 DA/PDA를 삭제하면 해당 RuleSet의 Creation Wizard가 열려 재생성 설정을 확인할 수 있다.

## 7. MCP로 최초 RuleSet 생성

Codex에 요청할 때는 [DataForge MCP 가이드](DataForge_MCP_Guide.md)의 템플릿을 사용한다.

간단한 요청 예:

```text
/Game/Data/GS_Items를 기준으로 DataForge RuleSet을 만들어줘.
Row Struct는 FItemTableRow이고 DataTable은 /Game/Data/DT_Items에 저장해줘.
Primary Key는 Id야.
```

DA/PDA와 Texture 규칙이 포함된 요청에는 Source, 저장 경로, 클래스, 이름 규칙과 대상 property를 적는다. Codex는 실제 Unreal 경로를 조회한 뒤 JSON Spec을 만들고 고수준 명령 하나로 Preview/Apply한다.

기존 RuleSet은 MCP가 덮어쓰지 않는다. 생성 이후에는 RuleSet Editor와 Creation Wizard에서 관리한다.

## 8. 예제

보존형 예제 위치:

```text
/Game/DataForgeExamples/Rules/RS_CsvItemExample
/Game/DataForgeExamples/Output/DT_CsvItems
Content/DataForgeExamples/Source/Items.csv
Content/DataForgeExamples/Source/Prices.csv
```

Items와 Prices를 외래키로 병합하고 Texture를 자동 조회하며 DA/PDA와 DataTable을 생성한다. `DataForge.Examples.PersistentCsvAssets` 테스트는 기존 예제 에셋을 재작성하지 않고 저장된 예제의 Preview/Apply 결과를 검증한다.

## 9. 오류 해결

- `DF1002 Could not read CSV source`: File 경로를 picker로 다시 선택하고 프로젝트 상대 경로인지 확인한다.
- Primary Key 오류: source header의 정확한 column 이름과 중복/빈 값을 확인한다.
- External asset missing: Base Folder, Subfolder, Asset Name Pattern으로 계산한 실제 Content 경로를 확인한다.
- Generated class/property 오류: 클래스와 target property가 존재하며 editable인지 확인한다.
- Existing asset ownership collision: 기존 에셋을 이동하거나 Managed 경로를 변경한다.
- Apply requires Preview: source 또는 설정이 바뀌었으므로 새 Preview를 실행한다.

## 10. Project Overview와 CI

Project Overview에서 모든 RuleSet의 경로, source/provider, DataTable output, dependency 및 dependency-first 실행 순서를 확인할 수 있다.

```text
UnrealEditor-Cmd.exe Chimera.uproject -run=DataForge -All -ValidateOnly
UnrealEditor-Cmd.exe Chimera.uproject -run=DataForge -All -ValidateOnly -FailOnChanges
UnrealEditor-Cmd.exe Chimera.uproject -run=DataForge -RuleSet=/Game/Data/RS_Items -Apply
```

Source, RuleSet, snapshot과 생성 결과는 같은 변경 단위로 검토하고 source control로 복구한다.
