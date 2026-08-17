# Chimera 비동기 로딩·스테이지 기믹 데이터 설계

## 1. 목적

이 문서는 NetKarma에서 범용화한 `AsyncPDALoader`를 Chimera에 적용하는 기준과 스테이지 내 장애물·퍼즐·분기의 데이터 작성 방식을 정의한다.

핵심 원칙은 다음과 같다.

- `AsyncPDALoader`는 게임 규칙을 모르는 범용 실행기로 유지한다.
- Chimera는 `StageLoadSchedule`과 `StageLoadCoordinator`에서 로드 순서와 보존 정책을 결정한다.
- TimingTag는 게임 생명주기만 나타내고 실제 등장 순서는 `LoadOrder`가 나타낸다.
- 레벨은 공간 배치의 원본이고 CSV는 논리 순서의 원본이다.
- 런타임은 원본 CSV를 읽지 않고 DataForge가 만든 DataTable/PDA를 사용한다.
- 서버가 로드 시점을 결정하지만 실제 로드는 서버와 각 클라이언트가 자기 머신에서 실행한다.

## 2. 전체 구조

```text
PlayGameMode                         서버 권한 흐름과 로드 배리어 결정
    ↓ 복제할 Load Request
PlayGameState 또는 전용 복제 상태   StageId, Timing, RequestToken 공유
    ↓ 각 머신
UCMStageLoadCoordinatorSubsystem     LoadGroup·LoadOrder·분기 정책 처리
    ↓ Scope + TimingTag
UAsyncPDALoader                      PDA 요청 병합·캐시·완료 결과 처리
    ↓
UAssetManager                        Primary Asset와 Asset Bundle 실제 로드
```

`GameplayMessageSubsystem` 메시지는 네트워크 RPC가 아니다. 서버에서 플러그인 요청 메시지를 방송해도 원격 클라이언트에는 전달되지 않는다. 멀티플레이 연결 단계에서는 GameState의 복제 요청을 각 머신의 Coordinator가 로컬 플러그인 요청으로 변환해야 한다.

## 3. 책임 분리

### PlayGameMode

- 현재 Phase 전환 결정
- 어떤 TimingTag 큐를 시작할지 결정
- 클라이언트별 필수 로드 완료 배리어 관리
- 로드 timeout과 실패 정책 결정
- 스테이지 전환 `ServerTravel` 실행

### PlayGameState

- 현재 Stage와 Phase 복제
- 현재 필수 로드 요청의 `RequestToken`, TimingTag 복제
- 로드 핸들이 아닌 게임 진행에 필요한 결과 상태만 복제

### StageDirector

- 레벨에 배치된 기믹 등록
- CSV/DataTable 기반 기믹 순서 실행
- 시작·결과 연출 실행
- 분기 선택, ActivationGate 도달, 클리어·실패를 GameMode에 보고
- 로더를 직접 소유하거나 네트워크 완료 인원을 판단하지 않음

### StageLoadCoordinator

- 각 머신에 하나 존재하는 `GameInstanceSubsystem`
- 현재 Stage Schedule을 범용 로더 Provider로 연결
- 같은 TimingTag 그룹을 `LoadOrder` 순서로 요청
- 개별 LoadGroup 준비 상태 관리
- 선택되지 않은 분기 그룹 억제와 안전한 해제
- `CorrelationId`로 자기 요청 완료만 처리

### AsyncPDALoader 플러그인

- `Scope + TimingTag`를 PrimaryAssetId 목록으로 해석
- Asset Bundle이 같은 중복 요청 병합
- 비동기 로드·캐시·취소 generation 관리
- 성공·부분 성공·실패·취소 결과 방송
- Chimera의 Stage, Phase, 분기, GameMode를 참조하지 않음

## 4. TimingTag

```text
Chimera.Load.Session
Chimera.Load.Stage.Entry
Chimera.Load.Stage.Background
Chimera.Load.Stage.Result
Chimera.Load.Ending
```

