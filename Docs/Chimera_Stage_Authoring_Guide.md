# Chimera 스테이지 제작 가이드

> Persistent Test Level과 개발자별 Always Loaded Sublevel 구성은
> [테스트 구역 제작 가이드](Chimera_Test_Area_Authoring_Guide.md)를 따른다.
> 테스트 구역에서는 일반 `CMStageDestination` 대신 `CMTestAreaDestination`을 사용한다.

## 1. 목적

여러 개발자가 스테이지를 하나씩 맡아도 같은 규칙으로 레벨, 기믹, 데이터와 비동기 로드를 구성하기 위한 실무 기준이다. 런타임 책임과 상태 전환은 [게임 흐름·스테이지 런타임 구조](Chimera_GameFlow_Stage_Architecture.md)를 따른다.

- 위치, 크기, Transform과 최종 시각 결과는 Level에서 조정한다.
- 장애물·퍼즐의 구체 동작은 해당 C++ 또는 Blueprint가 소유한다.
- 사건과 대상 명령 연결은 스테이지 전용 Sequence DataTable이 소유한다.
- StageDirector는 등록·라우팅·완료 보고만 담당한다.
- Sheet나 CSV는 편집 원본일 수 있지만 런타임은 DataTable과 PDA만 읽는다.
- 각 개발자의 Map, DataTable, Schedule을 스테이지별로 분리한다.

## 2. 권장 자산 구조

현재 맵 루트는 `/Game/Chimera/Environment/Level`이다.

```text
/Game/Chimera/Environment/Level/Stage01/
├─ Map/L_CMStage01
├─ Blueprint/
├─ Data/
│  ├─ DT_CMStage01Sequence
│  └─ PDA_CMStage01LoadSchedule
├─ Lighting/
├─ Effect/
├─ Audio/
└─ Sequence/
```

공용 장애물은 `/Game/Chimera/Environment/Obstacle`처럼 가장 가까운 공용 기능 폴더에 둔다. 특정 스테이지만 쓰는 변형은 해당 스테이지 폴더에 둔다. 프로젝트 전체 Route PDA는 동시 수정 충돌을 줄이도록 담당자를 정한다.

## 3. 새 스테이지 제작 순서

1. 스테이지 Map과 전용 폴더를 만든다.
2. 공용 키메라가 생성될 `PlayerStart`를 배치한다.
3. `ACMStageDirector` 또는 Blueprint 자식을 정확히 하나 배치한다.
4. 시작 연출과 결과 연출을 StageDirector에 구현하거나 연결한다.
5. 장애물, 퍼즐, Trigger, 조명, 이펙트를 배치한다.
6. 제어할 대상에 `CMStageElementComponent` 또는 파생 컴포넌트를 추가한다.
7. 룸 퍼즐은 PuzzleController에 Trigger와 대상을 직접 등록한다. Sequence 또는 Director 주소 검색이 필요한 요소에만 `PlacementId`나 `GroupTags`를 지정한다.
8. `FCMStageSequenceRow` 기반 DataTable을 만들고 StageDirector의 Sequence Component에 연결한다.
9. 스테이지 전용 `CMStageLoadSchedule` PDA를 만든다.
10. Schedule에서 `RefreshAndRebuildCatalog`를 실행해 Scope를 자동 생성한다.
11. `StageRouteDefinition`에 `StageId`, Map, Schedule을 연결한다.
12. `CMStageDestination`을 배치하고 모든 활성 몸통 마디가 함께 들어갈 크기로 Volume을 조정한다.
13. 현재 맵 직접 PIE와 2인 리슨 서버를 모두 시험한다.

Schedule에 로드할 PDA가 없어도 초기 프로토타입 실행은 가능하다. 다만 Route 행에는 Asset Manager에 등록된 유효한 Schedule ID가 있어야 데이터 검증을 통과한다.

## 4. 책임 분리

