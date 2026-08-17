# Chimera 스테이지 Gameplay Tag 카탈로그

## 1. 목적

스테이지 담당자가 임의로 비슷한 태그를 반복 생성하지 않도록 공통 태그와 확장 가능한 태그의 경계를 정의한다.

```text
Chimera.Stage
├─ Event
├─ Command
└─ Group
```

- `Event`: 이미 발생한 사건
- `Command`: 대상에게 수행하도록 전달하는 명령
- `Group`: 여러 배치 대상을 함께 선택하는 분류

Trigger는 최상위 Gameplay Tag 분류가 아니다. Trigger는 레벨에 배치하는 액터 종류이며, Trigger에서 발생한 사건은 Event로 발행한다.

```text
PlacementId = Hallway.Trigger.Entry
EventTag = Chimera.Stage.Event.AreaEntered
```

## 2. 태그 관리 등급

### Fixed Native

C++ Native Gameplay Tag로 등록한다.

- 시스템 코드가 직접 참조하는 태그
- 모든 스테이지가 공통으로 사용하는 Event
- 실제 컴포넌트 동작과 계약을 맺는 Command
- 이름 변경 시 코드와 Blueprint 동작이 바뀌는 태그

레벨 담당자가 CSV에서 이름을 수정하거나 삭제하지 않는다.

### Project Extension

Google Sheet 또는 CSV에서 `FGameplayTagTableRow` DataTable로 생성할 수 있다.

- 스테이지 배치에 따라 달라지는 Group
- 공통 Event만으로 표현할 수 없는 승인된 특수 Event
- 코드 동작과 직접 연결되지 않는 분류 태그

### Candidate

필요성이 예상되지만 아직 소비하는 시스템이 없는 후보 태그다. 후보는 문서에만 기록하고 실제 태그 사전에 미리 등록하지 않는다.

## 3. 고정 Event 태그 후보

다음 목록을 1차 Fixed Native 후보로 사용한다.

### 스테이지 생명주기

```text
Chimera.Stage.Event.Starting
Chimera.Stage.Event.Started
Chimera.Stage.Event.Completed
Chimera.Stage.Event.Failed
```

### 구역과 트리거

```text
Chimera.Stage.Event.AreaEntered
Chimera.Stage.Event.AreaExited
Chimera.Stage.Event.TriggerActivated
Chimera.Stage.Event.TriggerDeactivated
```

### 목표와 퍼즐

```text
Chimera.Stage.Event.ObjectiveActivated
Chimera.Stage.Event.ObjectiveCompleted
Chimera.Stage.Event.ObjectiveFailed
Chimera.Stage.Event.PuzzleStarted
Chimera.Stage.Event.PuzzleCompleted
Chimera.Stage.Event.PuzzleFailed
```

### 문과 진행 경로

```text
Chimera.Stage.Event.DoorOpened
Chimera.Stage.Event.DoorClosed
Chimera.Stage.Event.DoorLocked
Chimera.Stage.Event.DoorUnlocked
Chimera.Stage.Event.BranchSelected
Chimera.Stage.Event.ExitReached
```

공통 Event로 구체적인 대상을 구분하려면 향후 `EventId`를 메시지에 추가하는 것이 최종 구조다. 현재 구현은 EventTag 하나로만 DataTable 행을 찾으므로 필요한 경우 다음과 같은 특수 Event를 Project Extension으로 추가할 수 있다.

```text
Chimera.Stage.Event.TankOpened
Chimera.Stage.Event.PowerRestored
Chimera.Stage.Event.HallwayCleared
```

스테이지 번호를 태그에 넣지 않는다.

잘못된 예:

```text
Chimera.Stage.Event.Stage01PowerRestored
Chimera.Stage.Event.Stage02HallwayEntered
```

## 4. 고정 Command 태그 후보

Command는 코드 또는 Blueprint 컴포넌트가 실제로 지원하는 경우에만 등록한다.

### 공통 상태

