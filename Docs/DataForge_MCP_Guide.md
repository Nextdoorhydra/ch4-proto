# DataForge MCP RuleSet 생성 가이드

## 목적

개발자는 Unreal 내부 구조를 모두 알 필요 없이 Google parser 또는 `GoogleSheetConfig`를 Source of Truth로 지정하고, 원하는 출력과 에셋 규칙을 설명한다. Codex는 이를 검증된 DataForge 고수준 명령으로 변환한다.

MCP는 여러 개의 범용 에셋 수정 명령을 조합하지 않는다. 생성 도중 실패해 불완전한 RuleSet이 남지 않도록 `DataForge.MCP.CreateRuleSetFromGoogleParser` 한 경로만 사용한다.

## 사용자 프롬프트 템플릿

아래 템플릿에서 불필요한 선택 항목은 삭제해도 된다. 에셋 경로나 클래스명을 모르면 `자동 탐색`이라고 적는다.

```text
[DataForge RuleSet 생성 요청]

Source
- Google Parser 또는 Config: /Game/Data/Body/DA_BodyDataParser (모르면 `자동 탐색`)
- Source 검색 폴더: /Game/Data/Body (자동 탐색일 때)
- Primary Key: ID

DataTable Output
- Row Struct: FCMBodyTableRow
- 저장 경로: /Game/Data/Body/DT_Body
- RuleSet 저장 경로: /Game/Data/Body/RS_Body

Generated Assets (선택)
- Output Name: BodyData
- 종류: DA
- 클래스: CMBodyDataAsset
- 저장 폴더: /Game/Data/Body
- 이름 규칙: DA_{ID}
- DataTable 대상 속성: BodyData

External Assets (선택)
- Rule Id: Icon
- 종류: Texture
- 저장 폴더: /Game/Data/Texture
- 이름 규칙: T_{ID}
- Source Column: ID
- Generated Output: BodyData
- 대상 속성: Icon

Options
- 즉시 Apply: true
- 에셋 저장: true
```

Codex는 표시 이름을 Unreal 경로로 추측하지 않고 프로젝트에서 실제 Config, Row Struct 및 클래스를 조회한다. 후보가 여러 개면 생성 전에 사용자에게 선택을 요청한다.

## 단순 요청

PDA/DA 또는 외부 에셋 규칙이 없는 Google→DataTable 요청은 한 줄 명령을 사용한다.

```text
DataForge.MCP.CreateRuleSetFromGoogleParser Config=/Game/Data/GS_Items RowStruct=/Script/Chimera.CMItemTableRow Output=/Game/Data/DT_Items RuleSet=/Game/Data/Rules/RS_Items PrimaryKey=Id
```

Parser에 호환되는 `TargetTable`이 있으면 `RowStruct`와 `Output`은 생략할 수 있다. 기본 RuleSet 경로는 `/Game/DataForge/Rules/RS_<ConfigName>`이며 `Apply` 기본값은 `true`다.

## PDA/DA 및 외부 에셋이 포함된 요청

복합 요청은 Codex가 임시 JSON Spec으로 변환한다. Spec은 보통 `Saved/DataForge/McpRequests`에 두며 프로젝트 에셋이 아니다.

```json
{
  "config": "/Game/Data/Body/DA_BodyDataParser",
  "ruleSet": "/Game/Data/Body/RS_Body",
  "rowStruct": "/Script/Chimera.CMBodyTableRow",
  "output": "/Game/Data/Body/DT_Body",
  "primaryKey": "ID",
  "apply": true,
  "saveAssets": true,
  "assetRules": [
    {
      "id": "BodyData",
      "ownership": "Managed",
      "baseFolder": "/Game/Data/Body",
      "assetNamePattern": "DA_{ID}"
    },
    {
      "id": "Icon",
      "ownership": "External",
      "baseFolder": "/Game/Data/Texture",
      "assetNamePattern": "T_{ID}"
    }
  ],
  "generatedOutputs": [
    {
      "name": "BodyData",
      "type": "DataAsset",
      "class": "/Script/Chimera.CMBodyDataAsset",
      "assetRule": "BodyData"
    }
  ],
  "bindings": [
    {
      "source": "ResolvedAsset",
      "column": "ID",
      "assetRule": "Icon",
      "target": "GeneratedOutput",
      "targetOutput": "BodyData",
      "property": "Icon"
    },
    {
      "source": "GeneratedOutput",
      "sourceOutput": "BodyData",
      "target": "DataTableRow",
      "property": "BodyData"
    }
  ]
}
```

