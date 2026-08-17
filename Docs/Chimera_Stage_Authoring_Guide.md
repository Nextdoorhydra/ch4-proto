# Chimera 레벨 디자인 및 스테이지 제작 규칙

## 1. 문서 목적

이 문서는 각 개발자가 스테이지 하나를 독립적으로 제작하더라도 같은 구조와 명명 규칙을 사용하기 위한 기준이다.

핵심 원칙은 다음과 같다.

- 액터 위치와 시각 결과는 레벨에서 제작한다.
- 장애물의 구체적인 동작은 장애물 C++ 또는 Blueprint가 담당한다.
- 실행 순서와 대상 연결은 스테이지 전용 DataTable이 담당한다.
- StageDirector는 등록, 명령 전달, 스테이지 완료 보고만 담당한다.
- 개별 배치 액터는 `PlacementId`, 의미와 분류는 Gameplay Tag로 표현한다.
- 런타임은 Google Sheet나 CSV를 직접 읽지 않고 DataTable과 PDA를 사용한다.

## 2. 책임 분리

```text
Level
  액터 Transform, Trigger Volume, Light, Fog, Post Process 배치

Obstacle / Puzzle Blueprint 또는 C++ Actor
  이동, 충돌, 데미지, 애니메이션, 내부 상태

CMStageElementComponent
  PlacementId와 GroupTag 등록
  StageDirector 명령 수신

CMStageSequenceComponent
  DataTable 조회
  Stage Event에 연결된 행을 순서대로 실행

CMStageDirector
  StageElement 등록소
  개별·그룹 명령 라우팅
  멀티플레이 명령 전달
  클리어·실패를 GameMode에 보고

PlayGameMode
  게임 Phase, 승패, 다음 스테이지 전환
```

StageDirector에 장애물별 이동 코드나 조명별 수치를 직접 추가하지 않는다.

## 3. 개발자별 작업 범위

각 개발자는 자신의 스테이지 폴더와 데이터만 수정한다.

```text
Content/Chimera/Stage/Stage01/
├─ Map/
├─ Blueprint/
├─ Data/
│  ├─ DT_CMStage01Sequence
│  └─ PDA_CMStage01LoadSchedule
├─ Lighting/
├─ Audio/
└─ Sequence/
```

권장 담당 범위:

```text
개발자 A → Stage01
개발자 B → Stage02
개발자 C → Stage03
개발자 D → Stage04
```

공용 장애물은 `Content/Chimera/Environment/Obstacle`에 둔다. 특정 스테이지에서만 사용하는 변형은 공용 Blueprint의 자식으로 만들어 해당 스테이지 폴더에 둔다.

## 4. 새 스테이지 제작 순서

### 4.1 기본 레벨 구성

1. 스테이지 전용 Map을 만든다.
2. 플레이 시작 지점과 목적지 지점을 배치한다.
3. `ACMStageDirector` 또는 그 Blueprint 자식을 정확히 하나 배치한다.
4. 시작 연출과 결과 연출을 연결한다.
5. 게임 진행용 Trigger를 배치한다.
6. 장애물, 문, 퍼즐, 조명을 배치한다.
7. 스테이지 전용 Sequence DataTable을 만든다.
8. StageDirector의 `StageSequence` 컴포넌트에 DataTable을 연결한다.
9. 스테이지 Load Schedule PDA를 만들고 캠페인 데이터에 연결한다.

### 4.2 권장 레벨 분리

작은 선형 맵은 다음과 같이 기능별 Sublevel로 나눌 수 있다.

```text
Stage01_Persistent
Stage01_Gameplay
Stage01_Lighting
Stage01_Audio
Stage01_Cinematic
```

큰 맵은 World Partition과 One File Per Actor 사용을 고려한다. 하나의 스테이지 안에서 World Partition 방식과 수동 Streaming Sublevel 방식을 임의로 혼용하지 않는다.

## 5. 식별자와 태그 구분