| 영역 | 담당 |
|---|---|
| Level | 액터 배치, Transform, Trigger Volume, Light, Fog, Post Process |
| 장애물·퍼즐 Actor | 이동, 충돌, 데미지, 애니메이션, 내부 상태 |
| `CMStageElementComponent` | 선택적인 PlacementId·GroupTags 등록, Command 수신, Event 발행 |
| `CMStageSequenceComponent` | Stage Event에 맞는 DataTable 행 정렬·실행 |
| `CMStageDirector` | 요소 등록소, 명령 라우팅, 연출·완료·실패 보고 |
| `CMStageLoadSchedule` | 로드 그룹과 순서·정책·해제 정책 |
| `PlayGameMode` | Phase, 로드 배리어, 승패와 다음 맵 전환 |

StageDirector에 특정 피스톤의 속도, 특정 문의 잠금 로직, 조명 색상 수치를 직접 추가하지 않는다.

## 5. 식별자와 태그

| 종류 | 자료형 | 역할 | 예시 |
|---|---|---|---|
| `StageId` | `FName` | Route, Schedule, Sequence의 스테이지 식별 | `S01` |
| `PlacementId` | `FName` | Director가 검색할 개별 배치 대상의 선택 ID | `Hallway.Door.Exit` |
| `GroupTag` | Gameplay Tag | 여러 StageElement 동시 선택 | `Chimera.Stage.Group.Hallway.Lighting` |
| `EventTag` | Gameplay Tag | 발생한 사실 | `Chimera.Stage.Event.PowerRestored` |
| `CommandTag` | Gameplay Tag | 대상에 요구하는 동작 | `Chimera.Stage.Command.Unlock` |
| `LoadGroupId` | `FName` | 함께 로드·해제할 콘텐츠 묶음 | `S01.Hallway` |

PuzzleController 직접 참조는 별도 식별자가 필요 없다. Director가 개별 인스턴스를 검색할 때 PlacementId, 집합을 검색할 때 Gameplay Tag를 사용한다.

### PlacementId

다음 경우에만 지정한다.

- Stage Sequence의 `TargetPlacementId`로 제어
- Trigger의 직접 `TargetActor` 또는 `TargetPlacementId`로 제어
- StageElement의 `RequestStageCommand`로 개별 대상 제어

PuzzleController의 `Triggers`와 `Commands.Targets`에 Actor를 직접 등록한 룸 퍼즐은 비워도 된다. GroupTags만 사용하는 요소도 PlacementId가 필요 없다.

형식은 `구역.종류.의미있는이름`이다.

```text
TankRoom.Door.Entry
PowerRoom.Puzzle.MainPanel
Hallway.Obstacle.Piston01
Laboratory.Lighting.Main
Hallway.Effect.Steam
StageExit.Trigger.Goal
```

- 같은 스테이지 안에서 유일해야 한다.
- 영문·숫자를 사용하고 공백과 한글은 사용하지 않는다.
- UObject 경로, 자동 생성 Actor 이름, Outliner Label에 의존하지 않는다.
- DataTable에서 참조하기 시작한 ID는 함부로 변경하지 않는다.
- StageDirector 등록소가 맵마다 분리되므로 StageId를 반복하지 않아도 된다.

### Gameplay Tag 루트

```text
Chimera.Stage.Event.<사건>
Chimera.Stage.Command.<동작>
Chimera.Stage.Group.<구역>.<종류>
```

- Event: 무엇이 발생했는가
- Command: 대상이 무엇을 해야 하는가
- Group: 어떤 대상들을 함께 선택하는가

`OpenTankRoomDoor`처럼 대상이 섞인 Command를 만들지 않는다.

```text
TargetPlacementId = TankRoom.Door.Entry
CommandTag = Chimera.Stage.Command.Open
```

## 6. 태그 카탈로그와 승인

현재 Native Stage Command:

```text
Chimera.Stage.Command.Effect.Activate
Chimera.Stage.Command.Effect.Deactivate
Chimera.Stage.Command.Effect.Restart
Chimera.Stage.Command.Effect.Burst
```

