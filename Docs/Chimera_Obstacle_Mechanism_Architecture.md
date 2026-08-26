# Chimera 장애물·Mechanism 구성 가이드

## 장애물 제작 원칙

단순 장애물은 기존처럼 BP에 에셋을 직접 지정할 수 있다. 일반 장애물은 레벨에 Shell BP를 직접 배치하고 외형·이펙트·사운드와 효과 설정을 `CMObstacleDefinition` PDA로 준비한다. 위치·회전·이동축·이동거리는 레벨 인스턴스가 관리한다.

```text
레벨 배치 Shell
├─ 위치·회전
├─ PlacementId·GroupTags
├─ 이동축·속도·거리
└─ Soft ObstacleDefinition + LoadGroupId

CMObstacleDefinition PDA
├─ PrimaryMesh
├─ Materials
├─ NiagaraSystem
├─ LoopSound
├─ PartEffect
│  ├─ 코드 기반 내구도 피해·파츠 상태
│  └─ ApplicationPolicy·PeriodSeconds
└─ ChimeraEffect
   ├─ 공용 ASC GameplayEffectClass
   └─ ApplicationPolicy·PeriodSeconds
```

## 공통 장애물

`ACMStageObstacleBase`를 부모로 블루프린트를 만들고 충돌 영역과 필요한 기능 컴포넌트를 조합한다. PDA 방식에서는 부모가 제공하는 `PrimaryMesh`, `PrimaryEffect`, `LoopAudio`, `ObstacleDefinition`을 사용한다. 액터에 포함된 `StageElement`의 `PlacementId`와 `GroupTags`를 지정하면 StageDirector가 Activate, Deactivate, Restart 명령을 전달할 수 있다.

- 회전 칼날: Hazard + ObstacleMotion
- 움직이는 칼날: Hazard + ObstacleMotion
- 송곳: Hazard + ObstacleMotion
- 레이저: Hazard
- 독·감전·빙판 장판: Hazard + PartStatus
- 키메라 전체 혼란·경직 영역: ChimeraEffectZone
- 컨베이어·환풍구: ForceZone

충돌 영역의 BeginOverlap과 EndOverlap에서 각 컴포넌트의 `NotifyTargetEntered`, `NotifyTargetExited`를 호출합니다. 컴포넌트가 충돌 모양을 직접 소유하지 않으므로 장애물별 메시와 판정 영역을 자유롭게 구성할 수 있습니다.

## 레벨에 장애물 적용하기

### 1. 장애물 BP 준비

1. `CMStageObstacleBase` 또는 전용 C++ 베이스를 부모로 BP를 만든다.
2. Mesh·Collision과 Hazard·Motion·ForceZone 등 필요한 컴포넌트를 조합한다.
3. 장애물이 처음부터 작동해야 하면 `Start Active`를 켜고, 트리거 이후 작동해야 하면 끈다.
4. 한 액터만 직접 참조해 제어할 때는 `PlacementId`와 `GroupTags`를 비워도 된다.
5. StageDirector가 ID로 찾거나 여러 액터를 함께 제어해야 할 때만 `PlacementId` 또는 `GroupTags`를 지정한다.
6. 런타임 에셋을 비동기 준비하려면 `ObstacleDefinition`과 `LoadGroupId`를 설정한다. Definition을 사용하지 않는 테스트 BP는 BP에 에셋을 직접 넣어도 동작하지만 맵과 함께 로드된다.

`Start Active`는 초기 상태만 결정한다. 이후 `Activate`, `Deactivate`, `Toggle`, `Reset` 명령은 `CMStageElementBase`를 통해 장애물, 트리거, 문 같은 장치에 동일한 방식으로 전달된다.

### 2. 제어 방식 선택

| 상황 | 권장 연결 방식 |
|---|---|
| 버튼 하나가 장애물 하나를 제어 | 버튼의 `TargetActor` 직접 참조 |
| 여러 곳에서 같은 의미의 대상을 일괄 제어 | 대상의 `GroupTags`와 트리거의 `TargetGroup` |
| 버튼·압력판 여러 개와 장애물·문·조명을 조합 | `CMStagePuzzleController` 직접 참조 |
| 레벨 인스턴스 안에서 완결되는 퍼즐 | 컨트롤러와 구성 요소를 같은 레벨 인스턴스에 배치하고 직접 참조 |

