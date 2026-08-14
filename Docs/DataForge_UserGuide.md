# DataForge 에디터 사용법

대상 버전: DataForge 1.5

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
UnrealEditor-Cmd.exe Chimera.uproject -run=DataForge -All -ValidateOnly -FailOnOutdatedProfiles
UnrealEditor-Cmd.exe Chimera.uproject -run=DataForge -Profile=/Game/DataForge/Profiles/ALP_Character -Rebase -ValidateOnly
UnrealEditor-Cmd.exe Chimera.uproject -run=DataForge -Profile=/Game/DataForge/Profiles/ALP_Character -Rebase -Apply
UnrealEditor-Cmd.exe Chimera.uproject -run=DataForge -RuleSet=/Game/Data/RS_Items -Apply
```

Source, RuleSet, snapshot과 생성 결과는 같은 변경 단위로 검토하고 source control로 복구한다.

## 11. Rename Audit와 Recovery Center

에셋 이름 또는 폴더가 Naming Policy와 맞지 않을 때 다음 절차를 사용한다.

1. Content Browser에서 에셋을 하나 이상 선택하고 `DataForge Rename Audit...`를 실행한다.
2. 폴더 전체를 검사하려면 폴더를 우클릭하고 `DataForge Audit Folder...`를 실행한다.
3. `[Recommended]` 후보의 점수와 Evidence를 확인한다.
4. 적용할 후보만 체크하고 `Rename / Move Checked`를 누른다.
5. 확인 대화상자에서 경로를 검토한 뒤 승인한다.

추천은 기존 Association Manifest, 현재 이름의 정확한 일치, Subject 폴더, Naming Policy 파싱 결과를 사용한다. 최고 점수가 하나일 때만 추천하며 동점은 `DF1955`로 차단한다. DataForge는 증거가 없는 Source Row를 임의로 선택하지 않는다.

리네임 전에 `Saved/DataForge/Recovery/Rename_*.json`이 기록된다. 되돌리려면 `Tools > DataForge Recovery Center...`를 열고 다음을 확인한다.

- 상태가 `Succeeded` 또는 `FailedRollbackIncomplete`인지
- 원래 경로가 비어 있는지
- 변경된 경로에 기록된 에셋이 존재하는지
- 복원할 전체 경로가 충돌 없이 검증되는지

`Restore Selected Batch`를 승인하면 별도의 `Restore_*.json`을 먼저 만든 뒤 역방향 배치를 실행한다. 복원 실패 시 DataForge는 이미 이동된 에셋을 복원 직전 경로로 되돌리며 결과를 매니페스트에 기록한다. Recovery Center는 경로 복원 도구이며 source control이나 삭제된 `.uasset`의 바이너리 백업을 대체하지 않는다.

## 12. Rename 진단 코드

| 코드 | 의미 |
|---|---|
| DF1936-DF1939 | Naming Policy/폴더 추론 또는 목적지 경로·충돌 오류 |
| DF1940-DF1946 | 변경 없음, 오래된 에셋/RuleSet/Source of Truth 후보 |
| DF1947-DF1951 | 일괄 감사의 중복 목적지·중복 원본·AssetTools 실패 |
| DF1952-DF1953 | 리네임 복구 매니페스트 생성/갱신 실패 |
| DF1955 | 최고 점수 후보 동률로 자동 선택 차단 |
| DF1956 | 유일한 추천 후보의 점수와 근거 |
| DF1957-DF1961 | Recovery Center 읽기·상태·경로·충돌 검증 |
| DF1962-DF1965 | 복원 매니페스트·AssetTools·상태 갱신 오류 |

## 13. BodyParser에서 PDA와 폴더 에셋을 자동 연결하는 전체 가이드

이 절은 다음 결과를 만드는 참조 구성이다.

```text
BodyParser/Google cache
    -> ID별 PDA_CMBody_{ID} 생성
    -> Character/Body/{ID} 폴더의 Unreal 에셋 탐색
    -> Texture, Material, Mesh, Niagara를 PDA property에 연결
    -> source 또는 폴더 변경 시 자동 재조정