내부 메시지 채널은 `Chimera.Message.Stage.Event`다. 이는 `FCMStageEventMessage` 전달용 채널이므로 DataTable의 `StartEvent`로 사용하지 않는다.

다음은 제작에 유용하지만 프로젝트 태그 등록과 대상 구현이 함께 필요한 후보다.

```text
Chimera.Stage.Event.Started
Chimera.Stage.Event.AreaEntered
Chimera.Stage.Event.AreaExited
Chimera.Stage.Event.ObjectiveCompleted
Chimera.Stage.Event.PuzzleCompleted
Chimera.Stage.Event.BranchSelected
Chimera.Stage.Event.ExitReached
Chimera.Stage.Event.Failed

Chimera.Stage.Command.Activate
Chimera.Stage.Command.Deactivate
Chimera.Stage.Command.Enable
Chimera.Stage.Command.Disable
Chimera.Stage.Command.Open
Chimera.Stage.Command.Close
Chimera.Stage.Command.Lock
Chimera.Stage.Command.Unlock
Chimera.Stage.Command.Reset
Chimera.Stage.Command.Lighting.Default
Chimera.Stage.Command.Lighting.Alert
Chimera.Stage.Command.Lighting.Blackout
```

후보 문자열을 DataTable에 쓰는 것만으로 기능이 생기지 않는다. 태그 등록과 대상의 Command 처리가 필요하다.

새 태그는 기존 의미 검색 → Event/Command/Group 분류 → 공용 태그 합의 → 등록 → 대상 구현과 DataTable 검증 순으로 추가한다. 이름을 바꾸면 C++, Blueprint, 태그 설정, DataTable과 Sheet 참조를 모두 확인한다. 런타임 중 CSV나 Sheet에서 태그를 만들지 않는다.

## 7. StageElement 제작

Blueprint와 C++ 모두 같은 계약을 따른다.

1. Actor에 `UCMStageElementComponent` 또는 파생 컴포넌트를 둔다.
2. PuzzleController 직접 참조라면 PlacementId와 GroupTags를 비워도 된다. Director 주소 검색이 필요할 때만 해당 값을 지정한다.
3. 게임 판정 대상은 `AuthorityOnly`를 사용한다.
4. `ExecuteStageCommand`에서 지원 CommandTag를 처리한다.
5. 완료가 다음 진행 조건이면 서버에서 `BroadcastStageEvent`를 호출한다.

속도, 데미지, 반복 횟수, 이동 Curve와 퍼즐 내부 규칙은 해당 클래스나 전용 데이터가 소유한다. Sequence DataTable은 사건과 명령 연결만 가진다.

`AuthorityOnly`는 충돌, 물리, 데미지, 문 잠금, 퍼즐 상태와 클리어 판정에 사용한다. `AllMachines`는 조명, 나이아가라, 로컬 사운드와 비판정 표현에 사용한다. 서버 권한 Actor의 최종 상태는 해당 Actor가 Replication 또는 RepNotify로 동기화한다.

## 8. 조명과 이펙트

### CMStageLightingComponent

1. 조명 그룹 Actor에 `CMStageLightingComponent`를 추가한다.
2. PlacementId를 `<Area>.Lighting.<Name>`으로 정한다.
3. Light, Height Fog, Post Process Volume을 등록한다.
4. `Presets`에 CommandTag별 상태를 만든다.
5. 레벨에서 기준 조명을 조정하고 `Capture Current Lighting`으로 캡처한다.
6. `PreviewCommandTag`와 `Preview Selected Preset`으로 확인한다.
7. `Restore Captured Lighting`으로 프리뷰 전 상태를 복원한다.

Preset은 Light Intensity 배수, 선택적 Light Color, Fog Density, Post Process Color Gain을 가진다. 조명 표현은 일반적으로 `AllMachines`를 사용한다.

### CMStageEffectComponent

