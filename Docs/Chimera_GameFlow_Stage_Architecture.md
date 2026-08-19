# Chimera 게임 흐름·스테이지 런타임 구조

## 1. 목적과 기준

이 문서는 로비부터 엔딩까지의 서버 권한 흐름과 스테이지 런타임 책임을 정의한다. 현재 구현과 다음 구현을 구분하며, 레벨 제작 방법은 [스테이지 제작 가이드](Chimera_Stage_Authoring_Guide.md)를 따른다.

핵심 원칙은 다음과 같다.

- `GameMode`는 서버의 규칙과 상태 전환을 결정한다.
- `GameState`는 클라이언트가 알아야 할 결과만 복제한다.
- `StageRoute`는 맵 순서와 스테이지별 Load Schedule을 보관한다.
- `StageDirector`는 현재 레벨의 요소를 연결하고 완료·실패를 보고한다.
- 장애물, 조명, 이펙트의 구체 동작은 각 컴포넌트가 담당한다.
- 비동기 로드 실패는 동기 로드로 우회하지 않고 오류 상태와 로그를 남긴 채 흐름을 중단한다.

## 2. 런타임 구성

```text
LobbyGameMode
  Ready 집계와 시작 검증
  StageRoute 시작
  첫 맵 ServerTravel
        │
        ▼
StageRouteSubsystem (GameInstance)
  현재·대기 스테이지 인덱스 유지
        │
        ▼
PlayGameMode (서버)
  Phase, 로드 배리어, 승패, ServerTravel 결정
        │ 복제
        ▼
PlayGameState
  Phase, 스테이지 인덱스, 연출 상태, 로드 Snapshot
        │ 각 로컬 PlayerController
        ▼
ClientStageLoadComponent
  복제 요청을 로컬 Coordinator에 전달하고 서버에 결과 보고
        │
        ▼
StageLoadCoordinatorSubsystem (각 머신)
  Schedule PDA 준비, 자동 그룹 큐, OnDemand 그룹, 해제
        │
        ▼
AsyncPDALoader 플러그인
  실제 Primary Asset 비동기 로드·언로드
```

레벨 내부 흐름은 별도로 다음처럼 연결된다.

```text
Trigger / Puzzle / Obstacle
  Stage Event 발생
        ▼
StageSequenceComponent
  DataTable 행 선택
        ▼
StageDirector
  PlacementId 또는 GroupTag 대상에 Command 전달
        ▼
StageElementComponent 파생 컴포넌트
  장애물·조명·이펙트 동작
```

`GameplayMessageSubsystem`은 로컬 메시지 버스다. 네트워크 복제를 대신하지 않는다. 서버 결정은 GameState 복제, RPC 또는 StageDirector의 명시적인 NetMulticast 경계를 거친다.

## 3. GameMode와 GameState 책임

### CMGameMode / CMGameState

공통 기반은 플레이어 색상, 공용 키메라 생성, 개인 `ControlBody` 빙의, 조작 슬롯 재배정을 담당한다. `SharedChimera` 참조와 참가자 명단은 `CMGameState`가 제공한다.

플레이어는 공용 키메라를 Possess하지 않는다. 각 플레이어가 자신의 `ControlBody`를 Possess하고 서버 RPC의 소유권 통로로 사용하며 카메라는 공용 키메라를 본다.

### CMLobbyGameMode / CMLobbyGameState

- 참가·퇴장 시 Ready 집계를 갱신한다.
- 최소 인원 이상이며 전원이 Ready일 때 시작할 수 있다.
- 현재 구현의 기본 최소 인원은 2명이다.
- 시작 요청 시 `StageRouteDefinition`을 활성화하고 첫 스테이지 맵으로 이동한다.

### CMPlayGameMode

- 현재 `StageDirector` 하나를 등록한다.
- 스테이지 Load Schedule 요청과 전원 완료 배리어를 관리한다.
- 시작 연출, 실제 플레이, 결과 연출 Phase를 전환한다.
- StageDirector의 완료·실패 보고를 검증한다.
- 다음 스테이지를 예약하고 Seamless ServerTravel을 실행한다.
- 모든 활성 몸통 마디 사망을 전체 `Defeat`로 연결한다.
- 로드 실패 시 자동 진행하지 않는다.

개별 장애물 상태, 조명 수치, 퍼즐 내부 로직은 PlayGameMode에 넣지 않는다.

### CMPlayGameState

현재 복제하는 값은 다음과 같다.

- `PlayPhase`
- `CurrentStageIndex`, `TotalStageCount`
- Phase 시작 서버 시간과 제한 시간
- `StagePresentationState`
- `StageLoadSnapshot`: RequestId, ScheduleId, 상태, 실패 이유, 준비 인원, 제한 시간