```text
Chimera.Stage.Command.Activate
Chimera.Stage.Command.Deactivate
Chimera.Stage.Command.Enable
Chimera.Stage.Command.Disable
Chimera.Stage.Command.Reset
```

### 문과 잠금

```text
Chimera.Stage.Command.Open
Chimera.Stage.Command.Close
Chimera.Stage.Command.Lock
Chimera.Stage.Command.Unlock
```

### 움직임 후보

다음 태그는 이를 처리하는 공용 이동 컴포넌트가 만들어질 때 등록한다.

```text
Chimera.Stage.Command.Motion.Start
Chimera.Stage.Command.Motion.Stop
Chimera.Stage.Command.Motion.Pause
Chimera.Stage.Command.Motion.Resume
Chimera.Stage.Command.Motion.Reverse
```

### 조명

```text
Chimera.Stage.Command.Lighting.Default
Chimera.Stage.Command.Lighting.Alert
Chimera.Stage.Command.Lighting.Blackout
```

### 나이아가라 이펙트

현재 `CMStageEffectComponent`가 지원하는 Fixed Native 태그다.

```text
Chimera.Stage.Command.Effect.Activate
Chimera.Stage.Command.Effect.Deactivate
Chimera.Stage.Command.Effect.Restart
Chimera.Stage.Command.Effect.Burst
```

### 오디오 후보

오디오 제어 컴포넌트가 만들어질 때 등록한다.

```text
Chimera.Stage.Command.Audio.Play
Chimera.Stage.Command.Audio.Stop
Chimera.Stage.Command.Audio.Pause
Chimera.Stage.Command.Audio.Resume
```

### 시퀀스 후보

연출 제어 컴포넌트가 만들어질 때 등록한다.

```text
Chimera.Stage.Command.Sequence.Play
Chimera.Stage.Command.Sequence.Stop
Chimera.Stage.Command.Sequence.Skip
```

대상 이름을 Command에 포함하지 않는다.

```text
잘못된 태그: Chimera.Stage.Command.OpenTankRoomDoor

올바른 연결:
TargetPlacementId = TankRoom.Door.Entry
CommandTag = Chimera.Stage.Command.Open
```

## 5. Group 태그 규칙

Group은 Project Extension으로 관리한다.

형식:

```text
Chimera.Stage.Group.<Area>.<Type>
```

허용할 Type 후보:

```text
Obstacle
Door
Puzzle
Trigger
Lighting
Effect
Audio
Presentation
Objective
```

예시:

```text
Chimera.Stage.Group.Hallway.Obstacle
Chimera.Stage.Group.Hallway.Lighting
Chimera.Stage.Group.Hallway.Effect
Chimera.Stage.Group.PowerRoom.Door
Chimera.Stage.Group.PowerRoom.Puzzle
Chimera.Stage.Group.StageExit.Objective
```

개별 액터 하나를 위한 Group 태그를 만들지 않는다.

```text
잘못된 태그: Chimera.Stage.Group.Hallway.Obstacle.Piston01
올바른 ID: Hallway.Obstacle.Piston01
```

## 6. CSV와 Google Sheet로 확장하는 방법

Unreal Engine 5.7은 `FGameplayTagTableRow`를 제공한다.

| 열 | 역할 |
|---|---|
| `Name` | DataTable Row Name |
| `Tag` | 등록할 Gameplay Tag 문자열 |
| `DevComment` | 개발자용 설명 |

CSV 형식:

```csv
Name,Tag,DevComment
S01_Group_Hallway_Obstacle,Chimera.Stage.Group.Hallway.Obstacle,Stage01 복도 장애물 동시 제어
S01_Group_Hallway_Effect,Chimera.Stage.Group.Hallway.Effect,Stage01 복도 환경 이펙트 동시 제어
S01_Event_PowerRestored,Chimera.Stage.Event.PowerRestored,전력 퍼즐 완료 후 발생하는 특수 이벤트
```