실행 명령:

```text
DataForge.MCP.CreateRuleSetFromGoogleParser Spec=Saved/DataForge/McpRequests/body.json
```

## JSON 필드 계약

루트 필드:

- `config`, `ruleSet`: 필수
- `rowStruct`, `output`: Parser TargetTable이 없을 때 필수
- `primaryKey`: 생략하면 `RowName`, `Id`, `*Id`, 첫 번째 열 순서로 추론
- `apply`, `saveAssets`: 기본값 `true`
- `assetRules`, `generatedOutputs`, `bindings`: 선택 배열

Asset Rule:

- `id`: 다른 항목에서 선택할 안정적인 ID
- `ownership`: `Managed` 또는 `External`
- `baseFolder`: `/Game` Content 경로
- `subfolderPattern`: 선택
- `assetNamePattern`: 필수, `{ColumnName}` 토큰은 대소문자를 구분

Generated Output:

- `name`: 바인딩에서 사용하는 Output Name
- `type`: `DataAsset` 또는 `PrimaryDataAsset`
- `class`: 실제 Unreal `UDataAsset` 클래스 경로
- `assetRule`: `Managed` Asset Rule ID

Binding:

- `source`: `SourceValue`, `ResolvedAsset`, `GeneratedOutput`
- `column`, `sourceOutput`, `assetRule`: Source 종류에 필요한 항목만 지정
- `target`: `DataTableRow` 또는 `GeneratedOutput`
- `targetOutput`: Generated Output 대상일 때 지정
- `property`: 대상 속성 경로
- `required`: 선택, 기본값 `true`

정확히 이름이 같은 열과 속성은 자동으로 바인딩된다. JSON의 `bindings`에는 Texture 경로 해석이나 이름이 다른 속성처럼 추론할 수 없는 관계만 작성한다.

## 실행 및 완료 확인

1. Config와 클래스 경로가 실제로 존재하는지 확인한다.
2. Google normalized cache가 없으면 Config에서 Fetch를 한 번 실행한다.
3. 고수준 명령을 실행한다.
4. `[DataForge MCP]` 로그에서 RuleSet, DataTable, column 수, binding 수 및 Apply 결과를 확인한다.
5. 생성된 RuleSet과 DataTable이 존재하는지 확인한다.
6. 요청한 PDA/DA와 외부 에셋 참조가 Preview/Apply 결과에 포함되었는지 확인한다.

동일 경로에 RuleSet이 이미 존재하면 명령은 덮어쓰지 않는다. 이후 변경은 DataForge RuleSet Editor와 Creation Wizard에서 관리한다.

## 대표 실패 처리

- `Could not read DataForge MCP spec`: Spec 파일 경로를 프로젝트 기준 또는 절대 경로로 다시 확인한다.
- `Spec field ... is required`: 필수 JSON 필드를 추가한다.
- `class is not a UDataAsset class`: Blueprint generated class 또는 native 클래스의 실제 경로를 다시 조회한다.
- cache missing: Google Config에서 `Save Normalized Json`을 활성화하고 Fetch 후 재시도한다.
- Preview 실패: DataForge diagnostic의 column, property, Asset Rule ID 및 토큰 대소문자를 수정한다.
- RuleSet already exists: 기존 RuleSet을 Editor에서 유지보수하거나 새로운 RuleSet 경로를 사용한다.