개별 플레이어의 Ready, 색상, 고정 `PlayerSlotId`, 참가 상태는 `PlayerState` 책임이다. `PlayerSlotId`는 입장 순서대로 중복 없이 배정되고 Travel 경계에서도 유지된다. 참가 상태는 현재 `Lobby`, `Active`, `Defeated`, `Disconnected` 흐름에 연결되어 있으며 `Spectating`, `WaitingNextStage`는 중도 합류 정책 구현 때 사용한다.

Q/W/E/R의 실제 `ControlSlots`와 담당 `OwnedSegmentIndex`는 현재 월드의 입력 통로인 `ControlBody`가 소유한다. 공용 스태미나와 신체 능력 ASC는 `SharedChimera`가 소유한다. 이 값을 `PlayerState`나 `GameState`에 중복 저장하지 않는다.

## 4. 플레이 상태 전환

```text
Loading
  → WaitingForPlayers
  → Starting
  → Playing
  → Completed
  → 다음 스테이지 Loading

마지막 스테이지:
Playing → Completed → Result 연출 → Victory → Ending

실패 흐름:
Playing → Failed     현재 스테이지 실패, 재시도·포기 대기
Playing → Defeat     전체 게임 패배 확정
Victory/Defeat → Ending
```

`Starting`은 수조에서 깨어나는 연출처럼 플레이 시작 전 연출을 위한 상태다. 기본 구현은 30초 안에 StageDirector가 완료를 보고하지 않으면 Error 로그를 남기며 자동으로 `Playing`으로 넘어가지 않는다.

`Failed`는 현재 스테이지를 다시 시도할 수 있는 실패이고, `Defeat`는 게임 전체 종료가 확정된 상태다.

## 5. StageRoute와 맵 전환

`UCMStageRouteDefinition`은 순서가 있는 배열이며 각 행은 다음 값을 가진다.

| 값 | 역할 |
|---|---|
| `StageId` | 로그와 데이터에서 사용하는 안정적인 ID |
| `StageMap` | 서버가 이동할 월드 Soft Reference |
| `LoadScheduleId` | 해당 스테이지의 `CMStageLoadSchedule` Primary Asset ID |

배열 순서를 바꾸면 스테이지 순서가 바뀌고, 행을 추가하면 스테이지를 확장할 수 있다. 저장 검증은 빈 값, 중복 StageId·Map, Asset Manager에 등록되지 않은 Schedule을 오류로 보고한다.

정식 흐름은 로비가 Route를 시작한다. 에디터에서 플레이 맵을 직접 실행할 때는 `DefaultStageRouteDefinition`에서 현재 Map을 찾아 해당 인덱스로 시작한다. 이 직접 시작은 `WITH_EDITOR`에서만 허용된다.

스테이지 완료 후에는 `PrepareNextStage`로 다음 인덱스를 예약하고 ServerTravel한다. 새 월드의 PlayGameMode가 현재 Map과 예약 Map을 비교한 뒤 `CommitPendingStage`한다.

## 6. 비동기 로드 구조

### Schedule 단위

스테이지마다 `UCMStageLoadSchedule` PDA 하나를 만든다. `StageRouteDefinition`의 각 행이 자신의 Schedule을 참조한다. Schedule이 비어 있는 것은 초기 프로토타입 테스트에는 가능하지만 Route 데이터 검증을 통과하려면 유효한 Schedule Primary Asset이 필요하다.

GameMode의 `InitialStageLoadScheduleId`와 `StageLoadScheduleIds`는 Route를 사용하지 않는 이전·보조 fallback이다. 정상 제작에서는 `StageRouteDefinition.LoadScheduleId`를 기준으로 한다.

### LoadGroup 정책

TimingTag를 `Entry`, `Background`, `Result`로 나누지 않는다. 모든 스테이지 그룹은 내부적으로 `Chimera.Load.Stage`를 사용하고, 요청 시점은 `LoadPolicy`가 결정한다.

| 정책 | 동작 | 사용 예 |
|---|---|---|
| `BeforeStageStart` | 플레이 시작 전에 반드시 완료되어야 하며 서버 배리어가 기다림 | 초기 구역, 시작 연출, 즉시 필요한 파츠·기믹 |
| `Sequential` | Schedule 시작과 함께 `LoadOrder` 순서로 백그라운드 준비 | 선형 진행 후반부 장애물·퍼즐·표현 |
| `OnDemand` | 자동 큐에서 제외되고 분기·기믹이 명시적으로 요청 | 큰 비가역 분기, 선택 콘텐츠 |

