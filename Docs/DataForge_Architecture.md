# DataForge 플러그인 설계 문서

문서 버전: 1.1

대상 구현: DataForge 1.3

대상 엔진: Unreal Engine 5.7

## 1. 목적

DataForge는 외부 데이터와 Unreal 에셋 사이의 반복적인 변환 코드를 선언형 RuleSet으로 대체하는 Editor 전용 Content Materialization 프레임워크다.

핵심 목표는 데이터가 CSV, JSON, Google Sheet 캐시 또는 프로젝트 전용 파서 중 어디에서 왔는지와 관계없이 동일한 정규화 데이터 경계를 거쳐 DataTable, DataAsset, PrimaryDataAsset과 외부 에셋 참조를 생성하는 것이다.

```text
Raw Source / Existing Parser / Provider Cache
                    ↓
             Source Adapter
                    ↓
       FDataForgeDataSet (canonical)
                    ↓
           UDataForgeRuleSet
                    ↓
      Compile → Preview → Apply
                    ↓
 DataTable + DA/PDA + External References
```

DataForge는 런타임 데이터베이스나 게임 실행 중 Google Sheet 접근을 제공하지 않는다. 결과 에셋은 Editor에서 생성하고, 게임 런타임은 생성된 Unreal 에셋만 소비한다.

## 2. 설계 원칙

### 파서 독립성

컴파일러는 CSV나 Google Sheet의 구현을 알지 못한다. 모든 입력은 `IDataForgeSourceAdapter`가 `FDataForgeDataSet`으로 변환한다. 어댑터는 직접 파일을 파싱하거나 기존 파서의 결과를 변환하거나 캐시를 읽을 수 있다.

### Preview 우선

Apply 전에 반드시 성공한 Preview가 있어야 한다. Preview 이후 소스, RuleSet 또는 대상 에셋이 변하면 Apply를 차단하고 다시 Preview하도록 요구한다.

### 결정적 결과

동일한 소스와 RuleSet은 동일한 행 이름, 패키지 경로, 바인딩 결과를 만들어야 한다. 출력 이름에 시간이나 Asset Registry 검색 순서를 사용하지 않는다.

### 소유권 분리

- `Managed`: DataForge가 생성하고 추적하는 DA/PDA다. 이동, 갱신, 고아 판정 대상이다.
- `External`: 아티스트나 다른 시스템이 소유하는 Texture, Mesh 등의 에셋이다. DataForge는 정확한 경로로 조회하고 참조만 설정한다.

## 3. 모듈 구조

### DataForgeCore

- RuleSet 및 직렬화 타입
- Source Adapter registry
- CSV/JSON 어댑터
- RuleSet compiler
- 정규화 데이터와 바인딩 계획
- 외부 에셋 경로 해석
- 관리 에셋 생성 계획
- dependency graph

### DataForgeEditor

- RuleSet factory와 전용 에디터
- Creation Wizard
- Probe, Preview, Apply 서비스
- Binding Graph와 property picker
- Semantic Diff와 Project Overview
- Recovery Manifest
- Snapshot JSON/YAML
- DataForge commandlet
- DataTable/managed output 삭제 감지와 Creation Wizard 재개

### ChimeraEditor 통합

- `GoogleSheetCache` 어댑터
- `MultiSource` 외래키 조인 어댑터
- Google Sheet 캐시 갱신 이벤트 처리
- 프로젝트 통합 및 E2E 자동화 테스트
- Google parser 기반 MCP RuleSet bootstrap 명령

Google Sheet 및 MultiSource는 현재 프로젝트 통합 계층에 등록된다. DataForgeCore는 특정 Google Sheet 플러그인에 의존하지 않는다.

## 4. 핵심 데이터 모델

### FDataForgeDataSet

어댑터가 출력하는 canonical dataset이다.

- `Columns`: 정규화된 컬럼 이름
- `Rows`: 컬럼별 문자열 값과 원본 행 위치
- `SourceRevision`: 전체 입력을 대표하는 결정적 revision/hash

타입 변환은 Source Adapter가 아닌 binding 단계에서 대상 `FProperty`를 기준으로 수행한다.

### UDataForgeRuleSet

하나의 RuleSet은 다음을 소유한다.

- `Source`: 어댑터와 입력 위치
- `Schema`: Primary Key와 필수 컬럼
- `Output`: DataTable RowStruct와 출력 경로
- `AssetRules`: External 조회 또는 Managed 생성 경로
- `GeneratedOutputs`: DA/PDA 역할과 클래스
- `Bindings`: source/output에서 대상 property로의 연결
- `Dependencies`: 먼저 적용되어야 하는 다른 RuleSet

### Binding

Binding source는 세 종류다.

- `SourceValue`: 정규화 데이터 컬럼 값
- `ResolvedAsset`: External Asset Rule로 찾은 UObject
- `GeneratedOutput`: 같은 행에서 생성할 DA/PDA 참조

Binding target은 DataTable row 또는 Generated Output이다.