단순한 로컬 퍼즐에 고유 GameplayTag를 계속 만드는 것은 피한다. GameplayTag 그룹은 여러 퍼즐 또는 StageDirector에서 의미 기반으로 대상을 찾아야 할 때 사용하고, 한 퍼즐 내부의 구체적인 연결은 PuzzleController의 직접 참조를 사용한다.

라이트도 퍼즐 대상이 되려면 `CMStageElementBase` 명령을 받을 수 있는 라이트 컨트롤러 또는 라이트 그룹 액터로 감싸야 한다. 일반 `PointLight` 액터를 그대로 PuzzleController의 Target으로 등록할 수는 없다.

## Soft Definition 로드 흐름

```text
맵 로드
→ Shell Actor 생성
→ Definition 미준비 상태
→ PrimaryMesh 숨김
→ Motion·Hazard·ChimeraEffectZone·StatusZone·ForceZone 비활성
→ Schedule이 LoadGroup 요청
→ PDA와 Gameplay Bundle 준비
→ DefinitionComponent가 완료 감지
→ Mesh·Material·Niagara·Sound·PartEffect·ChimeraEffect 적용
→ 기존 활성화 요청이 있으면 실제 장애물 작동
```

Definition이 설정되지 않은 장애물은 BP 직접 참조 방식으로 간주해 기존 동작을 유지한다. Definition 또는 Bundle 에셋 로드 실패 시 동기 로드로 우회하지 않고 장애물을 숨김·비활성 상태로 유지하며 Error 로그를 출력한다.

## PDA 장애물 제작 절차

1. `CMStageObstacleBase`를 상속한 Shell BP를 만든다.
2. 부모의 `PrimaryMesh`, `PrimaryEffect`, `LoopAudio`를 사용하고 실제 에셋은 BP에 직접 지정하지 않는다.
3. Box·Sphere 등 판정 Collision과 Motion·Hazard·ChimeraEffectZone·ForceZone 중 필요한 컴포넌트를 추가한다.
4. `/Game/Chimera/Environment/Obstacle/Data`에 `CMObstacleDefinition` Data Asset을 만든다.
5. PDA의 Mesh·Material·Niagara·Sound와 PartEffect 또는 ChimeraEffect를 지정한다.
6. 배치 BP의 `ObstacleDefinition.Definition`에 PDA를 Soft Reference로 지정한다.
7. `ObstacleDefinition.LoadGroupId`에 Schedule의 그룹 ID를 지정한다.
8. Stage Schedule의 Catalog에서 같은 PDA를 같은 GroupId에 배정하고 `Refresh And Rebuild Catalog`를 실행한다.
9. 레벨 인스턴스의 PlacementId, 이동축, 속도, 이동거리와 Transform을 조정한다.

```text
시작 구역    S01.Entry           BeforeStageStart
중반 장애물  S01.Area02.Hazards  Sequential 10
후반 장애물  S01.Area03.Hazards  Sequential 20
선택 분기    S01.BranchA.Hazards OnDemand
```

모든 장애물을 BeforeStageStart에 넣으면 초기 로딩 분산 효과가 없으므로 실제 등장 순서대로 그룹을 나눈다. Shell BP가 PDA 에셋을 직접 하드 참조하면 맵 로드 때 함께 준비되므로 Soft Definition 외에는 같은 런타임 에셋을 BP 기본값에 중복 지정하지 않는다.

## 컴포넌트 책임

### CMLaserBeamComponent

시작점과 끝점을 받아 중앙 Pivot, 로컬 X축 기준 Mesh 길이와 Niagara의 `User.BeamStart`, `User.BeamEnd`를 갱신한다. 판정이나 데미지는 담당하지 않으며 고정 레이저, 카메라 Hitscan, 전기 Beam 표현에서 공용으로 사용한다.

### CMAttackEmitterComponent