```

### 13.1 필요한 파일과 에셋

| 구분 | 예시 파일/에셋 | 필수 여부 | 역할 |
|---|---|---:|---|
| Source schema | Google Sheet 또는 `Body.csv` | 필수 | `ID`, `BodyType` 등 원본 레코드 제공 |
| Google config | `DA_BodyDataParser` 또는 `GS_Body` | Google 사용 시 | normalized JSON cache 생성과 갱신 event 제공 |
| Legacy parser | `UBodyDataParser` | 선택 | 기존 DataTable materialization 유지 시 사용 |
| DataTable row struct | `FCMBodyTableRow` | DataTable 사용 시 | DataTable row schema와 선택적 PDA 참조 정의 |
| PDA class | `UCMBodyDataAsset` | 필수 | 생성할 PDA의 Texture/Material/Mesh/Niagara property 정의 |
| Naming Policy | `NP_CMCharacter` | 폴더 자동화 시 필수 | 에셋 prefix, project prefix, kind folder 규칙 정의 |
| Layout Recipe | `ALR_CMBody` | 폴더 자동화 시 필수 | 폴더에서 Subject와 AssetKind 위치 추출 |
| Folder Source Config | `FSC_CMBodyInventory` | 폴더 자동화 시 필수 | Asset Registry 검색 root와 제외 범위 정의 |
| Binding Preset | `BP_CMBodyPDA` | PDA association 시 필수 | 생성 PDA와 semantic slot/property 관계 정의 |
| RuleSet | `RS_CMBody` | 필수 | primary source, association source, output을 결합 |
| Output DataTable | `DT_CMBody` | 선택 | row별 PDA 참조가 필요한 경우 생성 |
| Generated PDA | `PDA_CMBody_HumanMale` | 자동 생성 | 최종 게임 런타임 소비 에셋 |
| Association Manifest | PDA package metadata | 자동 생성 | DataForge가 관리하는 property 경로와 에셋 참조 추적 |
| Rename recovery | `Saved/DataForge/Recovery/*.json` | 자동 생성 | Rename Audit 적용과 복원 기록 |
| Layout Profile | `ALP_CMCharacter` | 여러 RuleSet 재사용 시 | 공통 Asset Rule 경로 템플릿 재사용 |

최소 구성은 `PDA class + Source + NP + ALR + FSC + BP + RS`다. DataTable, 기존 parser와 Layout Profile은 요구에 따라 생략할 수 있다.

### 13.2 권장 프로젝트 배치

여러 Body ID를 지원하는 권장 구조다.

```text
Source/Chimera/Data/Body/
├─ CMBodyDataAsset.h
├─ CMBodyDataAsset.cpp
└─ CMBodyTableRow.h

Source/ChimeraEditor/Parser/
├─ BodyDataParser.h
└─ BodyDataParser.cpp

Content/Chimera/Character/Body/
├─ Definitions/
│  ├─ NP_CMCharacter
│  ├─ ALR_CMBody
│  ├─ FSC_CMBodyInventory
│  ├─ BP_CMBodyPDA
│  └─ RS_CMBody
├─ HumanMale/
│  ├─ Texture/
│  ├─ Material/
│  ├─ Mesh/
│  └─ Niagara/
├─ HumanFemale/
│  └─ {Texture,Material,Mesh,Niagara}/
└─ Generated/
   ├─ PDA_CMBody_HumanMale
   └─ PDA_CMBody_HumanFemale
```

Unreal `/Game` 경로는 실제 Content 폴더부터 시작한다. 예를 들어 물리 경로 `Content/Chimera/Character/Body`는 `/Game/Chimera/Character/Body`다. `Charcater`와 같은 오타, `body`/`Body` 대소문자 불일치는 source key와 folder match를 깨뜨릴 수 있으므로 사용하지 않는다.

Body 레코드가 하나뿐이고 `ID=Body`라면 기존 구조도 가능하다.

```text
/Game/Chimera/Character/Body/{Texture,Material,Mesh,Niagara}
```

이 경우에만 Folder Source Root를 `/Game/Chimera/Character`로 두고 `Body`를 Subject로 사용한다. 여러 ID가 하나의 `Body/Texture` 폴더를 공유하면 어느 PDA 소유인지 자동으로 판별할 수 없다.

### 13.3 Source column 준비

권장 Google Sheet 또는 CSV header:

```csv
RowName,ID,BodyType
HumanMale,HumanMale,Human
HumanFemale,HumanFemale,Human
```

- `ID`: RuleSet Primary Key이자 폴더 Subject와 PDA 이름 토큰
- `RowName`: 기존 `UBodyDataParser`가 DataTable row name으로 사용
- `BodyType`: PDA/DataTable에 전달할 일반 source value

Primary Key는 빈 값과 중복을 허용하지 않는다. 폴더 기반 association을 사용하면 `ID` 값은 Subject 폴더 이름과 정확히 같아야 한다.

### 13.4 PDA 클래스 파일 작성

프로젝트 runtime 모듈에 concrete `UPrimaryDataAsset` 클래스를 만든다. 클래스가 Editor 모듈에만 있으면 cook된 게임에서 PDA를 사용할 수 없으므로 `Source/Chimera` 같은 runtime 모듈에 둔다.

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/PrimaryDataAsset.h"
#include "CMBodyDataAsset.generated.h"

class UMaterialInterface;
class UNiagaraSystem;
class UStaticMesh;
class UTexture2D;

UCLASS(BlueprintType)
class CHIMERA_API UCMBodyDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString ID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString BodyType;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UTexture2D> Portrait;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<TSoftObjectPtr<UTexture2D>> Textures;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<TSoftObjectPtr<UMaterialInterface>> Materials;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<TSoftObjectPtr<UStaticMesh>> Meshes;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<TSoftObjectPtr<UNiagaraSystem>> NiagaraSystems;
};
```

Binding Preset이 찾을 수 있도록 대상 property는 `EditAnywhere` 또는 다른 editable flag를 가져야 한다. 단일 slot에는 단일 object/soft-object property를, `Many` slot에는 object/soft-object 배열을 사용한다.

DataTable row가 PDA를 참조해야 한다면 `FCMBodyTableRow`에 다음 필드를 추가한다.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite)
TSoftObjectPtr<UCMBodyDataAsset> BodyData;
```

현재 프로젝트의 `FCMBodyTableRow`에는 이 필드가 없으므로 DataTable에서 PDA까지 연결하려면 먼저 추가해야 한다.

### 13.5 Google Sheet Config 설정

Google Sheet를 사용하면 `UGoogleSheetConfig` 에셋을 열고 다음을 설정한다.

| 필드 | 값/예시 | 설명 |
|---|---|---|
| Google Sheet URL | 공개 Sheet URL 또는 spreadsheet id | 원본 위치 |
| Range From | `A1` | header 포함 시작 셀 |
| Range To | `Z1000` | 필요한 최대 범위 |
| Data Parser | `BodyDataParser` 또는 None | 기존 parser를 함께 실행할 때만 지정 |
| Save Normalized Json | true | DataForge Google Sheet Cache Adapter에 필수 |
| Skip Data Parser | cache-only면 true | TargetTable 없이 cache만 갱신 |
| Auto Apply DataForge | true | Fetch 성공 후 참조 RuleSet을 즉시 Preview/Apply |
| Auto Save On Complete | 팀 정책에 따라 선택 | legacy parser 결과 저장 정책 |

두 실행 모드를 혼동하지 않는다.

```text
Cache-only DataForge:
  Data Parser = None
  Save Normalized Json = true
  Skip Data Parser = true

Legacy BodyParser + DataForge 병행:
  Data Parser = BodyDataParser
  BodyDataParser.TargetTable = DT_BodyDataTable
  Save Normalized Json = true
  Skip Data Parser = false
```

현재 `UBodyDataParser::OnParseComplete`는 `ValidateTargetTable`을 호출한다. 따라서 `Skip Data Parser=false`로 기존 parser를 실행한다면 `TargetTable`이 반드시 필요하다. TargetTable을 사용하지 않으려면 cache-only 모드로 설정한다.

Fetch 후 `Last Normalized Json Path`, `Fetch Status`, `Last Message`를 확인한다. RuleSet Probe가 실패하면 먼저 Google config의 Fetch를 성공시켜 cache를 생성한다.

### 13.6 Naming Policy 생성과 설정

Content Browser에서 `Miscellaneous > Data Asset`을 선택하고 `DataForgeNamingPolicy` 클래스로 `NP_CMCharacter`를 생성한다.

```text
Project Prefix: CM
```

`Asset Kinds`:

| Asset Kind | Type Prefix | Folder Name | Expected Asset Class |
|---|---|---|---|
| Texture | `T` | `Texture` | `Texture2D` |
| Material | `M` | `Material` | `MaterialInterface` |
| StaticMesh | `SM` | `Mesh` | `StaticMesh` |
| SkeletalMesh | `SK` | `Mesh` | `SkeletalMesh` |
| Niagara | `NS` | `Niagara` | `NiagaraSystem` |

같은 `Mesh` 폴더에서 StaticMesh와 SkeletalMesh를 함께 허용할 수 있지만 `Asset Kind`와 Expected Class는 분리한다. 예상 이름은 다음과 같다.

```text
T_CMHumanMalePortrait
T_CMHumanMaleTexture_1
M_CMHumanMaleMaterial_1
SK_CMHumanMaleBody
NS_CMHumanMaleEffect_1
```

### 13.7 Asset Layout Recipe 생성과 설정

`Miscellaneous > Data Asset`에서 `DataForgeAssetLayoutRecipe`를 선택해 `ALR_CMBody`를 생성한다.

```text
Recipe Id: CMBody
Domain: Character
Naming Policy: NP_CMCharacter
Subject Source: Folder Segment
Subject Folder Index: 0
Kind Folder Index: 1
Require Kind Folder Match: true
```

이 인덱스는 Folder Source Root 아래의 상대 경로를 기준으로 한다.

```text
Root: /Game/Chimera/Character/Body
Path: /Game/Chimera/Character/Body/HumanMale/Texture/T_CMHumanMalePortrait

Segment 0 = HumanMale
Segment 1 = Texture
```

### 13.8 Folder Source Config 생성과 설정

`Miscellaneous > Data Asset`에서 `DataForgeFolderSourceConfig`를 선택해 `FSC_CMBodyInventory`를 생성한다.

```text
Root Folder: /Game/Chimera/Character/Body
Recursive: true
Allowed Asset Kinds:
  - Texture
  - Material
  - StaticMesh
  - SkeletalMesh
  - Niagara
Excluded Folders:
  - /Game/Chimera/Character/Body/Generated
Exclude DataForge Managed Assets: true
Layout Recipe: ALR_CMBody
```

`Generated`를 제외하면 생성된 PDA가 inventory에 다시 들어오는 순환을 피할 수 있다. `Allowed Asset Kinds` 값은 Naming Policy의 `Asset Kind`와 대소문자까지 같아야 한다.

### 13.9 Binding Preset 생성과 설정

Content Browser에서 `Miscellaneous > DataForge Binding Preset`으로 `BP_CMBodyPDA`를 생성한다.

기본 필드:

```text
Output Name: BodyData
Target Class: CMBodyDataAsset
Asset Name Prefix: PDA_CMBody
Row Reference Property: BodyData
```

`Asset Name Prefix`에는 `{ID}`를 직접 넣지 않는다. Preset materialization이 RuleSet Primary Key를 붙여 `PDA_CMBody_{ID}` 규칙을 만든다. DataTable을 만들지 않거나 row에서 PDA를 참조하지 않으면 `Row Reference Property`는 비워도 된다.

권장 slot:

| Slot ID | Asset Kind | Role | Target Property | Expected Class | Cardinality | Reconcile | Required |
|---|---|---|---|---|---|---|---:|
| Portrait | Texture | Portrait | `Portrait` | Texture2D | One | Assign | true |
| Textures | Texture | Texture | `Textures` | Texture2D | Many | Merge By Key | false |
| Materials | Material | Material | `Materials` | MaterialInterface | Many | Merge By Key | false |
| StaticMeshes | StaticMesh | Body | `Meshes` | StaticMesh | Many | Merge By Key | false |
| Effects | Niagara | Effect | `NiagaraSystems` | NiagaraSystem | Many | Merge By Key | false |

모든 자동 slot에 다음을 지정한다.

```text
Association Source Id: BodyInventory
Source Key Column: ID
```

- `One`: 후보가 둘 이상이면 Preview 오류
- `Optional One`: 0개 또는 1개 허용
- `Many`: 배열 property 필요
- `Merge By Key`: DataForge가 이전에 넣은 항목만 교체하고 수동 추가 항목 보존
- `Replace Managed`: 현재 association source에 없는 DataForge 관리 항목 제거
- `Manual`: slot 선언만 유지하고 자동 property write 생략

### 13.10 RuleSet 생성과 Creation Wizard 설정

`Miscellaneous > DataForge RuleSet`으로 `RS_CMBody`를 만들고 Creation Wizard를 연다.

#### Source 단계

Google 사용:

```text
Adapter: Google Sheet Cache
Source Asset: DA_BodyDataParser 또는 해당 GoogleSheetConfig
```

CSV 시험:

```text
Adapter: CSV
File: Content/Chimera/Character/Body/Source/Body.csv
```

`Run Probe`를 실행해 `RowName`, `ID`, `BodyType`이 표시되는지 확인한다.

#### Schema 단계

```text
Primary Key: ID
Required Columns:
  - ID
  - BodyType
```

기존 parser까지 함께 사용한다면 `RowName`도 Required Columns에 포함하는 것을 권장한다.

#### Output 단계

DataTable이 필요한 경우:

```text
Row Struct: CMBodyTableRow
Asset Path: /Game/Chimera/Character/Body/Generated/DT_CMBody
Create If Missing: true
Save After Apply: true
```

PDA만 필요하면 DataTable Output은 비워둘 수 있다. Binding Preset의 `Row Reference Property`도 비운다.

#### Binding Preset과 Generated Output

```text
Binding Preset: BP_CMBodyPDA
Generated Output Folder: /Game/Chimera/Character/Body/Generated
```

Preset을 materialize하면 다음 구성이 생성된다.

```text
Managed Asset Rule:
  Rule Id: BodyData_Managed
  Ownership: Managed
  Base Folder: /Game/Chimera/Character/Body/Generated
  Asset Name Pattern: PDA_CMBody_{ID}

Generated Output:
  Output Name: BodyData
  Type: Primary Data Asset
  Class: CMBodyDataAsset
  Asset Rule Id: BodyData_Managed
```

`ID`, `BodyType`처럼 이름이 같은 PDA property는 Source Value binding으로 자동 제안된다. 자동 제안되지 않았다면 다음을 확인한다.

```text
Source: Source Value
Source Column: BodyType
Target: Generated Output
Target Output: BodyData
Target Property: BodyType
```

#### Association Source

RuleSet의 Association Sources에 다음 항목을 추가한다.

```text
Source Id: BodyInventory
Adapter: Asset Registry Folder
Source Asset: FSC_CMBodyInventory
Match Column: Subject
Asset Path Column: ObjectPath
Asset Kind Column: AssetKind
Role Column: Role
```

Binding Preset slot의 `Association Source Id`와 RuleSet의 `Source Id`가 정확히 일치해야 한다. Association Source가 하나뿐이면 비어 있는 slot ID를 자동 선택할 수 있지만, 유지보수와 다중 source 확장을 위해 명시하는 것을 권장한다.

### 13.11 불규칙한 에셋 이름을 최초 정리하는 절차

다음 이름은 폴더에서 `Subject=HumanMale`까지만 알 수 있고 Role은 알 수 없다.

```text
HumanMale/Texture/body_diff_old
HumanMale/Texture/NewTexture_01
HumanMale/Texture/temp_mask
```

처리 순서:

1. `NP_CMCharacter`, `ALR_CMBody`, `FSC_CMBodyInventory`, `BP_CMBodyPDA`, `RS_CMBody`를 모두 저장한다.
2. Content Browser에서 `/Game/Chimera/Character/Body/HumanMale` 폴더를 우클릭한다.
3. `DataForge Audit Folder...`를 실행한다.
4. 후보의 Source record, Slot, 목적지와 Evidence를 확인한다.
5. 역할이 확실한 후보만 체크하고 `Rename / Move Checked`를 실행한다.
6. 여러 에셋이 같은 목적지를 요구하면 Role 또는 Numbering을 수동으로 확정한다.
7. RuleSet에서 새 Preview를 실행한다.
8. PDA create/update와 property write 목록을 확인하고 Apply한다.

리네임 후 정상화 예:

```text
body_diff_old -> T_CMHumanMaleTexture_1
NewTexture_01 -> T_CMHumanMalePortrait
temp_mask     -> T_CMHumanMaleTexture_2
```

현재 Rename Advisor는 아무 의미 정보가 없는 여러 파일에 `_1`, `_2`를 임의로 배정하지 않는다. 동일 목적지 충돌이나 slot 최고 점수 동점은 자동 적용하지 않고 diagnostic으로 차단한다.

### 13.12 이름을 바꿀 수 없는 에셋의 대안

레거시 또는 외부 플러그인 에셋은 explicit association CSV를 사용한다.

```csv
Subject,ObjectPath,AssetKind,Role
HumanMale,/Game/Chimera/Character/Body/HumanMale/Texture/body_diff_old.body_diff_old,Texture,Texture
HumanMale,/Game/Chimera/Character/Body/HumanMale/Texture/NewTexture_01.NewTexture_01,Texture,Portrait
HumanMale,/Game/Chimera/Character/Body/HumanMale/Material/temp_mat.temp_mat,Material,Material
```

Association Source 설정:

```text
Source Id: BodyInventory
Adapter: CSV
File: Content/Chimera/Character/Body/Source/BodyAssetMap.csv
Match Column: Subject
Asset Path Column: ObjectPath
Asset Kind Column: AssetKind
Role Column: Role
```

이 방식은 에셋 이름과 관계없이 바인딩할 수 있지만 CSV가 추가 Source of Truth가 된다. 장기 운영은 Naming Policy 기반 Folder Adapter를 우선하고 예외만 매핑 파일로 관리한다.

### 13.13 Preview에서 확인할 결과

`HumanMale`, `HumanFemale` 두 row가 있을 때 정상 Preview 예:

```text
Rows:
  Create 2 또는 Unchanged 2

Managed Assets:
  /Game/Chimera/Character/Body/Generated/PDA_CMBody_HumanMale
  /Game/Chimera/Character/Body/Generated/PDA_CMBody_HumanFemale

Associations:
  HumanMale.Portrait -> T_CMHumanMalePortrait
  HumanMale.Textures -> T_CMHumanMaleTexture_1, T_CMHumanMaleTexture_2
  HumanMale.Materials -> M_CMHumanMaleMaterial_1
```

Apply 전 다음을 검사한다.

- `DF1921`: Source ID, Source Key Column 또는 Association Source 누락
- `DF1922`: asset path가 없거나 Target Property 타입과 불일치
- `DF1923`: required slot 누락 또는 scalar slot에 후보가 여러 개
- `DF1924`: PDA property에 soft-object path를 쓸 수 없음
- `DF1925`: Association Source가 여러 개인데 slot이 Source ID를 선택하지 않음

### 13.14 자동 갱신 검증

초기 Apply 후 다음 시나리오를 각각 시험한다.

1. Google Sheet에 새 `ID` row를 추가하고 Fetch한다.
2. 대응 Subject 폴더와 convention-compliant 에셋을 추가한다.
3. 기존 Texture를 리네임하거나 이동한다.
4. PDA 배열에 수동 에셋을 하나 추가한 뒤 source 에셋을 변경한다.
5. source row를 제거하고 Preview의 orphan 계획을 확인한다.

기대 결과:

- `Auto Apply DataForge=true`이면 Google cache 갱신 후 관련 RuleSet이 재조정된다.
- Folder Source 아래 Asset Registry 변경은 해당 RuleSet의 새 Preview/Apply를 예약한다.
- `Merge By Key`는 수동 배열 항목을 보존하고 DataForge 관리 항목만 갱신한다.
- required association이 불충족되면 기존 PDA를 부분 변경하지 않는다.
- source row 제거는 orphan으로 계획되며 자동 삭제하지 않는다.

### 13.15 최종 체크리스트

```text
[ ] Source에 중복 없는 ID가 있다.
[ ] ID와 Subject 폴더 이름이 정확히 같다.
[ ] CMBodyDataAsset가 runtime 모듈의 concrete UPrimaryDataAsset다.
[ ] PDA 대상 property가 editable이고 slot cardinality와 타입이 맞다.
[ ] Google 사용 시 normalized JSON cache가 생성되었다.
[ ] Naming Policy의 Kind, Folder Name, Expected Class가 일치한다.
[ ] Layout Recipe index가 Folder Source Root 기준 상대 경로와 맞다.
[ ] Generated 폴더가 Folder Source에서 제외되었다.
[ ] Binding Preset Output Name과 Generated Output 이름이 같다.
[ ] Slot Association Source ID와 RuleSet Source ID가 같다.
[ ] 불규칙 이름은 Rename Audit 또는 explicit mapping으로 의미가 확정되었다.
[ ] Preview에 error diagnostic과 목적지 충돌이 없다.
[ ] Apply 후 PDA metadata에 Association Manifest가 기록되었다.
[ ] source control에 RuleSet, 정의 에셋, source와 생성 결과가 함께 포함되었다.
```

## 14. 파일별 생성 위치 요약

| 생성 대상 | Content Browser 생성 방법 | 권장 저장 위치 |
|---|---|---|
| Google Sheet Config | `Miscellaneous > Data Asset > GoogleSheetConfig` | `/Game/Chimera/Character/Body/Source` |
| DataForge RuleSet | `Miscellaneous > DataForge RuleSet` | `/Game/Chimera/Character/Body/Definitions` |
| Binding Preset | `Miscellaneous > DataForge Binding Preset` | `/Game/Chimera/Character/Body/Definitions` |
| Naming Policy | `Miscellaneous > Data Asset > DataForgeNamingPolicy` | `/Game/Chimera/Character/Body/Definitions` |
| Asset Layout Recipe | `Miscellaneous > Data Asset > DataForgeAssetLayoutRecipe` | `/Game/Chimera/Character/Body/Definitions` |
| Folder Source Config | `Miscellaneous > Data Asset > DataForgeFolderSourceConfig` | `/Game/Chimera/Character/Body/Definitions` |
| Asset Layout Profile | `Miscellaneous > DataForge Asset Layout Profile` | `/Game/DataForge/Profiles` |
| PDA class | C++ 또는 Blueprint class | runtime module 또는 `/Game/.../Classes` |
| Generated PDA | RuleSet Apply가 생성 | `/Game/Chimera/Character/Body/Generated` |
| DataTable | RuleSet Apply가 선택적으로 생성 | `/Game/Chimera/Character/Body/Generated` |

정의 에셋 이름의 `NP`, `ALR`, `FSC`, `BP`, `RS`, `ALP` 접두사는 문서와 Project Overview에서 역할을 빠르게 구분하기 위한 권장 규칙이다.

## 15. ID만으로 여러 폴더 에셋 자동 추론

CSV가 모든 에셋 ID를 나열하면 콘텐츠 추가 때마다 테이블도 수정해야 하므로 일반적인 폴더 자동화에는 권장하지 않는다. 대신 primary key만 CSV에 두고 Naming Policy와 Folder Source가 에셋 관계를 정규화하도록 구성한다.

```csv
Id,DisplayName
Armor,Armor Set
Robot,Robot Set
```

```text
Inventory/Armor/Texture/T_CMArmorTexture_1
Inventory/Armor/Texture/T_CMArmorTexture_2
Inventory/Armor/Material/M_CMArmorMaterial_1
```

Layout Recipe가 `Armor`를 Subject로, Naming Policy가 `Texture/Material` Kind와 Role을 추출한다. 예제 Binding Preset의 `Many + ReplaceManaged` 슬롯은 `CSV.Id == Inventory.Subject`인 모든 후보를 `PDA.Textures[]`, `PDA.Materials[]`에 기록한다. 새 에셋을 폴더에 추가할 때 CSV 수정은 필요하지 않으며 폴더 inventory가 배열 전체의 권위 있는 상태가 된다. 수동 배열 항목을 보존해야 하는 실제 프로젝트에서는 초기 migration을 정리한 뒤 `MergeByKey`를 선택한다.

보존 예제:

```text
/Game/DataForgeExamples/MultiAssetRefs/Definitions/NP_MultiAssetRefs
/Game/DataForgeExamples/MultiAssetRefs/Definitions/ALR_MultiAssetRefs
/Game/DataForgeExamples/MultiAssetRefs/Definitions/FSC_MultiAssetRefs
/Game/DataForgeExamples/MultiAssetRefs/Definitions/BP_MultiAssetRefs
/Game/DataForgeExamples/MultiAssetRefs/Rules/RS_MultiAssetRefs
/Game/DataForgeExamples/MultiAssetRefs/Output/DT_MultiAssetRefs
/Game/DataForgeExamples/MultiAssetRefs/Generated/PDA_Armor
Content/DataForgeExamples/MultiAssetRefs/Source/Products.csv
```

`DataForge.Examples.PersistentMultiAssetReferences` 테스트가 정의 에셋과 inventory를 생성하고 ID-only association 결과를 검증한다.

### 15.1 명시적인 ID 목록이 필요한 예외

`ResolvedAsset`의 Target Property가 soft-object 배열이면 source cell을 세미콜론(`;`)으로 나눠 각 ID에 같은 Asset Rule을 반복 적용한다. DataTable row와 Generated Output의 배열에 모두 사용할 수 있다.

```csv
Id,TextureIds,MaterialIds
Armor,Armor_D;Armor_N,Armor_Base;Armor_Trim
```

```text
Texture Asset Rule:
  Base Folder: /Game/DataForgeExamples/MultiAssetRefs/Textures
  Asset Name Pattern: T_{TextureIds}

Material Asset Rule:
  Base Folder: /Game/DataForgeExamples/MultiAssetRefs/Materials
  Asset Name Pattern: M_{MaterialIds}
```

`TextureIds` 배열 바인딩은 먼저 `Armor_D`를 대입해 `T_Armor_D`를 찾고, 다음으로 `Armor_N`을 대입해 `T_Armor_N`을 찾는다. 하나라도 없거나 target 배열의 element class와 호환되지 않으면 Preview가 실패한다. ID 앞뒤 공백은 제거하며 빈 항목은 무시한다.

이 방식은 폴더나 이름에 관계 의미가 없거나 외부 데이터가 명시적인 순서를 소유해야 할 때만 사용한다. 일반적인 프로젝트 에셋 자동화는 위 ID-only Folder Association 방식을 우선한다.