| TimingTag | 완료 마감 시점 | 대표 데이터 |
|---|---|---|
| `Session` | 캠페인 시작 전 | 공통 파츠·UI·사운드·Ability 정의 |
| `Stage.Entry` | 로딩 화면 종료 전 | 시작 연출, 초기 구역, 첫 기믹 |
| `Stage.Background` | 해당 그룹 실제 사용 전 | 이후 장애물·퍼즐·생명체·구역 표현 |
| `Stage.Result` | 클리어·실패 결과 연출 전 | 결과 Sequence·음악·촬영 연출 |
| `Ending` | Ending Phase 진입 전 | 캐릭터 기록 화면·엔딩 크레딧 |

`Early`, `Middle`, `Late`는 사용하지 않는다. 맵 크기와 분기 구조가 바뀌면 의미가 깨지기 때문이다. `Stage.Background` 안의 실제 우선순위는 `LoadOrder`가 담당한다.

## 5. LoadGroup과 순차 큐

한 LoadGroup은 함께 준비할 PDA 묶음이다. 장애물 하나마다 그룹을 만들지 않고 실제 플레이 구간이나 함께 등장하는 기믹 단위로 묶는다.

```text
Stage01.Entry             LoadOrder 0
Stage01.OpeningArea       LoadOrder 10
Stage01.PowerRoom         LoadOrder 20
Stage01.BranchA.Entry     LoadOrder 30
Stage01.BranchB.Entry     LoadOrder 31
Stage01.ExitArea          LoadOrder 40
Stage01.Result            LoadOrder 50
```

권장 개수는 스테이지당 Entry 1~2개, Background 3~6개, Result 1개 정도다. 실제 프로파일링 결과가 나오기 전에는 더 잘게 쪼개지 않는다.

### 시작 흐름

```text
로딩 화면
    Session 큐 완료
    Stage.Entry 큐 완료
    맵과 StageDirector 초기화 완료
    모든 필수 참가자 준비 완료
        ↓
Starting 시작
    Stage.Background 순차 큐 시작
        ↓
Playing
    큐는 LoadOrder 순서로 계속 진행
    실제 사용 직전 ActivationGate가 Ready를 최종 확인
```

큐는 한 그룹이 완료된 후 다음 그룹을 요청한다. 디스크 요청 집중을 줄이고 앞쪽 필수 콘텐츠가 뒤쪽 콘텐츠에 밀리지 않게 하기 위한 기본 정책이다.

## 6. 갈림길

### 작은 분기 또는 재방문 가능한 분기

두 경로를 같은 `Stage.Background` 타이밍으로 로드하고 스테이지 종료까지 유지한다.

```text
BranchA LoadOrder 30
BranchB LoadOrder 31
RetentionPolicy = KeepUntilStageEnd
```

### 큰 비가역 분기

선택 전에는 양쪽의 입구 그룹만 로드한다. 서버에서 선택이 확정되고 되돌릴 수 없게 된 뒤 선택되지 않은 그룹을 거부한다.

```text
BranchA.Entry LoadOrder 30
BranchB.Entry LoadOrder 31
RetentionPolicy = ReleaseWhenBranchRejected
```

선택되지 않은 그룹 처리:

- 아직 큐에 있으면 이후 요청 목록에서 제거한다.
- 이미 준비됐으면 즉시 해제한다.
- 로드 중이면 `PendingRelease`로 표시하고 완료 메시지를 받은 직후 해제한다.
- `BeginNewSession()`은 다른 모든 요청도 취소하므로 분기 하나를 취소하는 용도로 사용하지 않는다.

공유 Mesh·Sound 등은 각 Primary Asset의 Asset Bundle 참조를 통해 AssetManager가 residency를 유지한다. 배치 Blueprint가 같은 리소스를 하드 참조하면 PDA를 해제해도 메모리에서 내려가지 않으므로 무거운 런타임 콘텐츠는 soft reference여야 한다.

## 7. ActivationGate