| 종류 | 자료형 | 역할 | 예시 |
|---|---|---|---|
| `PlacementId` | `FName` | 개별 배치 액터 식별 | `Hallway.Obstacle.Piston01` |
| `GroupTag` | Gameplay Tag | 여러 액터 동시 선택 | `Chimera.Stage.Group.Hallway.Obstacle` |
| `EventTag` | Gameplay Tag | 발생한 사건 표현 | `Chimera.Stage.Event.PowerRestored` |
| `CommandTag` | Gameplay Tag | 대상에게 내릴 명령 | `Chimera.Stage.Command.Activate` |
| `LoadGroupId` | `FName` | 함께 비동기 로드할 자산 묶음 | `Stage01.Hallway` |

개별 인스턴스 이름을 Gameplay Tag로 만들지 않는다.

## 6. PlacementId 규칙

PlacementId는 같은 스테이지 안에서 유일해야 한다.

형식:

```text
구역.종류.의미있는이름
```

권장 예시:

```text
TankRoom.Door.Entry
PowerRoom.Door.Exit
Hallway.Obstacle.Piston01
Hallway.Obstacle.Piston02
PowerRoom.Puzzle.MainPanel
Laboratory.Lighting.Main
Hallway.Trigger.Entry
StageExit.Trigger.Goal
```

규칙:

- 영문과 숫자를 사용한다.
- 공백과 한글을 사용하지 않는다.
- Outliner의 Actor Label이나 UObject 경로를 ID로 사용하지 않는다.
- `Actor01`, `Object03`처럼 의미 없는 이름을 사용하지 않는다.
- DataTable에서 사용하기 시작한 ID는 가급적 변경하지 않는다.
- 스테이지 번호는 넣지 않아도 된다. StageDirector 등록소가 스테이지별로 분리되어 있다.

잘못된 예:

```text
Actor01
문1
BP_Piston_C_0
Chimera.Stage.Obstacle.Piston01
```

## 7. Gameplay Tag 최상위 구조

```text
Chimera.Stage
├─ Event
├─ Command
└─ Group
```

세 분류의 역할을 섞지 않는다.

```text
Event   → 무슨 일이 발생했는가
Command → 대상에게 무엇을 시킬 것인가
Group   → 어떤 여러 대상을 함께 선택할 것인가
```

## 8. EventTag 규칙

형식:

```text
Chimera.Stage.Event.<사건>
```

공통 이벤트:

```text
Chimera.Stage.Event.Starting
Chimera.Stage.Event.Started
Chimera.Stage.Event.AreaEntered
Chimera.Stage.Event.AreaExited
Chimera.Stage.Event.TriggerEntered
Chimera.Stage.Event.TriggerExited
Chimera.Stage.Event.ObjectiveCompleted
Chimera.Stage.Event.PuzzleCompleted
Chimera.Stage.Event.BranchSelected
Chimera.Stage.Event.ExitReached
Chimera.Stage.Event.Failed
```

현재 구현은 `EventTag` 하나로 DataTable 행을 찾는다. 특정 진행 사건은 다음처럼 구체적인 태그를 사용할 수 있다.

```text
Chimera.Stage.Event.TankOpened
Chimera.Stage.Event.PowerRestored
Chimera.Stage.Event.HallwayEntered
Chimera.Stage.Event.HallwayCleared
```

새 이벤트를 만들기 전에 기존 공통 이벤트로 표현할 수 있는지 확인한다. 장소나 액터 하나마다 이벤트 태그를 무조건 만들지 않는다.

## 9. CommandTag 규칙

형식:

```text
Chimera.Stage.Command.<동작>
```

공통 명령:

```text
Chimera.Stage.Command.Activate
Chimera.Stage.Command.Deactivate
Chimera.Stage.Command.Enable
Chimera.Stage.Command.Disable
Chimera.Stage.Command.Open
Chimera.Stage.Command.Close
Chimera.Stage.Command.Lock
Chimera.Stage.Command.Unlock
Chimera.Stage.Command.Reset
```

조명 명령:

```text
Chimera.Stage.Command.Lighting.Default
Chimera.Stage.Command.Lighting.Alert
Chimera.Stage.Command.Lighting.Blackout
```

규칙:

- CommandTag에는 대상 이름을 넣지 않는다.
- `OpenTankRoomDoor` 대신 대상과 명령을 분리한다.
- 의미가 같은 동작에 새 태그를 중복 생성하지 않는다.

```text
TargetPlacementId = TankRoom.Door.Entry
CommandTag = Chimera.Stage.Command.Open
```

## 10. GroupTag 규칙

형식:

```text
Chimera.Stage.Group.<구역>.<종류>
```

예시:

```text
Chimera.Stage.Group.Hallway.Obstacle
Chimera.Stage.Group.PowerRoom.Obstacle
Chimera.Stage.Group.Laboratory.Door
Chimera.Stage.Group.Hallway.Lighting
Chimera.Stage.Group.Laboratory.Lighting
```

규칙:

- 여러 액터에 같은 명령을 보낼 때만 사용한다.
- 개별 액터 하나를 위한 GroupTag를 만들지 않는다.
- 같은 의미의 그룹은 다른 스테이지에서도 태그를 재사용할 수 있다.
- DataTable 한 행에는 `TargetPlacementId`와 `TargetGroup` 중 하나만 설정한다.

잘못된 예:

```text
Chimera.Stage.Group.Hallway.Obstacle.Piston01
```

이 경우에는 다음 PlacementId를 사용한다.

```text
Hallway.Obstacle.Piston01
```

## 11. 태그 등록과 변경 규칙

고정 태그 후보, CSV 확장 범위와 승인 절차는 [Chimera 스테이지 Gameplay Tag 카탈로그](Chimera_Stage_Tag_Catalog.md)를 기준으로 한다.

프로젝트 전체에서 공통으로 사용하는 Event와 Command는 Native Gameplay Tag로 관리하는 것을 권장한다.

레벨 제작자가 새 태그를 추가할 때는 다음 절차를 따른다.

1. Google Sheet의 `Common_Tag` 또는 프로젝트 태그 목록에서 같은 의미의 태그를 검색한다.
2. 기존 태그로 표현할 수 없다면 새 이름을 제안한다.
3. Event, Command, Group 중 정확한 분류를 선택한다.
4. 공통 Event와 Command는 코드 담당자가 Native Tag로 등록한다.
5. 레벨 그룹 태그는 Gameplay Tag 설정에 등록한다.
6. 태그를 등록한 후에 DataTable과 레벨 컴포넌트에서 사용한다.

태그 이름을 변경할 때 DataTable 문자열만 수정하면 안 된다. C++, Blueprint, Gameplay Tag 설정, Google Sheet 참조를 함께 확인한다.

## 12. 장애물 배치 규칙

### Blueprint 장애물

1. 장애물 Blueprint에 `CMStageElementComponent`를 추가한다.
2. 레벨에 배치한다.
3. 배치된 인스턴스에 고유 PlacementId를 지정한다.
4. 필요하면 GroupTags를 지정한다.
5. 충돌이나 게임 판정이 있으면 `AuthorityOnly`를 사용한다.
6. `ExecuteStageCommand` 이벤트에서 지원할 CommandTag를 처리한다.
7. 동작 완료가 다음 단계 조건이면 `BroadcastStageEvent`를 서버에서 호출한다.

### C++ 장애물

C++ Actor에서도 같은 규칙을 사용한다. `UCMStageElementComponent`의 자식 컴포넌트를 만들고 `ExecuteStageCommand_Implementation`을 재정의한다.

장애물의 이동 속도, 데미지, 반복 횟수, 이동 Curve는 장애물 클래스 또는 장애물 전용 데이터가 소유한다. Stage Sequence DataTable에는 넣지 않는다.

## 13. 조명 배치 규칙

1. 조명 그룹 관리 Actor를 레벨에 배치한다.
2. `CMStageLightingComponent`를 추가한다.
3. PlacementId를 `<Area>.Lighting.<Name>` 형식으로 지정한다.
4. 제어할 Light, Fog, Post Process Volume을 등록한다.
5. `Presets`에 조명 CommandTag별 상태를 추가한다.
6. 기본 조명을 맞춘 뒤 `Capture Current Lighting`을 누른다.
7. `PreviewCommandTag`를 선택하고 `Preview Selected Preset`으로 확인한다.
8. 확인 후 `Restore Captured Lighting`으로 기준 상태를 복원한다.
9. DataTable에서 PlacementId 또는 GroupTag를 대상으로 조명 CommandTag를 보낸다.