Creation Wizard가 Bindings 단계에 진입할 때 exact-name 추론을 자동 실행한다. source column은 같은 이름의 editable row property에 연결하고, 정의된 Generated Output은 같은 이름의 soft-object row property에 연결한다. 기존 target binding은 우선하며 자동 추론은 누락된 target만 추가한다.

## 5. Source Adapter 계약

```cpp
class IDataForgeSourceAdapter
{
public:
    virtual FDataForgeSourceDescriptor Describe() const = 0;
    virtual bool Probe(
        const FDataForgeSourceConfig& Source,
        FDataForgeDataSet& OutDataSet,
        TArray<FDataForgeDiagnostic>& OutDiagnostics) const = 0;
    virtual bool Fetch(
        const FDataForgeSourceConfig& Source,
        FDataForgeDataSet& OutDataSet,
        TArray<FDataForgeDiagnostic>& OutDiagnostics) const = 0;
};
```

`Probe`는 UI를 위한 제한된 샘플을 반환할 수 있다. `Fetch`는 Apply 계획에 필요한 전체 데이터를 반환해야 한다. 두 함수 모두 동일한 컬럼 의미와 안정적인 `SourceRevision`을 제공해야 한다.

현재 등록 어댑터는 다음과 같다.

| Adapter | 입력 | 비고 |
|---|---|---|
| CSV | 파일 | 프로젝트 상대 경로와 절대 경로 지원 |
| JSON | 파일 | flat object array 또는 normalized JSON 지원 |
| Google Sheet Cache | `UGoogleSheetConfig` | `Saved/GoogleSheetLoader`의 normalized JSON 사용 |
| Multi Source (Join) | 입력 배열 | CSV, JSON, Google Sheet Cache를 left join |

`Parameters`는 사용자 정의 어댑터의 확장 옵션을 위한 `FName → FString` 맵이다. 현재 기본 어댑터들은 값을 요구하지 않는다.

## 6. Multi Source 외래키 조인

`Inputs[0]`은 primary dataset이고 이후 입력은 모두 primary에 left join된다.

```text
Inputs[0] Items.csv       JoinColumn = Id
Inputs[1] Prices.csv      JoinColumn = ItemId
Inputs[2] Localization    JoinColumn = ItemId
Inputs[3] Balance.json    JoinColumn = Code
```

각 secondary input의 `JoinColumn` 값이 primary input의 `JoinColumn` 값과 비교된다. 입력 개수는 배열이므로 하나로 제한되지 않는다.

현재 제약은 다음과 같다.

- 모든 secondary source는 `Inputs[0]`과 직접 조인한다.
- 조인은 left join으로 고정된다.
- 입력 하나당 단일 컬럼 키만 지원한다.
- 각 입력의 join key는 비어 있거나 중복될 수 없다.
- secondary source끼리의 연쇄 조인과 복합키는 지원하지 않는다.

`ColumnPrefix`는 secondary source의 비키 컬럼 앞에 붙는다. 예를 들어 `Stats_`를 지정하면 `Attack`은 `Stats_Attack`이 된다. 두 입력이 같은 비키 컬럼을 제공하면서 값이 다를 경우 prefix가 없으면 Preview 오류가 발생한다.

모든 입력 revision은 하나의 결합 revision으로 계산되므로 어느 한 소스가 변경되어도 기존 Preview는 무효가 된다.

## 7. Asset Rule과 경로 계산

Asset Rule은 source column token을 사용한다.

```text
BaseFolder       = /Game/DataForgeExamples/Textures
SubfolderPattern = {Category}
AssetNamePattern = T_{TextureId}
```

행이 `Category=Weapons`, `TextureId=Sword`라면 원하는 경로는 다음과 같다.

```text
/Game/DataForgeExamples/Textures/Weapons/T_Sword.T_Sword
```

토큰 이름은 컬럼 이름과 대소문자까지 정확히 일치해야 한다. 미해결 토큰, 잘못된 package path, 누락된 External 에셋은 Preview 오류다.

Managed Asset은 다음 metadata로 소유권을 기록한다.

```text
DataForge.Managed
DataForge.RuleSetId
DataForge.RecordId
DataForge.Role
DataForge.RuleVersion
```

동일한 `(RuleSetId, RecordId, Role)`이면서 목표 경로만 달라지면 Move로 계획한다. 소유권이 다른 기존 에셋은 덮어쓰지 않는다.

## 8. Compile, Preview, Apply

### Compile

- Source Adapter 존재 확인
- Primary Key와 필수 컬럼 확인
- RowStruct와 target property 확인
- Generated Output과 Asset Rule 연결 확인
- dependency cycle 확인

### Preview

- 전체 source fetch
- 원하는 DataTable row 계산
- 외부 에셋 정확 경로 조회
- Managed DA/PDA create/move/update 계산
- current/desired diff와 diagnostic 생성
- source/rule/target revision 저장

Preview는 Content를 변경하지 않는다.

