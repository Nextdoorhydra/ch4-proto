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

## Mechanism 구성

### CMStageMechanismBase

문과 버튼의 공통 활성화, 비활성화, 초기화 상태를 서버에서 관리하고 복제합니다. StageElement를 통해 `Mechanism.Activate`, `Mechanism.Deactivate`, `Mechanism.Reset` 명령을 받습니다.

### CMActivationTriggerComponent

버튼의 작동 가능 상태와 일회성 작동 이력을 관리합니다. 상호작용 또는 Overlap 판정은 구체적인 버튼에서 수행하고 `TryActivate`를 호출합니다.

### CMStageButtonBase

`TargetPlacementId` 또는 `TargetGroup`과 `TargetCommandTag`를 설정합니다. 버튼이 작동하면 같은 StageDirector를 통해 대상 문이나 장치에 명령을 전달합니다.

### CMStageDoorBase

활성 상태를 문의 열림 상태로 사용합니다. 기본값은 닫힘이며 `OnDoorOpenStateChanged`에서 Timeline, 이동, 충돌 변경을 구현합니다.

## 버튼과 문 연결 예시

1. 문 `StageElement.PlacementId`를 `Zone01.Mechanism.ExitDoor`로 지정합니다.
2. 버튼 `TargetPlacementId`에 같은 값을 지정합니다.
3. 버튼 `TargetCommandTag`를 `Chimera.Stage.Command.Mechanism.Activate`로 지정합니다.
4. 버튼 충돌 또는 상호작용 이벤트에서 서버의 `PressButton`을 호출합니다.
5. 문 블루프린트의 `OnDoorOpenStateChanged`에서 열림 연출을 구현합니다.

물건 배치 조건, 여러 버튼 조합, 플레이어별 시야 퍼즐은 현재 범위에서 제외합니다.

## 권장 구현 순서

1. KillZone으로 Hazard의 몸통 마디 판별과 처치 연결
2. 회전 칼날로 Motion의 서버 이동과 복제 방식 검증
3. Hazard와 Motion을 조합한 회전 칼날 완성
4. ForceZone으로 컨베이어와 바람의 물리 반응 검증
5. GAS 정책 확정 후 StatusZone과 지속형 GE 연결