서버에서 `Hitscan` 또는 `Projectile` 공격 방식을 실행한다. Hitscan은 즉시 Line Trace 결과를 `OnAttackResolved`로 전달하고, Projectile은 비동기 준비된 Soft Class만 생성한다. 실제 부위 데미지와 GE 적용은 파츠 피격 계약 확정 후 연결한다.

### CMTargetScannerComponent

서버 Timer에서 거리, 시야각, 벽 가림을 검사해 가장 가까운 대상을 선택한다. 공격 종류와 좌우 스캔 연출은 알지 않으며 `OnTargetAcquired`, `OnTargetLost`만 전달한다.

## 고정 레이저 제작 절차

1. `CMLaserObstacleBase`를 상속한 BP를 만든다.
2. 부모가 제공하는 `LaserStart`, `BeamCollision`, `Hazard`, `BeamPresentation`을 사용한다.
3. `PrimaryMesh`에는 중앙 Pivot과 로컬 X축 길이를 가진 Beam Mesh를 지정한다.
4. `BeamPresentation.MeshOriginalLength`에 원본 Mesh 길이를 cm 단위로 입력한다.
5. `BeamPresentation.BeamThickness`, `MaxDistance`, `TraceChannel`을 설정한다.
6. 고정 벽이면 `RefreshInterval=0`, 움직이는 문이 레이저를 가리면 0.05~0.1을 사용한다.
7. 접촉 확인은 `Hazard.OnTargetEntered`에 Print를 연결한다.
8. `StartActive=false`면 Stage 명령의 Activate 전까지 Mesh, Niagara, Collision이 모두 꺼진다.

기존 BP에서 직접 만든 `LaserStart`, `BeamCollision`, `Hazard`와 `UpdateLaser` 그래프를 그대로 둔 채 부모를 변경하면 이름과 실행이 중복된다. 새 부모로 변경하기 전에 BP 컴포넌트와 Line Trace 그래프를 제거하거나, 새 자식 BP를 만들고 외형 설정만 옮긴다.

### CMHazardComponent

접촉한 팔·다리 파츠만 추적해 `PartEffect`의 내구도 피해와 상태를 코드로 적용한다. `OnceOnEnter`, `PeriodicWhileOverlapping`, `WhileOverlapping`, `KillOnEnter` 정책을 지원하며 복수 콜리전 중복은 파츠별 오버랩 횟수로 방지한다.

### CMPartStatusComponent

개별 파츠의 감전·경직·감속 상태를 관리한다. 같은 발생원의 같은 태그는 갱신하고, 여러 상태의 이동 배율은 곱한다. 행동 차단 상태는 해당 파츠의 `IsOperational`만 실패시켜 다른 파츠 GA에는 영향을 주지 않는다.

### CMChimeraEffectZoneComponent

키메라 전체에 영향을 주는 Definition의 Soft GE를 공용 ASC에 적용한다. `WhileOverlapping` 효과는 적용 Handle을 키메라별로 저장해 마지막 마디가 이탈하거나 장애물이 비활성화될 때 정확히 제거한다.

### CMObstacleMotionComponent

Rotation, Linear, PingPong 종류와 축, 속도, 거리, 시작·정지·반전·초기화를 제공합니다. 서버에서 실제 Transform을 갱신하고 `CMStageObstacleBase`의 이동 복제를 통해 클라이언트에 반영합니다.

- Rotation: `Speed`를 초당 회전 각도로 사용합니다.
- Linear: 최초 위치에서 `MoveDistance`만큼 이동한 뒤 정지합니다.
- PingPong: 최초 위치와 `MoveDistance` 지점 사이를 계속 왕복합니다.
- MotionAxis: 최초 배치 회전을 기준으로 하는 로컬 축입니다.

### CMStatusZoneComponent

대상 진입·이탈과 상태 태그만 전달하는 기존 확장 지점입니다. 신규 파츠 장판은 `CMHazardComponent + CMPartStatusComponent`, 키메라 전체 GE 영역은 `CMChimeraEffectZoneComponent`를 사용합니다.

### CMForceZoneComponent

