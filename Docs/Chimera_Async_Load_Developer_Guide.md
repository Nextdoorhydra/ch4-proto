# Chimera 비동기 로드 개발 가이드

## 1. 목적

이 문서는 파츠, 장애물, 시야, 연출 등 런타임 콘텐츠를 Chimera의 공용 비동기 로드 흐름에 연결하는 기준이다. 다른 개발자에게 전달하거나 AI 작업 프롬프트에 첨부할 수 있도록 구현 계약과 검증 기준을 한곳에 정리한다.

스테이지 전체 흐름은 [게임 흐름·스테이지 런타임 구조](Chimera_GameFlow_Stage_Architecture.md), 레벨 제작 절차는 [스테이지 제작 가이드](Chimera_Stage_Authoring_Guide.md)를 따른다.

## 2. 결론

런타임 콘텐츠는 다음 한 경로로 준비한다.

```text
StageRoute
  -> CMStageLoadSchedule
  -> LoadGroup
  -> Primary Asset ID
  -> UPrimaryDataAssetBase 파생 Definition PDA
  -> Asset Bundle에 등록된 Soft Reference
  -> 그룹 Ready 알림
  -> Actor/Component가 Get()으로 이미 로드된 에셋 사용
```

게임플레이 Actor나 Component가 `LoadSynchronous()`, `TryLoad()`, 개별 `RequestAsyncLoad()`를 호출하지 않는다. 로드 요청과 수명 관리는 `CMStageLoadCoordinatorSubsystem`과 `AsyncPDALoader`만 담당하고, 콘텐츠 코드는 준비 완료된 결과만 소비한다.

## 3. 책임 분리

| 구성요소 | 책임 |
|---|---|
| DataForge·Google Sheet | 제작 데이터 입력, 검증, Definition PDA 자동 생성 |
| Definition PDA | 런타임 설정과 Soft Asset Reference 소유 |
| CMStageLoadSchedule | PDA를 그룹으로 묶고 요청 순서와 수명 결정 |
| CMStageLoadCoordinatorSubsystem | 스테이지 단위 그룹 요청, 상태, 완료 이벤트 관리 |
| AsyncPDALoader | Primary Asset과 지정 Asset Bundle 실제 비동기 로드·캐시·해제 |
| Actor·Component | 그룹 Ready 이후 PDA와 하위 에셋 사용, 실패 시 비활성 유지 |

DataForge는 런타임 로더가 아니다. Sheet에서 생성한 PDA도 수동 생성한 PDA와 동일하게 Schedule에 등록해야 한다.

## 4. Definition PDA 규칙

### 필수 규칙

- 런타임 Definition은 `UPrimaryDataAssetBase`를 상속한다.
- Mesh, Material, Niagara, Sound, Ability, GameplayEffect, Blueprint Class 같은 무거운 참조는 `TSoftObjectPtr` 또는 `TSoftClassPtr`로 둔다.
- 플레이에 필요한 Soft Property에는 `meta = (AssetBundles = "Gameplay")`를 지정한다.
- 소비자는 로드 완료 뒤 `SoftReference.Get()`만 사용한다.
- 같은 에셋을 Shell BP 기본값에 Hard Reference로 다시 지정하지 않는다.
- 생성된 PDA를 런타임 원본으로 사용하고, 같은 값을 DataTable에서 `BeginPlay`마다 다시 찾지 않는다.

```cpp
UCLASS(BlueprintType)
class CHIMERA_API UCMPartDefinition : public UPrimaryDataAssetBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (AssetBundles = "Gameplay"))
    TSoftObjectPtr<UStaticMesh> Mesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (AssetBundles = "Gameplay"))
    TSoftClassPtr<UGameplayAbility> AbilityClass;
};
```

초기에는 필수 에셋을 모두 `Gameplay` Bundle로 묶는다. 실제 메모리 측정 결과가 생기기 전에는 `Visual`, `Audio` 같은 Bundle을 세분화하지 않는다.

### DataTable을 유지해야 하는 임시 이행안

기존 구조를 한 번에 PDA 중심으로 바꾸기 어렵다면 DataTable 자체를 Soft Reference로 가진 래퍼 PDA를 만들고 그 PDA를 Schedule에 등록할 수 있다. 이 경우에도 그룹 Ready 후 `DataTableSoftRef.Get()`으로만 접근한다.