1. Actor에 Niagara Component와 `CMStageEffectComponent`를 둔다.
2. PlacementId를 `<Area>.Effect.<Name>`으로 정한다.
3. Niagara Component를 `Effects`에 등록한다.
4. 처음부터 보여야 하는 경우가 아니면 Auto Activate를 끈다.
5. `PreviewAction`과 `Preview Selected Action`으로 확인한다.
6. DataTable에서 Native Effect Command를 보낸다.

Niagara Spawn Rate, Color와 User Parameter는 Niagara System 또는 전용 Blueprint에서 조정한다. Sequence DataTable에 시각 수치를 넣지 않는다.

## 9. Sequence DataTable

Row Struct는 `FCMStageSequenceRow`다.

| 열 | 역할 | 규칙 |
|---|---|---|
| `StageId` | 스테이지 식별 | Route와 같은 ID 권장 |
| `StepOrder` | 큰 진행 단계 정렬 | 10 단위 권장 |
| `StepId` | 사람이 읽는 단계 ID | 의미 있는 영문 이름 |
| `ActionOrder` | 같은 단계 실행 정렬 | 10 단위 권장 |
| `StartEvent` | 행 실행 조건 | 정확히 일치하는 등록 EventTag |
| `TargetPlacementId` | 개별 대상 | TargetGroup과 둘 중 하나만 설정 |
| `TargetGroup` | 복수 대상 | PlacementId와 둘 중 하나만 설정 |
| `CommandTag` | 실행 명령 | 대상이 처리하는 등록 Tag |
| `DelaySeconds` | Event 이후 지연 | 0 이상 |
| `PreloadGroupId` | 명령 전 준비할 OnDemand 그룹 | Schedule의 LoadGroupId |

런타임 정렬은 `StepOrder → ActionOrder → StepId → TargetPlacementId`다. 숫자 순서는 이전 액션 완료를 기다리지 않는다. 완료 의존성이 있으면 Actor가 완료 Event를 발행하고 다음 행이 그 Event를 구독한다.

행은 StartEvent/CommandTag 누락, PlacementId와 Group이 둘 다 비었거나 모두 설정, 음수 Delay일 때 오류로 제외된다.

## 10. Load Schedule PDA

파츠·시야 등 시작 전 준비가 필요한 개별 런타임 콘텐츠를 Definition PDA와 Asset Bundle로 연결하는 구현 규칙은 [비동기 로드 개발 가이드](Chimera_Async_Load_Developer_Guide.md)를 따른다. 배치 장애물은 Definition PDA 대상이 아니며 룸 서브레벨과 함께 스트리밍한다.

스테이지마다 `CMStageLoadSchedule` 하나를 만든다. LoadGroupId는 `<StageId>.<AreaOrPurpose>`를 권장한다.

```text
S01.Entry
S01.Hallway
S01.PowerRoom
S01.BranchA
```

LoadGroup은 같은 시점에 함께 준비하고 같은 수명 정책으로 관리할 Primary Asset 묶음이다. 장애물 인스턴스의 배치 순서가 아니다.

| 정책 | 사용 |
|---|---|
| `BeforeStageStart` | 첫 화면, 시작 연출, 초기 구역처럼 플레이 전 필수 |
| `Sequential` | 진행 순서에 따라 미리 준비할 후속 콘텐츠 |
| `OnDemand` | 선택 전에는 필요 없는 큰 분기·선택 콘텐츠 |

Schedule 시작 시 BeforeStageStart와 Sequential은 자동 큐에 들어간다. 서버 시작 배리어는 BeforeStageStart 완료까지만 기다리고 Sequential은 LoadOrder 순서로 계속 준비된다. OnDemand는 `PreloadGroupId`나 StageDirector 요청이 있을 때 로드한다.

- LoadOrder는 10, 20, 30처럼 간격을 둔다.
- 첫 구역은 반드시 BeforeStageStart다.
- 결과 연출은 마지막 순간에 로드하지 말고 BeforeStageStart 또는 충분히 앞선 Sequential에 둔다.
- Scope는 직접 입력하지 않는다.
- Catalog에 PDA를 GroupId로 배정하고 `RefreshAndRebuildCatalog`를 실행한다.
- 분기 탈락 후 해제할 그룹만 `ReleaseWhenBranchRejected`를 쓴다.
- 프로파일 근거 없이 작은 오브젝트마다 그룹을 만들지 않는다.