조명은 모든 클라이언트에서 표현되어야 하므로 기본 실행 정책은 `AllMachines`를 사용한다.

### 13.1 나이아가라 이펙트 배치 규칙

1. 이펙트 관리 Actor에 Niagara Component를 배치한다.
2. `CMStageEffectComponent`를 추가한다.
3. PlacementId를 `<Area>.Effect.<Name>` 형식으로 지정한다.
4. 함께 제어할 Niagara Component를 `Effects` 배열에 등록한다.
5. Niagara Component의 `Auto Activate`는 시작부터 보여야 하는 경우가 아니면 끈다.
6. `PreviewAction`을 선택하고 `Preview Selected Action`으로 에디터에서 확인한다.
7. DataTable에서 PlacementId 또는 GroupTag를 대상으로 Effect Command를 보낸다.

지원하는 명령:

```text
Chimera.Stage.Command.Effect.Activate
Chimera.Stage.Command.Effect.Deactivate
Chimera.Stage.Command.Effect.Restart
Chimera.Stage.Command.Effect.Burst
```

- `Activate`: 현재 상태에서 지속형 이펙트 활성화
- `Deactivate`: 자연스러운 종료 요청
- `Restart`: 시스템을 초기화하고 다시 실행
- `Burst`: 즉시 정지한 뒤 처음부터 일회성 실행

예시:

```text
PlacementId = Hallway.Effect.Steam
GroupTag = Chimera.Stage.Group.Hallway.Effect
ExecutionPolicy = AllMachines
```

이펙트는 모든 클라이언트에서 보여야 하므로 `CMStageEffectComponent`는 기본적으로 `AllMachines`를 사용한다. Niagara의 Spawn Rate, Color, User Parameter 같은 시각 수치를 Stage Sequence DataTable에 넣지 않는다. 해당 수치는 Niagara System 또는 이펙트 전용 Blueprint에서 조정한다.

## 14. DataTable 작성 규칙

Row Struct는 `FCMStageSequenceRow`를 사용한다.

| 열 | 역할 | 규칙 |
|---|---|---|
| `StageId` | 스테이지 식별 | `S01`, `S02` 형식 |
| `StepOrder` | 큰 진행 단계 순서 | 10 단위 권장 |
| `StepId` | 단계 의미 이름 | 변경에 강한 이름 사용 |
| `ActionOrder` | 같은 이벤트 안의 호출 순서 | 10 단위 권장 |
| `StartEvent` | 행 실행 조건 | 등록된 EventTag 사용 |
| `TargetPlacementId` | 개별 대상 | TargetGroup과 동시 사용 금지 |
| `TargetGroup` | 복수 대상 | PlacementId와 동시 사용 금지 |
| `CommandTag` | 실행 명령 | 대상이 지원하는 명령 사용 |
| `DelaySeconds` | 실행 지연 | 0 이상 |
| `PreloadGroupId` | OnDemand 로드 요청 | Schedule에 존재하는 ID 사용 |

`StepOrder`는 이전 단계의 애니메이션 완료를 기다리는 기능이 아니다. 완료를 기다려야 한다면 액터가 완료 Event를 발행하고 다음 행이 그 Event를 `StartEvent`로 사용한다.

```text
PowerRestored 이벤트
  → 조명 Alert 명령
  → 출구 Unlock 명령

문 열림 완료 이벤트
  → 다음 퍼즐 Activate 명령
```

## 15. Google Sheet 구성 규칙

프로젝트 Google Sheet 파일 하나에 스테이지별 탭을 분리하는 것을 기본으로 한다.

```text
Chimera Stage Data
├─ Common_Tag
├─ S01_Sequence
├─ S01_LoadGroup
├─ S02_Sequence
├─ S02_LoadGroup
├─ S03_Sequence
├─ S03_LoadGroup
├─ S04_Sequence
└─ S04_LoadGroup
```