이 방식은 전환용이다. 최종적으로는 DataForge가 생성한 행별 Definition PDA가 런타임 원본이 되어야 불필요한 전체 테이블 로드와 행 조회를 없앨 수 있다.

## 5. 소비자 구현 계약

레벨 배치 Actor나 런타임 Component는 다음 상태 흐름을 따른다.

```text
BeginPlay
  -> Definition과 LoadGroupId 검증
  -> 시작은 숨김·Collision Off·Tick Off 등 안전한 비활성 상태
  -> 이미 Ready면 즉시 Resolve
  -> 아니면 OnLoadGroupFinished 구독
  -> 성공: Definition.Get()과 하위 Soft Reference.Get() 검증 후 적용·활성화
  -> 실패: Error 로그를 남기고 비활성 유지
  -> EndPlay: Delegate 해제
```

`UCMObstacleDefinitionComponent`가 이 계약의 기준 구현이다. 새로운 파츠 소비 컴포넌트가 필요하다면 같은 패턴을 사용하되, 공용 로더에 파츠 전용 API를 추가하지 않는다.

실패 시 동기 로드로 우회하거나 누락 상태로 계속 진행하지 않는다. 필수 `BeforeStageStart` 그룹 실패는 스테이지 시작 실패로 처리하고, 후속 콘텐츠 실패는 해당 콘텐츠를 비활성 상태로 유지하면서 오류를 크게 남긴다.

## 6. LoadGroup과 로드 시점

LoadGroup은 같은 시점에 준비하고 같은 수명으로 관리할 Primary Asset 묶음이다. 액터 배치 순서나 한 개 오브젝트를 뜻하지 않는다.

| 정책 | 사용 기준 | 파츠 예시 |
|---|---|---|
| `BeforeStageStart` | 플레이 시작 즉시 필요하고 서버 배리어가 기다려야 함 | 초기 머리·다리·손·몸통, 시작 시야 설정 |
| `Sequential` | 진행 순서가 정해진 후속 콘텐츠 | 중후반 고정 파츠 픽업, 후반 장애물 외형 |
| `OnDemand` | 선택 전에는 필요 없는 큰 분기 | 분기 전용 파츠·장애물 후보 |

- 첫 구역은 반드시 `BeforeStageStart`다.
- `Sequential`은 10, 20, 30처럼 간격을 둔 `LoadOrder`를 사용한다.
- `Priority`는 같은 요청 시점의 로더 우선도이고, 콘텐츠 진행 순서는 `LoadOrder`가 정한다.
- 현재 `OnDemand`는 모든 클라이언트의 준비를 기다리는 통과 배리어가 아니다. 요청 직후 반드시 필요한 콘텐츠에는 쓰지 않는다.
- 랜덤 후보 중 하나를 선택한다면 선택 전에 후보 풀 전체를 준비하거나, 선택 결과가 확정된 뒤 안전한 대기 구간에서 해당 그룹을 요청한다.
- 프로파일링 근거 없이 파츠 하나마다 그룹을 만들지 않는다.

권장 이름은 `<StageId>.<AreaOrPurpose>`다.

```text
S01.Entry.Parts
S01.Area02.Parts
S01.Area03.Hazards
S01.BranchA.Content
```

## 7. 두 파츠 브랜치 수정 가이드

### feat/parts-head

현재 머리·시야 경로의 `DefaultRenderConfig.LoadSynchronous()`와 `PostProcessMaterial.LoadSynchronous()`는 게임 시작 중 동기 로드를 발생시킨다.

권장 수정 순서:

1. 시야 설정 PDA가 `UPrimaryDataAssetBase`를 상속하도록 바꾸거나, 머리 Definition PDA 안에 시야 설정을 포함한다.
2. Post Process Material과 필요한 Texture를 `Gameplay` Asset Bundle Soft Reference로 등록한다.
3. 초기 머리와 시야 Definition Primary Asset ID를 `BeforeStageStart` 그룹에 넣는다.
4. Vision Manager가 로드 요청을 직접 하지 않게 하고, 준비된 Definition을 주입받거나 캐시에서 조회하게 한다.
5. 그룹 Ready 이후 `DefaultRenderConfig.Get()`과 `PostProcessMaterial.Get()`만 사용한다.
6. 누락 시 시야를 임의 기본값으로 진행하지 말고 명확한 Error 로그와 비활성 상태를 유지한다.