순차 프리로드는 평상시 대기 시간을 숨기기 위한 최적화다. 느린 PC나 예상보다 빠른 플레이를 위해 실제 사용 직전에는 준비 상태를 다시 확인한다.

```text
플레이어가 Laboratory 문 도착
    ↓
Stage01.Laboratory Ready?
    Yes → 문 개방 및 기믹 활성화
    No  → 문·엘리베이터·암전 연출 유지
          로드 완료 후 개방
```

Gate timeout과 실패 시 재시도·타이틀 복귀 정책은 권한이 있는 GameMode가 결정한다. Coordinator는 로컬 결과만 제공한다.

## 8. 레벨 배치와 CSV

### 레벨이 소유하는 데이터

- 액터 Transform
- 기믹 Blueprint 클래스와 인스턴스 설정
- Trigger·Volume 위치
- 라이트·포스트프로세스·Sequence 액터
- World Partition/Data Layer 또는 Streaming Level 배치

### CSV가 소유하는 데이터

- 기믹 Step과 Action 순서
- 시작 신호와 완료 신호
- 대상 PlacementId 또는 GroupTag
- 실행 CommandTag
- Delay와 클리어 필수 여부
- 해당 Step 전에 요청할 LoadGroupId

CSV에 Transform, Actor 경로, 복잡한 분기 스크립트는 넣지 않는다.

### 배치 액터 식별

```text
PlacementId = ExitArea.Door.ExitA    개별 인스턴스 식별 FName
GroupTag = Chimera.Stage.Group.Door  여러 액터 대상 GameplayTag
```

개별 액터마다 GameplayTag를 만들지 않는다. 사람이 관리하는 안정적인 `PlacementId`를 사용하고 에디터 Validator가 레벨 내 중복과 CSV의 누락 참조를 검사한다.

## 9. CSV 예시

### 로드 그룹 데이터

```csv
RecordId,StageId,LoadGroupId,Scope,LoadOrder,TimingTag,Priority,RetentionPolicy
S01_000,S01,Stage01.Entry,0,0,Chimera.Load.Stage.Entry,100,KeepUntilStageEnd
S01_020,S01,Stage01.PowerRoom,20,20,Chimera.Load.Stage.Background,50,KeepUntilStageEnd
S01_030,S01,Stage01.BranchA.Entry,30,30,Chimera.Load.Stage.Background,40,ReleaseWhenBranchRejected
S01_031,S01,Stage01.BranchB.Entry,31,31,Chimera.Load.Stage.Background,40,ReleaseWhenBranchRejected
S01_050,S01,Stage01.Result,50,50,Chimera.Load.Stage.Result,10,KeepUntilStageEnd
```

### 기믹 순서 데이터

```csv
RecordId,StageId,StepOrder,StepId,ActionOrder,StartSignal,TargetPlacementId,TargetGroup,Command,CompletionSignal,DelaySeconds,RequiredForClear,PreloadGroupId
S01_010_010,S01,10,WakeTank,10,Chimera.Stage.Event.Starting,,Chimera.Stage.Group.Tank,Chimera.Stage.Command.Open,Chimera.Stage.Event.TankOpened,0,true,Stage01.PowerRoom
S01_020_010,S01,20,PowerPuzzle,10,Chimera.Stage.Event.TankOpened,Puzzle.PowerA,,Chimera.Stage.Command.Activate,Chimera.Stage.Event.PowerRestored,0,true,Stage01.BranchA.Entry
S01_030_010,S01,30,ExitDoor,10,Chimera.Stage.Event.PowerRestored,ExitArea.Door.ExitA,,Chimera.Stage.Command.Unlock,Chimera.Stage.Event.ExitReached,0,true,Stage01.Result
```

퍼즐 내부 버튼 순서나 실패 규칙은 퍼즐 Blueprint/Component가 담당한다. CSV는 퍼즐 활성화와 완료 신호 연결까지만 담당한다.

## 10. DataForge 작업 흐름