로컬 방향과 힘 세기를 보관하고 월드 방향을 계산합니다. 서버에서 키메라 전체 Force를 활성 몸통 마디의 질량 비율로 분배합니다.

### CMObstacleDefinitionComponent

Soft Definition과 LoadGroupId를 보관한다. 로컬 Coordinator의 그룹 완료를 확인하고 이미 준비된 PDA만 `.Get()`으로 조회한다. 동기 로드는 사용하지 않는다. 성공 시 `OnDefinitionReady`, 실패 시 `OnDefinitionFailed`를 전달한다.

### CMStageObstacleBase 활성 조건

실제 장애물 활성 상태는 `활성화 요청 && Definition 준비 완료`다. Definition이 준비되기 전에 Activate 명령을 받으면 요청을 기억하고 로드 성공 후 작동한다. 비활성 상태에서는 Motion, Hazard, ChimeraEffectZone, StatusZone, ForceZone, 기본 Niagara와 반복 사운드를 함께 정지한다.

## 에디터 프리뷰

현재 안전한 런타임 로드 경로를 우선 구현했다. PDA 에셋을 런타임 컴포넌트 기본값에 복사하면 하드 참조가 저장될 수 있으므로 에디터 프리뷰는 아직 제공하지 않는다. 후속 구현에서는 에디터 전용 Transient Preview Component를 사용해 저장 데이터와 런타임 Soft Reference를 분리한다.

## Stage Element 구성

```text
CMStageElementBase
├─ CMStageObstacleBase
│  ├─ Laser
│  ├─ Blade
│  └─ HazardVolume
├─ CMStageTriggerBase
│  ├─ CMStageButtonBase
│  │  ├─ BasicButton
│  │  ├─ PressurePlate
│  │  └─ Lever
│  └─ VisionStone
└─ CMStageDeviceBase
   ├─ Door
   ├─ Bridge
   └─ Elevator
```

`CMStageElementBase`는 활성화, 비활성화, 토글, 초기화와 네트워크 상태 복제의 공통 계약이다. StageDirector는 장애물과 장치를 따로 순회하지 않고 해당 서브레벨의 Stage Element를 동일한 방식으로 초기화할 수 있다.

### CMStageTriggerBase

버튼, 압력판, 레버, 시야석의 공통 부모다. 유효한 입력을 받으면 다음 표준 신호를 발생시킨다.

- `Pulse`: 눌렀다는 순간 신호. `Toggle On Hit`을 끈 BasicButton의 신호다.
- `Activated`: 압력판이 눌린 상태, 레버가 켜진 상태처럼 지속 상태가 시작됨.
- `Deactivated`: 압력판에서 무게가 빠지거나 레버가 꺼지는 등 지속 상태가 끝남.

트리거를 PuzzleController에 등록하면 컨트롤러가 해당 트리거의 직접 Target 명령을 비활성화한다. 같은 입력으로 직접 대상과 컨트롤러 대상이 두 번 실행되는 것을 막기 위한 처리다.

### CMBasicButtonBase

기본 버튼은 직접 연결 모드에서 대상의 현재 상태를 반대로 바꾸는 `Toggle`을 기본으로 사용한다. PuzzleController에는 버튼 설정에 따라 순간 또는 상태 신호를 보낸다.

- `Toggle On Hit = false`: 한 번만 작동하고 `Pulse`를 보낸다. 내부 `One Shot`이 켜진다.
- `Toggle On Hit = true`: 누를 때마다 ON/OFF가 바뀌고 각각 `Activated`, `Deactivated`를 보낸다. 내부 `One Shot`이 꺼진다.
- Target과 Release Command 필드는 공통 부모 구조 때문에 보이지만 BasicButton은 런타임에 둘 다 `Toggle`로 사용한다.

실제 팔 휘두르기의 Sphere/Box Sweep 타격 판정은 아직 연결되지 않았다. 에디터 임시 테스트에서는 `Allow Chimera Body Overlap For Testing`을 켜고 몸통이 버튼 HitVolume에 진입하도록 한다. 다시 작동시키려면 몸통 전체가 HitVolume을 벗어난 뒤 재진입해야 한다. 이 옵션은 실제 상호작용 구현이 완료되면 제거하거나 Non-Shipping 테스트 전용으로 제한한다.