자동 그룹 큐는 `BeforeStageStart`를 먼저, 이후 `Sequential`을 `LoadOrder`, `LoadGroupId` 순으로 한 번에 하나씩 요청한다. 클라이언트는 모든 `BeforeStageStart` 그룹이 끝나면 서버에 준비 완료를 보고한다. Sequential 큐는 이후에도 계속 진행할 수 있다.

첫 구역 콘텐츠는 반드시 `BeforeStageStart`로 둔다. 이후 콘텐츠는 순서가 확실하면 10, 20, 30처럼 간격을 둔 `LoadOrder`를 사용한다. 단순히 맵의 중반·후반이라는 이름으로 그룹을 나누지 않는다.

### Scope

`Scope`는 AsyncPDALoader가 그룹을 조회하는 내부 정수 키다. 개발자가 직접 입력하지 않는다. `RefreshAndRebuildCatalog`가 `LoadGroupId`를 안정적으로 정렬해 Scope를 만들고 Catalog 항목의 Scope와 Timing을 동기화한다.

### 분기와 해제

- 작은 분기나 재방문 가능 구간은 양쪽을 Sequential로 미리 로드하고 스테이지 종료까지 유지한다.
- 큰 비가역 분기는 OnDemand로 요청할 수 있다.
- 선택되지 않은 그룹은 `ReleaseWhenBranchRejected` 정책일 때만 해제한다.
- 재진입 불가능 구역은 명시적인 구역 이탈 이후 `ReleaseAfterExit` 정책을 사용할 수 있다.
- 실제 프로파일링 근거 없이 그룹을 지나치게 잘게 나누지 않는다.

### 실패 정책

클라이언트 실패 보고나 타임아웃은 `StageLoadSnapshot`에 `Failed` 또는 `TimedOut`으로 기록한다. GameMode는 요청 큐를 정리하고 `Loading` 흐름을 중단한다. 동기 강제 로드, 누락 상태로 진행, 자동 다음 스테이지 이동은 하지 않는다.

## 7. StageDirector 책임

StageDirector는 스테이지마다 정확히 하나 배치한다.

담당:

- StageElement의 PlacementId 등록과 중복 검사
- PlacementId 또는 GroupTag 대상 명령 라우팅
- 필요한 경우 모든 머신에 표현 명령 전달
- Stage Event와 Sequence DataTable 연결
- OnDemand LoadGroup 준비 후 보류 명령 실행
- 시작·결과 연출 완료 보고
- 스테이지 완료·실패 보고

담당하지 않음:

- 장애물별 이동 공식과 데미지
- 퍼즐 내부 상태 기계
- 조명·나이아가라의 개별 수치
- 전체 게임 승패 결정
- 맵 전환 순서 소유

## 8. 목적지 완료 계약

목적지 액터는 아래 경계로 구현되어 있다.

```text
StageDestination
  모든 활성 몸통 마디가 Volume 안에 있는지 서버에서 판정
        ▼
StageDirector::CompleteStage()
        ▼
PlayGameMode::HandleStageCompleted()
        ▼
Completed → Result → 다음 Stage 또는 Victory
```

파츠나 ControlBody가 아니라 공용 키메라의 활성 `BodySegment` 전부를 판정한다. 목적지 액터가 StageRoute, GameState 또는 ServerTravel을 직접 조작하지 않는다.

PlayGameMode는 레벨의 첫 `PlayerStart` Transform에 공용 키메라를 생성한다. 플레이 맵에는 PlayerStart를 하나만 배치한다. `CMStageDestination`은 서버에서 각 활성 몸통 마디와 목적지 Volume의 동시 Overlap을 확인한 뒤 StageDirector에 완료를 보고한다.

## 9. 현재 구현과 남은 작업

현재 구현됨:

- 로비 Ready와 StageRoute 시작
- 공용 키메라·ControlBody 생성 및 배정
- 직접 스테이지 PIE 부트스트랩
- 스테이지별 Schedule 요청과 멀티플레이 배리어
- `BeforeStageStart`, `Sequential`, `OnDemand` 그룹
- StageDirector 등록·명령·연출·완료·실패 경계
- 마지막 스테이지 Result 이후 Victory 전환
- 시작 연출 타임아웃 오류 보고
- 전체 몸통 마디 사망 Defeat 연결
- 스테이지 시작점과 공용 키메라 생성 위치 연결
- 목적지와 전 활성 몸통 마디 Overlap 완료 판정

다음 구현:

- 낙사와 활력 0 등 추가 패배 조건 집계
- Failed 재시도·게임 포기 UI 흐름
- 스테이지별 캐릭터 모습 기록과 Ending 표시
- PlacementId·DataTable·LoadGroup 교차 Validator
- 실제 2인 리슨 서버와 다중 맵 ServerTravel 통합 테스트