```text
Stage01_LoadGroups.csv
    ↓ DataForge Preview / Apply
DT_CMStage01LoadGroups
    ↓ Editor materialization 또는 수동 Stage Schedule 반영
PDA_CMStage01LoadSchedule

Stage01_GimmickSequence.csv
    ↓ DataForge Preview / Apply
DT_CMStage01GimmickSequence
    ↓
StageDirector Sequence Component
```

스테이지별 CSV, RuleSet, DataTable을 분리해 네 명이 각 스테이지를 작업할 때 동일 uasset 충돌을 줄인다.

```text
Stage01/RS_CMStage01LoadGroups
Stage01/DT_CMStage01LoadGroups
Stage02/RS_CMStage02LoadGroups
Stage02/DT_CMStage02LoadGroups
```

원본 CSV는 런타임에서 파싱하지 않는다. DataForge의 Preview diff와 source drift 검증을 통과한 DataTable/PDA만 Cook한다.

## 11. 에디터 검증 항목

- LoadGroupId 중복과 빈 값
- 동일 Schedule 내 Scope 중복
- 동일 TimingTag와 LoadOrder 충돌
- 잘못된 TimingTag
- Schedule catalog에 없는 Scope·Timing 조합
- 레벨 내 PlacementId 중복
- CSV가 참조하지만 레벨에 없는 PlacementId
- 대상이 하나도 없는 GroupTag
- 도달할 수 없는 필수 Step
- 비가역 분기가 아닌 그룹에 `ReleaseWhenBranchRejected` 사용
- PDA 내부 무거운 에셋의 hard reference

## 12. 현재 구현된 기본 틀

- NKM `Plugins/AsyncPDALoader` Runtime/Editor Source 이식
- `CMStageLoadTags` 고정 TimingTag
- `FCMStageLoadGroupDefinition`과 보존 정책
- `UCMStageLoadSchedule` Schedule Provider
- `UCMStageLoadCoordinatorSubsystem` 순차 큐·개별 요청·분기 해제

아직 연결하지 않은 부분:

- GameState의 복제 Load Request
- PlayerController의 로컬 완료 Server RPC
- GameMode의 참가자별 로드 배리어와 timeout
- StageDirector의 기믹 Registry/Sequence Component
- CSV RowStruct와 DataForge RuleSet 인스턴스
- Data Layer/Streaming Level 활성화 연동

이 항목들은 PlayerState와 실제 스테이지 콘텐츠 계약이 확정되는 순서대로 추가한다.

## 13. 에디터 기본 사용 순서

1. `CMStageLoadSchedule` PDA를 스테이지마다 만든다.
2. `Refresh PDA Catalog`로 등록된 PDA 목록을 갱신한다.
3. `LoadGroups`에 Entry, Background, Result 그룹을 작성한다. Scope는 직접 입력하지 않는다.
4. `Refresh and Rebuild Catalog`를 눌러 PDA 목록과 자동 Scope를 갱신한다.
5. Catalog의 각 PDA에서 숫자 대신 `Load Group` 드롭다운을 선택한다. Scope와 TimingTag는 즉시 동기화된다.
6. PlayGameMode 또는 임시 테스트 Blueprint에서 Coordinator에 Schedule을 활성화한다.
7. 로딩 화면에서는 `Chimera.Load.Stage.Entry` 큐를 시작한다.
8. Starting 진입 시 `Chimera.Load.Stage.Background` 큐를 시작한다.
9. 분기 확정 시 선택되지 않은 LoadGroupId를 `RejectBranchLoadGroups`에 전달한다.
10. ActivationGate에서 `IsLoadGroupReady`를 확인한다.

`Rebuild Load Group Scopes`는 LoadGroupId를 이름순으로 정렬해 내부 Scope를 결정적으로 생성한다. 배열 순서를 바꿔도 같은 LoadGroupId는 같은 정렬 결과를 사용한다. 빈 ID와 중복 ID는 Scope를 만들지 않으며 Catalog의 존재하지 않는 그룹 선택은 미할당 처리한다.