각 개발자는 자신의 스테이지 탭만 수정한다. Google Sheet는 원본 편집 데이터이며 런타임 데이터가 아니다.

```text
Google Sheet
→ DataForge Preview/Apply
→ DataTable 또는 PDA
→ StageDirector와 StageLoadCoordinator
```

## 16. 비동기 로드 연결 규칙

Stage Load Schedule의 정책은 다음 세 가지다.

```text
BeforeStageStart
  시작 전에 반드시 준비할 콘텐츠

Sequential
  게임 시작 후 LoadOrder 순서로 미리 준비할 콘텐츠

OnDemand
  분기나 특정 기믹에서 요청할 콘텐츠
```

DataTable의 `PreloadGroupId`가 지정된 행은 각 머신에서 해당 OnDemand 그룹이 준비될 때까지 명령 실행을 보류한다. 로드에 실패하거나 준비 직후 해제된 경우 명령은 실행하지 않고 오류 로그를 남긴다.

## 17. 멀티플레이 실행 정책

`AuthorityOnly`를 사용하는 대상:

- 충돌 판정
- 데미지 적용
- 문 잠금 상태
- 퍼즐 진행 상태
- 클리어 조건
- 물리 게임플레이 결과

`AllMachines`를 사용하는 대상:

- 조명
- 로컬 사운드
- 파티클
- 화면 연출
- 게임 결과에 영향을 주지 않는 시각 효과

AuthorityOnly 액터의 최종 상태와 표현은 해당 액터가 Replication 또는 RepNotify로 동기화한다.

## 18. 레벨 완료 체크리스트

### 레벨 구조

- StageDirector가 정확히 하나 배치되어 있다.
- 시작 지점과 목적지 지점이 있다.
- 시작 및 결과 연출이 연결되어 있다.
- 스테이지 전용 DataTable과 Load Schedule이 연결되어 있다.

### 배치 액터

- 모든 StageElement의 PlacementId가 비어 있지 않다.
- 같은 스테이지 안에 중복 PlacementId가 없다.
- 개별 대상에 불필요한 GroupTag를 만들지 않았다.
- 게임 판정 요소는 AuthorityOnly다.
- 조명과 로컬 연출은 AllMachines다.

### 태그와 데이터

- 모든 Event, Command, Group 태그가 프로젝트에 등록되어 있다.
- 기존 태그와 의미가 중복되는 새 태그가 없다.
- DataTable의 PlacementId가 실제 레벨에 존재한다.
- DataTable 한 행에 PlacementId와 GroupTag를 동시에 넣지 않았다.
- 대상 액터가 CommandTag를 지원한다.
- PreloadGroupId가 Load Schedule에 존재한다.
- 완료 의존성은 숫자 순서가 아니라 Stage Event로 연결되어 있다.

### 테스트

- Standalone에서 시작부터 클리어까지 진행된다.
- 리슨 서버 2인 이상 PIE에서 동일하게 진행된다.
- 장애물 충돌과 데미지가 서버 기준으로 동일하다.
- 조명과 연출이 모든 클라이언트에서 동일하게 보인다.
- 중도 이탈 또는 늦은 로드 상황에서 진행이 잘못 열리지 않는다.
- 스테이지 전환 후 이전 스테이지 자산과 상태가 남지 않는다.

## 19. 아직 추가해야 할 에디터 지원

다음 항목은 규칙은 정해졌지만 자동 검증 기능은 아직 구현되지 않았다.

- PlacementId 중복·누락 Data Validation
- DataTable 대상과 실제 레벨 배치 비교
- 대상이 지원하지 않는 CommandTag 검사
- 등록되지 않은 Gameplay Tag 검사
- PreloadGroupId와 Load Schedule 비교
- StageDirector 복수 배치 검사
- OnDemand Activation Gate

자동 검증이 추가되기 전까지는 이 문서의 체크리스트를 코드 리뷰와 레벨 리뷰 기준으로 사용한다.