### CMStageDeviceBase

문, 다리, 엘리베이터처럼 퍼즐 결과를 표현하는 장치의 부모다. 활성 상태를 실제 열림, 연결, 이동 상태로 변환하는 Timeline·애니메이션·Collision 처리는 각 장치 자식이 담당한다.

## 단일 버튼으로 장애물 토글하기

1. 레벨에 BasicButton과 대상 장애물을 배치한다.
2. 장애물의 `Start Active`로 처음 켜짐/꺼짐 상태를 정한다.
3. 버튼의 `TargetActor`에 레벨에 배치된 장애물을 지정한다.
4. 반복 토글이면 `Toggle On Hit`을 켜고, 일회성이면 끈다.
5. 테스트 중에는 버튼 HitVolume에 유효 입력이 들어오는지 Print 또는 로그로 먼저 확인한다.
6. 버튼을 누를 때마다 장애물의 전체 활성 상태가 반전되는지 확인한다.

직접 참조를 사용하면 `TargetPlacementId`, `TargetGroup`, Target/Release Command Tag를 별도로 채울 필요가 없다. 장애물의 Motion만 끄는 것처럼 일부 기능만 제어하는 명령은 현재 공통 Toggle 범위가 아니므로 전용 장치 명령 또는 별도 컴포넌트 API로 확장한다.

## 여러 대상을 그룹으로 제어하기

1. 함께 제어할 각 Stage Element의 `GroupTags`에 같은 GameplayTag를 지정한다.
2. 트리거의 `TargetGroup`에 같은 태그를 지정한다.
3. 직접 참조와 그룹을 동시에 지정하지 않아 중복 명령을 피한다.

그룹 태그는 `Stage01.Puzzle.PowerGrid`처럼 레벨 구현 순서가 아니라 의미를 표현하는 이름을 권장한다. 레벨 인스턴스 내부에서만 쓰는 작은 퍼즐은 그룹 태그보다 PuzzleController 직접 참조가 더 단순하다.

## CMStagePuzzleController 적용

PuzzleController는 여러 Trigger의 신호를 받아 하나 이상의 Stage Element에 단계별 명령을 실행하는 퍼즐 단위 관리자다. 서버에서 조건과 Step을 실행하며 현재 Step과 완료 상태를 복제한다. 대상 장애물과 장치는 각자의 활성 상태를 복제한다.

### 주요 설정

- `Puzzle Channels`: 서로 독립적으로 진행되는 퍼즐 채널 목록.
- `Channel Id`: 에디터와 로그에서 구분할 이름.
- `Triggers`: 이 채널에 입력을 보내는 버튼, 압력판, 레버 등의 직접 참조.
- `Trigger Condition`
  - `Any`: 등록된 Trigger 중 하나의 유효 신호만 와도 현재 Step 실행.
  - `All`: 모든 Trigger가 조건을 충족해야 현재 Step 실행.
- `All Condition Mode`
  - `Latched`: 각 Trigger가 한 번씩 신호를 보냈으면 충족. 동시에 누를 필요가 없다.
  - `Simultaneous`: 모든 Trigger의 현재 ON/OFF 상태가 `Simultaneous Match State`와 일치해야 충족.
- `Simultaneous Match State`
  - `All Active`: 모든 Trigger가 ON일 때 실행.
  - `All Inactive`: 모든 Trigger가 OFF일 때 실행.
  - `All Equal`: 모든 Trigger가 ON이거나 모두 OFF에 도달할 때마다 실행.
- `Accepted Signal`
  - `PulseOrActivated`: 일반 버튼과 압력판을 함께 쓰는 기본값.
  - `ActivatedOnly`: 눌림/켜짐 상태만 인정.
  - `DeactivatedOnly`: 해제/꺼짐 상태만 인정.
  - `Any`: 모든 신호 인정.