시야 마스크의 Render Target 생성과 매 프레임 갱신은 런타임 계산이므로 비동기 로드 대상이 아니다. 설정 PDA, Material, Texture처럼 디스크에서 읽는 콘텐츠만 대상이다.

### feat/parts/leg

현재 `CMPartActorBase::InitializeFromPartData()`의 `PartDataTable.LoadSynchronous()`는 파츠 생성 시 동기 로드를 발생시킨다. 테스트 파츠 Class의 `LoadSynchronous()`도 같은 문제를 가진다.

권장 수정 순서:

1. DataForge가 Leg, Arm, SpringArm 행별 `UCMPartDefinition` PDA를 생성하게 한다.
2. 파츠 Actor는 DataTable과 RowName 대신 Soft Definition과 LoadGroupId를 가진다.
3. 초기 장착·드롭 파츠 PDA는 `BeforeStageStart`, 후반 픽업은 `Sequential`에 등록한다.
4. 그룹 Ready 후 Definition의 수치와 `Mesh.Get()`, `AbilityClass.Get()` 등을 적용한다.
5. 랜덤 스폰용 파츠 Class가 필요하면 별도 파츠 카탈로그 PDA의 `TSoftClassPtr`로 관리하고 스폰 전에 그룹을 준비한다.
6. Non-Shipping 테스트 Class도 가능하면 같은 경로를 사용한다. 임시 동기 로드를 남기면 디버그 전용임을 코드와 문서에 명확히 표시한다.

공통 몸통 데이터의 `CMChimeraData.cpp`에 남은 `BodyDataTable.LoadSynchronous()`도 같은 규칙으로 이후 이행해야 한다. 두 파츠 브랜치만 고치고 몸통 경로를 남기면 게임 시작 Hitch가 완전히 사라지지 않는다.

## 8. 금지 패턴

| 금지 | 이유 | 대체 |
|---|---|---|
| Actor `BeginPlay`에서 `LoadSynchronous()` | 프레임 Hitch와 머신별 로드 시점 불일치 | BeforeStageStart 또는 Sequential 그룹 |
| 소비자가 직접 `RequestAsyncLoad()` | 요청·실패·해제 상태가 분산됨 | StageLoadCoordinator 요청 |
| Soft Reference를 BP Hard Reference로 중복 지정 | 맵 로드 때 에셋이 함께 로드되어 비동기 설계 무효화 | Definition PDA 한 곳에서만 참조 |
| 실패 후 동기 fallback | 누락 데이터가 테스트에서 숨겨짐 | Error 로그와 비활성·흐름 중단 |
| 모든 것을 BeforeStageStart에 등록 | 초기 로딩만 길어짐 | 실제 등장 순서에 따라 Sequential 사용 |
| 오브젝트마다 LoadGroup 생성 | 관리 비용과 요청 수 증가 | 같은 구역·시점·수명끼리 묶기 |

Editor 모듈과 DataForge 제작 파이프라인의 `LoadSynchronous()`는 에디터 도구 실행을 위한 것이므로 런타임 게임플레이 코드와 구분한다.

## 9. 개발 절차

1. 콘텐츠의 첫 사용 시점을 정한다.
2. `UPrimaryDataAssetBase` 파생 Definition과 필요한 Soft Property를 만든다.
3. 필요한 Property에 `Gameplay` Asset Bundle을 지정한다.
4. DataForge 또는 수동 제작으로 PDA를 생성한다.
5. 해당 스테이지 `CMStageLoadSchedule` Catalog에 Primary Asset을 등록한다.
6. GroupId, LoadPolicy, LoadOrder를 지정하고 `Refresh And Rebuild Catalog`를 실행한다.
7. 소비 Actor·Component에 Soft Definition과 동일한 GroupId를 지정한다.
8. Ready 전 비활성, Ready 후 `Get()` 적용, 실패 시 Error 상태를 구현한다.
9. 에디터를 재시작한 Cold Cache 상태에서 직접 스테이지와 로비 Route 양쪽을 테스트한다.

## 10. 검증 체크리스트

