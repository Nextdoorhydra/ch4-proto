# DataForge MCP 최초 생성 지침

## 목적과 책임 경계

MCP는 기존 `GoogleSheetConfig`와 그 안의 Google parser를 기준으로 DataForge RuleSet을 **최초 한 번 생성**한다. 생성 이후의 바인딩, Asset Rule, Generated Output, dependency 변경은 개발자가 DataForge RuleSet Editor에서 직접 관리한다.

MCP는 일반 `create_data_table`, `set_property` 명령을 여러 번 조합하지 않는다. 다음 프로젝트 고수준 명령 하나를 `system_control.console_command` capability로 실행한다.

```text
DataForge.MCP.CreateRuleSetFromGoogleParser Config=/Game/Data/GS_Body
```

## MCP 호출 형태

```json
{
  "operation": "execute",
  "capability": "system_control.console_command",
  "params": {
    "action": "console_command",
    "command": "DataForge.MCP.CreateRuleSetFromGoogleParser Config=/Game/Data/GS_Body"
  }
}
```

Parser에 `TargetTable`이 설정되어 있다면 해당 DataTable의 Row Struct와 패키지 경로를 자동으로 재사용한다. 생성 RuleSet의 기본 경로는 `/Game/DataForge/Rules/RS_<GoogleSheetConfig 이름>`이다.

Parser에 DataTable이 없는 cache-only 구성은 Row Struct와 출력 경로를 명시한다.

```text
DataForge.MCP.CreateRuleSetFromGoogleParser Config=/Game/Data/GS_Items RowStruct=/Script/Chimera.CMItemTableRow Output=/Game/Data/DT_Items RuleSet=/Game/Data/Rules/RS_Items PrimaryKey=Id
```

선택 인자:

- `RuleSet`: 생성할 RuleSet 패키지 경로
- `RowStruct`: parser TargetTable이 없을 때 사용할 native 또는 asset Row Struct 경로
- `Output`: parser TargetTable이 없을 때 사용할 DataTable 패키지 경로
- `PrimaryKey`: 자동 추론 대신 사용할 source column
- `Apply=false`: RuleSet만 생성하고 DataTable 반영은 보류

## 명령 동작

1. GoogleSheetConfig와 parser TargetTable을 확인한다.
2. normalized JSON cache를 Probe한다.
3. 전체 source column을 Required Columns로 기록하고 `RowName`, `Id`, `*Id`, 첫 컬럼 순서로 Primary Key를 추론한다.
4. 동일 이름의 editable Row Struct property를 자동 바인딩한다.
5. mutation-free Preview를 통과해야 RuleSet을 생성한다.
6. 기본값으로 즉시 Apply하고 RuleSet 및 GoogleSheetConfig를 저장한다.
7. Google config의 `Save Normalized Json`과 `Auto Apply DataForge`를 활성화한다.

## 실패 처리

- cache가 없으면 Google config에서 Fetch를 먼저 실행한다.
- TargetTable이 없으면 `RowStruct`와 `Output`을 모두 제공한다.
- Primary Key가 source column에 없으면 정확한 컬럼명으로 다시 요청한다.
- 같은 경로의 RuleSet이 이미 존재하면 덮어쓰지 않는다. Creation Wizard 또는 RuleSet Editor에서 기존 에셋을 유지보수한다.
- Preview/Apply가 실패하면 로그의 DataForge diagnostic을 해결한 후 재시도한다.

## 완료 확인

MCP는 실행 후 `[DataForge MCP]` 로그에서 RuleSet 경로, DataTable 경로, detected columns, bindings, apply 상태를 보고하고 두 에셋의 존재를 확인한다. 이 결과가 최초 생성의 인계점이며, 이후 RuleSet은 사용자가 에디터에서 관리한다.