- `Steps`: 조건이 충족될 때 실행할 단계 목록.
- `Commands`: 한 Step에서 실행할 `Activate`, `Deactivate`, `Toggle`, `Reset`과 직접 대상 목록.
- `End Behavior`
  - `Stop`: 마지막 Step 이후 채널 종료.
  - `Loop`: 마지막 Step 이후 첫 Step으로 복귀.
  - `RepeatCurrent`: 같은 Step을 계속 반복.

### 예시 A: 버튼 하나로 여러 장애물 토글

```text
TriggerCondition = Any
AcceptedSignal = PulseOrActivated
Triggers = [Button01]
Steps[0].Commands[0]
  Command = Toggle
  Targets = [Laser01, Blade01, Door01]
EndBehavior = RepeatCurrent
```

### 예시 B: 같은 버튼을 누를 때마다 다른 단계 실행

```text
Steps[0] = Laser01 Deactivate
Steps[1] = Blade01 Deactivate
Steps[2] = ExitDoor Activate
EndBehavior = Stop 또는 Loop
```

버튼의 `Toggle On Hit`을 켜야 여러 번 입력할 수 있다. `Loop`이면 마지막 입력 다음에 Step 0으로 돌아가고, `Stop`이면 마지막 Step 이후 추가 입력을 무시한다.

### 예시 C: 버튼과 압력판을 모두 충족해야 실행

```text
Triggers = [Button01, PressurePlate01]
TriggerCondition = All
AllConditionMode = Latched
AcceptedSignal = PulseOrActivated
Steps[0] = Door01 Toggle
```

버튼과 압력판은 어느 순서로 작동해도 된다. 두 입력이 모두 들어오면 Step을 실행하고 채널의 누적 조건을 비우므로, 다음 실행에는 두 Trigger가 다시 신호를 보내야 한다.

압력판 여러 개가 동시에 눌려 있는 동안만 실행해야 한다면 `All + Simultaneous + ActivatedOnly`를 사용한다. `Latched`는 순서를 강제하지 않는다. `ButtonA → ButtonC → ButtonB`처럼 정확한 입력 순서를 검사하고 틀리면 초기화하는 기능은 아직 구현 범위에 포함되지 않는다.

여러 Toggle Button이 모두 ON일 때만 실행하고, 같은 버튼을 다시 눌러 OFF로 만들면 조건도 취소되게 하려면 각 버튼의 `Toggle On Hit`을 켜고 `All + Simultaneous + All Active + ActivatedOnly`를 사용한다.

모든 버튼이 ON일 때 대상 장애물을 Toggle하고, 모든 버튼이 다시 OFF가 되었을 때 한 번 더 Toggle하려면 `All + Simultaneous + All Equal + Any`를 사용한다. 초기화 직후 모두 OFF인 상태만으로는 실행되지 않으며 실제 `Activated` 또는 `Deactivated` 신호로 모든 버튼이 같은 상태에 도달한 순간에만 실행된다. Step 하나를 계속 사용하려면 `End Behavior`는 `Repeat Current`로 설정한다.

### 레벨 인스턴스에서 사용

PuzzleController, Trigger, 장애물, 문, 라이트 컨트롤러를 같은 레벨 인스턴스에 넣고 컨트롤러가 내부 액터를 직접 참조하게 한다. 같은 레벨 인스턴스 에셋을 여러 번 배치하면 각 인스턴스의 내부 참조 관계도 함께 복제되므로 퍼즐 묶음을 재사용하기 좋다.

인스턴스마다 데미지나 색을 다르게 조정해야 하면 공통 PDA는 유지하고 BP의 인스턴스 오버라이드 값으로 수치만 바꾼다. 구성 자체가 달라지거나 에셋 조합이 달라질 때 별도 레벨 인스턴스 또는 Definition 변형을 만든다.

## 권장 구현 순서

1. KillZone으로 Hazard의 몸통 마디 판별과 처치 연결
2. 회전 칼날로 Motion의 서버 이동과 복제 방식 검증
3. Hazard와 Motion을 조합한 회전 칼날 완성
4. ForceZone으로 컨베이어와 바람의 물리 반응 검증
5. GAS 정책 확정 후 StatusZone과 지속형 GE 연결