- Definition이 `UPrimaryDataAssetBase` 파생 클래스인가?
- Project Settings의 Asset Manager Scan 대상에 해당 Primary Asset Type과 경로가 포함됐는가?
- Schedule Catalog에 올바른 Primary Asset ID와 GroupId가 들어갔는가?
- `Refresh And Rebuild Catalog`를 실행했는가?
- 필요한 하위 Soft Reference에 `Gameplay` Bundle이 지정됐는가?
- 소비자가 그룹 Ready 전 `Get()` 결과를 사용하지 않는가?
- 런타임 Source에 새 `LoadSynchronous()`, `TryLoad()`, 독립 `RequestAsyncLoad()`가 없는가?
- BP 기본값에 같은 런타임 에셋의 Hard Reference가 중복되지 않았는가?
- 실패 시 로그가 보이고 해당 콘텐츠가 안전하게 비활성화되는가?
- 멀티 PIE에서 서버와 모든 클라이언트가 동일한 준비 결과를 받는가?
- `AssetReferenceValidator`와 Requirement Coverage 검증을 통과하는가?

## 11. AI 작업용 프롬프트 규칙

아래 블록을 기능 요청 뒤에 그대로 붙여 사용할 수 있다.

```text
[Chimera 비동기 로드 구현 계약]

이 프로젝트의 런타임 콘텐츠 로드는 기존 AsyncPDALoader와
CMStageLoadCoordinatorSubsystem만 사용한다.

1. 런타임 Definition은 UPrimaryDataAssetBase를 상속한다.
2. 무거운 에셋은 TSoftObjectPtr/TSoftClassPtr로 선언하고 필요한 Property에
   meta=(AssetBundles="Gameplay")를 지정한다.
3. 게임플레이 Actor/Component에서 LoadSynchronous(), TryLoad(), 개별
   RequestAsyncLoad()를 새로 사용하지 않는다.
4. PDA는 스테이지 CMStageLoadSchedule Catalog의 LoadGroup에 등록한다.
5. 초기 필수 콘텐츠는 BeforeStageStart, 진행 순서가 있는 후속 콘텐츠는
   Sequential, 선택 전 불필요한 큰 분기만 OnDemand를 사용한다.
6. 소비자는 OnLoadGroupFinished 또는 이미 Ready인 그룹 상태를 확인한 뒤
   Soft Reference.Get()으로 이미 로드된 객체만 사용한다.
7. Ready 전에는 Actor를 숨김/Collision Off/Tick Off 등 안전한 비활성 상태로 둔다.
8. 실패 시 동기 fallback하지 않고 Error 로그를 남기며 비활성 상태를 유지한다.
9. DataForge가 생성한 PDA를 런타임 원본으로 사용하고 BeginPlay에서 DataTable을
   동기 로드해 행을 다시 조회하지 않는다.
10. 같은 에셋을 Definition Soft Reference와 BP Hard Reference에 중복 등록하지 않는다.
11. 공용 로더에 기능 전용 API를 추가하지 말고 기존 장애물 DefinitionComponent의
    Ready/Resolve/Failure 패턴을 따른다.
12. 변경 후 Cold Cache와 멀티 PIE에서 성공·실패 흐름을 검증하고, 남아 있는
    런타임 LoadSynchronous/TryLoad/독립 RequestAsyncLoad 위치를 보고한다.

작업 전 현재 Schedule, Primary Asset Type, Asset Bundle, 소비 시점을 먼저 조사하고
실제 프로젝트 API에 없는 함수나 타입을 가정하지 않는다.
```

## 12. 완료 기준

두 파츠 브랜치의 비동기 전환은 단순히 Soft Pointer 타입으로 바꾸는 것으로 끝나지 않는다. 다음 조건을 모두 만족해야 완료다.

- 로비 Route 또는 직접 스테이지 시작 시 필수 파츠와 시야 에셋이 시작 배리어 전에 준비된다.
- 파츠 생성과 시야 초기화 중 게임플레이 코드의 동기 로드가 발생하지 않는다.
- Ready 전 접근과 Schedule 누락이 Validator 또는 Error 로그로 드러난다.
- 실패를 동기 로드로 숨기지 않는다.
- 스테이지 종료 시 Schedule 수명 정책에 따라 에셋을 해제할 수 있다.