### 배치 장애물 로드와 밸런스

장애물은 해당 룸 서브레벨에 BP를 직접 배치하고 메시·머티리얼·Niagara·사운드를 BP 또는 인스턴스에서 참조한다. 룸 스트리밍이 액터와 하드 참조 에셋의 로드/언로드 경계다. 장애물마다 Definition, LoadGroupId, Schedule Catalog 항목을 만들지 않는다.

피해는 Damage Balance DataTable 행을 선택하고, 상태 종류·지속시간·강도는 장애물 인스턴스에서 설정한다. 스테이지 피해 배율은 `CMStageDirector`에서 한 번 선택한다. Custom Damage는 피해 행 없이 인스턴스 값만으로 사용할 수 있다. 상세 설정은 [장애물 밸런스 가이드](Chimera_Obstacle_Balance_Sheet.md), 컴포넌트 연결은 [장애물·버튼 가이드](Chimera_Obstacle_Mechanism_Architecture.md)를 따른다.

`CMObstacleDefinition`과 `CMObstacleDefinitionComponent`는 기존 BP 호환을 위해 코드가 남아 있지만 신규 제작 경로가 아니다. 에셋 참조를 먼저 마이그레이션하지 않고 타입을 삭제하면 기존 BP가 깨질 수 있으므로 즉시 삭제하지 않는다.

## 11. DataForge와 Sheet

DataForge는 선택적인 에디터 자동화다. 런타임은 DataForge 없이 수동 생성한 DataTable/PDA도 동일하게 사용한다.

```text
Chimera Stage Data
├─ Common_Tag
├─ S01_Sequence
├─ S01_LoadGroup
├─ S02_Sequence
└─ S02_LoadGroup
```

```text
Google Sheet 또는 CSV
→ DataForge Preview/Apply
→ DataTable / PDA
→ 런타임
```

Transform, Actor 경로와 복잡한 퍼즐 스크립트는 Sheet에 넣지 않는다. PlacementId, 논리 연결과 LoadGroup 정의처럼 검증하기 좋은 데이터만 관리한다. DataForge 생성 방식이 확정되기 전에는 수동 PDA가 기준이며, 생성 PDA도 StageRoute와 Catalog에 명시적으로 연결해야 한다.

## 12. 시작점과 목적지

`PlayerStart`는 공용 키메라 생성 지점이다. 레벨마다 하나만 배치하고 화살표의 +X 방향을 키메라가 바라볼 방향으로 맞춘다. 플레이어별 ControlBody는 별도 시작점을 요구하지 않으므로 여러 PlayerStart를 배치하지 않는다.

`CMStageDestination`은 목적지 판정 Volume을 소유한다. 제작자는 위치와 `DestinationVolume` 크기만 조정한다. 서버에서 공용 키메라의 모든 활성 BodySegment가 Volume과 동시에 Overlap하면 성공하며, 파츠·머리·ControlBody는 세지 않는다. 현재 판정은 각 마디의 완전한 기하학적 포함이 아니라 Overlap 기준이므로, 경계에서 오판하지 않도록 Volume에 충분한 여유를 둔다. 성공 시 StageDirector에 보고하며 목적지 액터가 직접 ServerTravel하지 않는다.

## 13. 테스트

개발자는 Route 중간 스테이지 맵을 직접 PIE할 수 있다. `BP_CMPlayGameMode.DefaultStageRouteDefinition`에 현재 Map이 등록되어 있어야 한다. 에디터 전용 부트스트랩이며 Shipping에서는 로비를 거친다.