권장 Google Sheet 탭:

```text
Common_Tag
├─ 고정 Native 태그 문서 목록
└─ 태그 설명과 담당 시스템

Stage_Tag_Extension
├─ Group 태그
└─ 승인된 특수 Event 태그
```

DataForge 흐름:

```text
Google Sheet 또는 CSV
→ DataForge Preview/Apply
→ FGameplayTagTableRow DataTable
→ Project Settings의 Gameplay Tag Table List에 등록
→ 에디터 재시작
→ Stage Sequence DataTable에서 태그 선택
```

태그 DataTable은 엔진의 Gameplay Tag 사전이 초기화될 때 읽힌다. 편집 중 태그 목록을 변경한 경우 에디터 재시작을 기본 규칙으로 한다.

## 7. CSV에서 수정할 수 있는 범위

| 종류 | CSV 추가 | CSV 이름 변경 | 이유 |
|---|---:|---:|---|
| 루트 `Event/Command/Group` | 금지 | 금지 | 시스템 분류 계약 |
| 공통 Event | 금지 | 금지 | C++와 여러 DataTable에서 공유 |
| 특수 Event | 승인 후 가능 | 직접 변경 금지 | 기존 DataTable 참조 보호 |
| Command | 금지 | 금지 | 실행 코드와 계약 |
| Group | 가능 | 사용 전 가능 | 레벨 배치에 따라 변함 |
| PlacementId | 해당 없음 | 레벨과 DataTable 동시 변경 | Gameplay Tag가 아닌 FName |

CSV에 새 Command 문자열을 추가한다고 해당 기능이 생기는 것은 아니다. 대상 컴포넌트가 그 Command를 처리하도록 구현되어 있어야 한다.

## 8. 멀티플레이와 패키징 규칙

- Gameplay Tag 목록은 서버와 모든 클라이언트에서 동일해야 한다.
- 런타임 중 Google Sheet나 CSV를 읽어 태그를 새로 추가하지 않는다.
- 태그 DataTable은 Cook 대상에 포함한다.
- 네트워크 테스트 전에 에디터를 재시작하고 모든 개발자가 같은 태그 데이터를 받았는지 확인한다.
- 배포된 빌드의 태그 목록은 콘텐츠 업데이트 절차 없이 임의로 변경하지 않는다.

## 9. 새 태그 승인 절차

1. 기존 태그 카탈로그에서 같은 의미를 검색한다.
2. PlacementId나 기존 Group으로 해결할 수 있는지 확인한다.
3. Event, Command, Group 중 분류를 결정한다.
4. Command라면 이를 처리할 코드 또는 Blueprint 컴포넌트가 있는지 확인한다.
5. 공통 Event나 Command는 코드 담당자가 Native Tag로 추가한다.
6. Group 또는 특수 Event는 태그 확장 Sheet에 추가한다.
7. DataForge Preview/Apply 후 태그 DataTable을 갱신한다.
8. 에디터 재시작 후 Stage Sequence DataTable에서 선택 가능한지 확인한다.
9. 리슨 서버 테스트에서 서버와 클라이언트가 같은 태그를 해석하는지 확인한다.

## 10. 현재 구현 상태

현재 코드에 Native Tag로 등록된 항목:

```text
Chimera.Message.Stage.Event
Chimera.Stage.Command.Effect.Activate
Chimera.Stage.Command.Effect.Deactivate
Chimera.Stage.Command.Effect.Restart
Chimera.Stage.Command.Effect.Burst
```

아직 해야 할 작업:

- 공통 Event Native Tag 등록
- 공통 상태 및 문 Command Native Tag 등록
- 조명 Command Native Tag 등록
- `FGameplayTagTableRow` DataTable을 DataForge에서 생성하는 RuleSet 구성
- 생성된 태그 DataTable을 Gameplay Tag Table List에 등록
- 중복·금지 루트·잘못된 이름을 검사하는 Data Validation 추가