### Apply

- Preview 이후 drift 재검증
- Managed asset 생성 또는 이동
- 선언된 property만 기록
- DataTable row 생성 또는 갱신
- package 저장
- Recovery Manifest 기록

Apply는 프로젝트 전체의 원자적 트랜잭션이 아니다. dependency graph는 순서대로 각 RuleSet을 다시 Preview하고 Apply하며, 뒤의 RuleSet 실패가 앞의 성공을 롤백하지 않는다.

## 9. 자동 갱신 정책

### Google Sheet

새 `UGoogleSheetConfig`는 다음 옵션을 기본 활성화한다.

- `Save Normalized Json`
- `Auto Apply DataForge`

Fetch 성공 시 normalized cache를 저장하고 cache update event를 방송한다. ChimeraEditor는 해당 config를 직접 또는 MultiSource input으로 참조하는 모든 RuleSet을 찾아 각각 새 Preview와 Apply를 실행한다. 한 RuleSet의 실패는 다른 RuleSet을 막지 않는다.

`DataParser`와 DataTable 할당은 선택 사항이다. cache-only 용도에서는 parser 없이도 Fetch할 수 있다.

### MCP 최초 생성

프로젝트 MCP는 `system_control.console_command`를 통해 `DataForge.MCP.CreateRuleSetFromGoogleParser` 고수준 명령을 실행한다. 이 명령은 기존 `GoogleSheetConfig`의 parser TargetTable을 우선 재사용하고, 없으면 요청에서 Row Struct와 DataTable 경로를 받는다. normalized cache Probe, schema/primary-key 추론, exact-name binding, Preview, Apply, RuleSet 저장을 하나의 검증된 흐름으로 수행한다.

이 경로는 최초 생성 전용이다. 동일 경로의 RuleSet은 덮어쓰지 않으며 생성 이후의 RuleSet 변경 책임은 전용 에디터와 사용자에게 넘어간다. 범용 MCP 에셋 명령을 여러 번 조합해 RuleSet 내부 구조를 작성하지 않는 이유는 중간 실패로 불완전한 RuleSet이 남는 것을 막기 위해서다.

Parser TargetTable이 없는 cache-only 구성에서는 `RowStruct`와 `Output`을 명시해야 한다. cache가 없으면 mutation 전에 실패하며 Google config에서 Fetch한 뒤 재시도한다. 상세 계약은 `Docs/DataForge_MCP_Guide.md`에 정의한다.

### 출력 삭제 복구

Asset Registry의 in-memory delete event에서 DataTable 출력 경로 또는 `DataForge.Managed` metadata가 RuleSet과 일치하는 DA/PDA 삭제를 감지한다. 삭제 처리가 끝난 다음 tick에 해당 RuleSet의 Creation Wizard를 열어 재생성 경로를 제공한다. commandlet과 unattended 실행에서는 UI를 열지 않는다.

### CSV와 JSON

현재 CSV/JSON 파일에는 파일 watcher가 없다. 파일 변경 후 RuleSet Editor의 Preview/Apply 또는 commandlet을 실행해야 한다. 단, Apply 직전 source revision을 재검증하므로 오래된 Preview로 변경된 파일을 적용할 수는 없다.

## 10. 안전성과 추적

- Preview 전에는 mutation 없음
- External asset은 이동하거나 삭제하지 않음
- Apply는 orphan을 자동 삭제하지 않음
- orphan 정리는 별도 확인 작업
- 이전 DataTable JSON과 관리 property 변경을 Recovery Manifest에 기록
- RuleSet snapshot을 `Config/DataForge/Snapshots`에 JSON/YAML로 export 가능
- Project Overview에서 RuleSet source, output, dependencies를 추적

Recovery Manifest는 자동 복구나 binary backup이 아니다. 삭제/이동 복구에는 source control을 함께 사용해야 한다.

## 11. 예제 구성

보존형 예제는 `/Game/DataForgeExamples`에 있다.

```text
Source/Items.csv
Source/Prices.csv
Rules/RS_CsvItemExample
Textures/{Category}/T_{TextureId}
Generated/{Category}/DA_{Id}
Generated/{Category}/PDA_{Id}
Output/DT_CsvItems
```

`Items.Id`와 `Prices.ItemId`를 조인하며, category와 texture id로 외부 Texture 경로를 계산한다. `DataForge.Examples.PersistentCsvAssets` 자동화 테스트가 생성, 저장, 재적용과 참조를 검증한다.

## 12. 현재 한계와 후속 과제

- MultiSource 복합키와 join 방식 선택
- secondary source 간 단계적 조인
- CSV/JSON 파일 watcher 기반 자동 Apply
- Recovery Manifest 자동 복원
- 대규모 dataset용 changed-record incremental apply/cache
- 기본 Adapter `Parameters` UI의 adapter별 schema화

이 기능들은 현재 계약을 깨지 않고 Source Adapter와 Editor service 확장으로 추가하는 것을 원칙으로 한다.