반복 여부는 실행 방법이 아니라 `StageRouteDefinition.RouteMode`가 결정한다. 테스트 맵 전용 `TestRoute`를 만들고 `LoopCurrentStage`로 지정하면 직접 PIE와 로비 멀티플레이 모두 목적지 도착, 스테이지 실패 또는 전체 패배 확정 후 `LoopingStageRestartDelay`만큼 기다렸다가 현재 맵을 다시 연다. 키메라와 레벨 기믹은 초기화되고 `PlayerStart`에서 재시작한다. 개별 플레이어 마디 하나의 파괴는 기존 멀티플레이 규칙상 전체 패배가 아니므로 재시작 조건에 포함하지 않는다. 정식 Route는 `Normal`을 사용한다.

로비 GameMode의 `TestStageRouteDefinition`에 TestRoute를 연결하고 테스트 시작 UI에서 로컬 PlayerController의 `RequestStartTestStageRoute`를 호출한다. 모든 플레이어가 Ready이고 최소 인원을 만족해야 서버가 TestRoute의 첫 맵으로 함께 이동한다.

1. 1인 PIE로 배치와 기믹을 빠르게 확인한다.
2. 2인 리슨 서버 PIE로 복제와 로드 배리어를 확인한다.
3. Route에 맵 2개 이상을 넣고 Result 이후 ServerTravel을 확인한다.
4. 늦은 로드, 이탈과 로드 실패 로그를 확인한다.

1인 직접 PIE는 정상 지원한다. 로비의 정식 최소 인원 2명 정책과 별개다.

파츠 이동이 완성되기 전 목적지·기믹 동선을 시험할 때는 Non-Shipping 콘솔에서 `CM.DebugMove.Enable 1`을 실행한다. 위·아래 화살표는 공용 키메라를 전진·후진시키고 왼쪽·오른쪽 화살표는 몸통을 해당 방향으로 회전시킨다. `CM.DebugMove.Enable 0`으로 끈다. 이 기능은 물리 Force·Torque를 사용하며 Q/W/E/R 파츠 입력과 스태미나 규칙을 변경하지 않는다.

## 14. 완료 체크리스트

### 레벨과 요소

- StageDirector가 정확히 하나 있다.
- `PlayerStart`가 하나 있고 방향이 올바르다.
- `CMStageDestination`이 있고 모든 활성 몸통 마디가 동시에 Overlap할 수 있다.
- Director 개별 주소를 사용하는 요소만 PlacementId가 있으며, 지정한 ID는 중복되지 않는다.
- PuzzleController 직접 참조 대상은 빈 PlacementId여도 정상이다.
- 판정은 AuthorityOnly, 로컬 표현은 AllMachines다.
- 대상이 DataTable의 CommandTag를 처리한다.

### 데이터와 로드

- Route, Schedule과 Sequence의 StageId가 일관된다.
- 태그가 등록되어 있다.
- 한 Sequence 행은 PlacementId와 TargetGroup 중 하나만 가진다.
- PreloadGroupId가 Schedule에 존재한다.
- Scope를 수동 편집하지 않았다.
- 첫 구역이 BeforeStageStart다.
- 완료 의존성을 숫자 순서가 아니라 Event로 연결했다.

### 실행

- 직접 PIE에서 시작부터 완료까지 진행된다.
- 2인 리슨 서버에서 판정과 표현이 일치한다.
- 전환 후 이전 StageDirector와 상태가 남지 않는다.
- 로드 실패 시 동기 로드나 자동 진행 없이 Error가 발생한다.

## 15. 아직 자동화되지 않은 검증

- 레벨 전체 비어 있지 않은 PlacementId 중복 검사
- Sequence/직접 Trigger 대상에 필요한 PlacementId 누락 검사
- DataTable 대상과 실제 배치 비교
- 대상이 지원하지 않는 CommandTag 검사
- PreloadGroupId와 Schedule 교차 검사
- StageDirector 복수 배치 검사
- 목적지 Volume 크기 적절성 검사

자동 Validator가 추가되기 전까지 이 체크리스트를 레벨 리뷰 기준으로 사용한다.
