# Chimera 장애물·버튼·퍼즐 구성 가이드

기준: 2026-08-31 작업 트리의 C++ 구현. BP에 저장된 오버라이드는 코드 기본값과 다를 수 있다.
이 문서는 기존 Soft ObstacleDefinition 제작 절차를 대체한다.

UI 담당자용 함수·이벤트·무게 조회 계약은 [버튼 UI 연동 가이드](Chimera_Button_UI_Integration.md)를 참고한다.

## 1. 현재 구조

- CMStageElementBase: 공통 활성화/비활성화/토글/초기화와 상태 복제.
  - CMStageObstacleBase: 위험 효과·이동·힘·연출. CMLaserObstacleBase는 레이저 전용 자식.
  - CMStageTriggerBase: Pulse/Activated/Deactivated 신호.
    - CMStageButtonBase: CMBasicButtonBase, CMPressurePlateBase, CMLeverBase.
    - CMVisionStoneBase: 시야 조건.
    - CMPowerTriggerBase: 소켓 전원 조건.
  - CMStageDeviceBase: 문 등의 장치.
  - CMStagePuzzleController: 여러 입력/대상과 단계 관리.
- CMRoomStreamingController: 룸 서브레벨 로드 관리 Actor.
- CMRoomEntryTrigger: 몸통 진입 확인 Actor. 일반 버튼의 자식이 아니다.

### 장애물 제작 원칙

장애물 BP 또는 배치 인스턴스에 메시·머티리얼·Niagara·사운드·효과 수치를 직접 설정한다. 장애물별 Definition, LoadGroupId, Schedule Catalog 등록은 현재 제작 절차에 없다. 룸 서브레벨과 참조 에셋을 함께 로드한다.

남아 있는 구형 Definition 에셋/호환 코드를 신규 제작 경로로 해석하지 않는다. 이 문서 작업은 기존 에셋을 삭제하지 않는다. Head/Vision 등 다른 기능의 PDA 비동기 로드까지 폐지한 것은 아니다. 구글시트 장애물 밸런스/스테이지 배율 설계도 구현 완료로 취급하지 않는다.

## 2. 상태와 제어 경로

| 목적 | 현재 API |
|---|---|
| 요소 작동 상태 | IsElementActive() |
| 서버 요소 제어 | ActivateElement(), DeactivateElement(), ToggleElement(), ResetElement() |
| 장애물용 동일 계약 | ActivateObstacle(), DeactivateObstacle(), ResetObstacle(), IsObstacleActive() |
| 버튼 눌림 상태 | UCMActivationTriggerComponent::IsTriggered() |

Start Active는 초기 **장치 작동 요청**이다. 버튼의 초기 눌림이 아니다. 버튼 ON과 대상 레이저 OFF는 동시에 성립할 수 있다.

장애물 비활성화는 부착된 Motion/Hazard/StatusZone/ChimeraEffectZone/ForceZone과 기본 Niagara·반복 사운드를 정지한다. 공통 베이스는 외형 메시를 무조건 숨기거나 물리 Block을 제거하지 않는다. 레이저 등 전용 구현이 추가 표시/판정을 제어한다.

### 직접 대상과 ID

| 설정 | 실제 경로 |
|---|---|
| 트리거 TargetActor | 대상 StageElement의 PlacementId를 읽어 StageDirector에 명령 요청 |
| TargetPlacementId | StageDirector에서 ID 검색 |
| TargetGroup | StageDirector에서 GroupTags 검색 |
| PuzzleController Commands.Targets | 대상 Element 함수 직접 호출 |

**트리거 TargetActor는 직접 함수 호출이 아니다.** 이 경로는 트리거와 대상의 StageElement 등록이 필요하다. 월드에 StageDirector가 정확히 하나 있어야 하며, 빈 PlacementId는 등록 오류, 중복 ID는 뒤의 등록이 실패한다. 버튼·장애물 등 타입이 달라도 같은 등록 공간이다.

PuzzleController 직접 호출은 ID 검색에 의존하지 않지만, ID 누락에 따른 StageElement 등록 오류가 없어지는 것은 아니다. 레벨 인스턴스를 여러 번 배치해도 PlacementId가 자동으로 고유해진다고 가정하지 않는다.

TargetActor/TargetPlacementId/TargetGroup은 대상 목록 세 개가 아니라 선택 경로다. 의도한 경로만 채운다. 작은 룸 내부 퍼즐은 PuzzleController 직접 참조가 단순하다. 일반 PointLight는 그대로 대상이 될 수 없고 StageElement 기반 장치가 필요하다.

## 3. 장애물 배치

1. CMStageObstacleBase 또는 CMLaserObstacleBase를 부모로 BP를 만든다.
2. PrimaryMesh/PrimaryEffect/LoopAudio에 에셋을 지정한다. 메시 기본 머티리얼이 맞으면 별도 덮어쓰기는 불필요하다.
3. 일반 장애물에는 판정 볼륨과 필요한 Hazard/Motion/ForceZone 등을 추가한다. 전용 부모에 있는 컴포넌트는 중복 추가하지 않는다.
4. 일반 볼륨 Begin/EndOverlap에서 기능 컴포넌트의 NotifyTargetEntered/Exited를 호출한다. Hazard에는 OtherActor와 **OtherComp 모두** 전달한다. 레이저는 부모 연결을 사용한다.
5. 액터의 Part Effect/Chimera Effect, Start Active를 설정한다.
6. 해당 룸 서브레벨을 Current로 선택하고 배치한 뒤 퍼즐에 연결한다.

CMStageObstacleBase::ConfigureDirectEffects()는 BeginPlay에서 액터의 효과 설정을 부착 컴포넌트에 복사한다. **장애물 액터 설정이 원본**이다. Details에 비슷한 항목이 보여도 컴포넌트 런타임 복사본까지 따로 수정하지 않는다. 런타임 Details 수정의 자동 재적용·동기화는 보장하지 않는다.

### 현재 Hazard 피해 계약

| 대상 | 판별 | 적용 |
|---|---|---|
| 팔·다리 | Part Actor + 정확히 GetDamageHurtbox()인 컴포넌트 | ApplyPartDamage() 및 유효 StatusTag의 파츠 상태 |
| 몸통 마디 | GetSegmentIndexFromHurtbox(OtherComp) | ApplyDamageToSegment()로 해당 마디 직접 피해 |
| 기타 메시/컴포넌트 | 위 조건 불충족 | 효과 대상 아님 |

이름은 PartEffect지만 **현재 몸통 마디 피해도 같은 Damage/Policy 설정을 사용한다.** 이 마디 피해는 GE가 아니다. MovementMultiplier/BlocksAbility 같은 파츠 상태를 몸통에 적용하는 경로는 아니다. 바닥/공기 장판을 분리하는 별도 대상 정책 옵션이 있다고 가정하지 않는다.

| Part Effect 설정 | 의미 |
|---|---|
| Enabled | 효과 허용 |
| Once On Enter | 최초 진입 한 번 |
| Periodic While Overlapping | Period Seconds마다 적용. 첫 피해도 타이머 주기를 기다림 |
| While Overlapping | 진입 시 피해/상태 적용, 마지막 이탈 시 발생원 상태 제거. 매 프레임 피해가 아님 |
| Kill On Enter | 접촉 파츠 또는 마디에 처치량 피해 |
| Damage Per Application | 한 번 적용할 피해량 |
| Status Tag / Duration | 파츠 상태/지속시간. WhileOverlapping은 이탈 제거 방식 |
| Movement Multiplier | 상태가 적용된 파츠의 이동 배율. 태그 없이 배율만 바꾸면 상태가 생성되지 않음 |
| Blocks Ability | 해당 파츠 행동 차단 |

파츠는 Actor별, 몸통은 Hurtbox Component별 오버랩 횟수를 추적한다. ChimeraEffect는 별도로 공용 ASC에 GameplayEffect를 적용한다. 단순 접촉 피해를 위해 GE를 만들 필요는 없다.

### Collision

- 외형 메시의 물리 Block과 효과 볼륨의 Overlap을 분리한다.
- 양쪽 Generate Overlap Events와 채널 응답을 확인한다. ChimeraHurtbox가 볼륨에서 Ignore이면 효과가 전달되지 않는다.
- 공통 PrimaryMesh는 Camera를 Ignore한다. 기존 BP 저장값은 따로 확인한다.
- 장식용 회전체에는 Hazard/판정 볼륨이 필요 없다. 이동을 막을 외형 충돌까지 제거할지는 별도 결정이다.

### 레이저·Motion·Force

레이저는 CMLaserObstacleBase의 LaserStart/BeamCollision/Hazard/BeamPresentation을 사용한다. 로컬 X축 Beam 길이에 맞춰 MeshOriginalLength, 두께, MaxDistance, TraceChannel을 설정한다. 정적 벽이면 RefreshInterval=0, 움직이는 차폐물이 있으면 갱신 주기를 준다. CMLaserBeamComponent는 표현, Hazard는 피해 담당이다. 액터 PartEffect의 Enabled와 피해량/주기를 설정한다. BP 자체 Trace/Overlap 그래프와 부모 처리를 중복 실행하지 않는다.

UCMObstacleMotionComponent는 Rotation(도/초), Linear(거리까지 이동 후 정지), PingPong(왕복)을 지원한다. MotionAxis는 초기 배치 회전 기준이다. 서버 Transform 갱신과 장애물 이동 복제를 사용한다. StartMotion/StopMotion/ReverseMotion/ResetMotion은 이동만 제어하며, Element 명령은 위험 효과·연출까지 함께 제어한다.

UCMForceZoneComponent는 LocalDirection/ForceStrength로 서버에서 영역 안 키메라에 지속 Force를 적용한다. 컨베이어/팬에 사용하며 회전 메시 연출과는 별개다.

## 4. 버튼 종류

### 기본 버튼 — CMBasicButtonBase

- 서버 팔 스윙 범위 검사에서 정확히 HitVolume에 맞으면 NotifySwingHit()를 호출한다. 같은 팔의 같은 AttackId는 한 번만 처리한다.
- 몸통 Overlap 임시 테스트 기능은 제거되었다. 팔이 가만히 닿는 것만으로도 누르지 않는다.
- Toggle On Hit=true: 타격마다 눌림/해제 교대. BeginPlay에서 One Shot=false로 설정한다.
- Toggle On Hit=false: Press를 시도하고 Pulse를 보낸다. 자동 Release는 없으므로 보통 최초 눌림 후 재입력이 상태를 바꾸지 않는다. One Shot 기본값은 true지만 이 분기가 저장값을 강제로 true로 덮지는 않는다.
- 직접 연결의 Target/Release Command는 **런타임에 둘 다 Toggle로 강제**한다. 명시적인 Activate/Deactivate 동작은 PuzzleController에서 설정한다.

One Shot은 한 번 활성화한 이력을 Reset까지 유지하여 재활성화를 막는다. One Shot을 끄는 것과 현재 눌림 해제는 별개다.

### 압력판 — CMPressurePlateBase

1. PressureVolume 크기와 Query Only/Generate Overlap Events를 설정한다.
2. 프로젝트 커스텀 ChimeraHurtbox 응답을 Overlap으로 확인한다. OverlapAllDynamic 이름만 믿지 않는다.
3. Required Weight=100, Release Weight=90, One Shot=false로 테스트한다.
4. 직접 연결이면 TargetActor와 고유 PlacementId를 준비한다. 눌러서 레이저를 끄려면 Target Command=Chimera.Stage.Command.Mechanism.Deactivate, Release Command=Chimera.Stage.Command.Mechanism.Activate.
5. 복합 퍼즐이면 PuzzleController Triggers에 등록하고 직접 Target은 비운다.

OFF에서 CurrentWeight >= RequiredWeight이면 ON, ON에서 CurrentWeight < ReleaseWeight이면 OFF다. 정확히 90이면 유지하고 90 미만에서 해제한다.

오버랩 Actor의 UCMMechanismWeightComponent를 합산한다. 서로 다른 팔/다리는 더하고 같은 Actor의 여러 콜리전은 한 번만 센다. 몸통에 붙은 모든 파츠를 자동 합산하지 않으며 몸통 마디 자체 무게는 포함하지 않는다.

팔/다리는 코드에서 무게 컴포넌트를 생성하고 FCMPartLegArmTableRow::Weight를 적용한다. CSV 열은 정확히 Weight이며 0 이상의 유한값이다. 기존 데이터가 0이면 합산에 기여하지 않는다.

UI는 공통 GetPresentationState()/OnPresentationStateChanged(State)를 사용한다. **현재·목표·해제 무게와 On/Off를 하나의 표시용 구조체로 복제한다.** GetCurrentWeight()/GetRequiredWeight()/GetReleaseWeight()도 클라이언트에서 해당 스냅샷을 조회한다. 기존 OnPressureChanged는 서버 이벤트로 유지한다. 초기 bReady 처리와 연결 예시는 [UI 문서](Chimera_Button_UI_Integration.md)를 참고한다.

재계산은 서버 Begin/EndOverlap에서 한다. 안에서 무게만 변경하거나 Actor가 사라졌을 때 즉시 반영은 보장하지 않는다. Reset은 추적 목록을 비우고 이미 겹친 Actor를 재검색하지 않으므로 재진입 테스트가 필요하다.

### 레버 — CMLeverBase

- 움직일 메시를 LeverPivot 자식, 피벗을 회전 중심, ArmHoldVolume을 손잡이에 둔다. 논리 레버이며 손잡이 물리 시뮬레이션을 사용하지 않는다.
- 일반 팔은 ICMArmHoldTarget으로 잡고 팔 Actor 이동을 LocalPullAxis에 투영한다.
- LocalPullAxis는 입력 이동 방향, LocalRotationAxis는 표현 회전축이다. 수평 회전은 배치 회전을 고려해 회전축이 월드 Z가 되도록 설정한다.
- FullTravelDistance는 Alpha -1→+1 전체 이동 거리. Alpha >= SwitchThreshold에서 Press, <= -SwitchThreshold에서 Release한다.
- RotationHalfAngle=45, SwitchThreshold=.8이면 기본 배치 회전에 대한 목표 오프셋 ±36도에서 전환한다. 시각 보간 때문에 화면 각도는 다를 수 있다.
- 키를 놓으면 마지막 ON/OFF 끝점으로 돌아간다. Hold에 소비한 입력을 놓을 때 추가 일반 스윙이 발생하지 않도록 처리한다.
- 그랩 팔은 Pull 방향/세기로 양/음 상태를 선택한다. 처리한 대상이면 변화가 없어도 몸통 Pull로 우회하지 않는다.
- RotationTransitionDuration은 양끝 간 시각 이동 시간(기본 .3초, 0은 즉시). 일반 Hold와 그랩 Pull 모두 같은 보간 경로를 사용한다.
- OnLeverAlphaChanged는 논리 목표, OnLeverVisualAlphaChanged는 보간된 표현 값. On/Off는 IsTriggered가 기준이다.

시야석은 RequiredWatchingPlayers/VisionStoneMode/EvaluationInterval, 전원 트리거는 RequiredSockets/bRequireAllSockets로 설정한다. 개별 카운트/표현 이벤트의 클라이언트 복제를 공통 Trigger 상태 복제와 혼동하지 않는다.

## 5. PuzzleController

서버가 Trigger 신호를 구독하고 Step을 실행한다. 등록한 Trigger의 직접 Target 명령은 비활성화해 이중 실행을 막는다. 같은 Trigger를 여러 컨트롤러가 소유하기보다 한 컨트롤러의 여러 채널에 등록한다.

| 옵션 | 의미 |
|---|---|
| Any | 수락 신호 하나마다 실행 |
| All + Latched | 각 입력이 한 번씩 수락되면 실행 후 누적 기록 비움. 순서 검사 아님 |
| All + Simultaneous | Activated/Deactivated로 추적한 상태 평가. Pulse 무시 |
| All Active / All Inactive / All Equal | 모두 ON / 모두 OFF / 모두 같은 상태 |
| Accepted Signal | PulseOrActivated / ActivatedOnly / DeactivatedOnly / Any |
| Steps → Commands | Activate / Deactivate / Toggle / Reset 및 대상 목록 |
| Stop | 마지막 Step 후 종료 |
| Loop | 마지막 Step 후 Step 0 |
| Repeat Current | 마지막 Step에 머물며 반복. 여러 Step이면 앞 단계는 먼저 진행 |
| Reset Targets with Puzzle | Reset 시 Commands 대상도 초기화 |

Simultaneous는 수락 여부와 별개로 ON/OFF를 추적하지만 실행을 일으킨 신호는 Accepted Signal을 통과해야 한다. 모두 OFF에서도 실행하려면 ActivatedOnly로 두면 안 된다. BeginPlay/Reset에서 현재 눌림을 재조회하지 않으며 초기 모두 OFF만으로 자동 실행하지 않는다.

| 목적 | 예시 |
|---|---|
| 토글 버튼 매 타격마다 여러 대상 반전 | Toggle On Hit=true; Any + Accepted Any; Step 0 Toggle; Repeat Current |
| 모두 ON 또는 모두 OFF에서 반전 | All + Simultaneous + All Equal + Accepted Any; Toggle; Repeat Current |
| 모두 ON이면 레이저 OFF, 모두 OFF이면 ON | 채널 A: AllActive + ActivatedOnly → Deactivate / 채널 B: AllInactive + DeactivatedOnly → Activate. 둘 다 All + Simultaneous + Repeat Current |
| 버튼과 압력판을 한 번씩 충족 | All + Latched + PulseOrActivated |
| 매 입력마다 다른 대상 제어 | Any + Accepted Any; Step 0/1/2에 명령; Loop 또는 Stop |

두 채널 예시는 혼합 상태에서 이전 대상 상태를 유지한다. 하나라도 풀리면 즉시 켜는 조건과 다르다. 토글 버튼에서 PulseOrActivated만 받으면 해제 타격은 실행하지 않아 매 두 번째 입력이 빠진 것처럼 보인다.

ResetPuzzle()은 채널 진행 기록과 선택한 대상들을 초기화하지만 Triggers 자체를 자동 Reset하지 않는다. 버튼/압력판 상태도 함께 초기화해야 한다. 정확한 입력 순서 검사·오답 처리는 별도 구현 대상이다.

## 6. 룸 스트리밍

1. 퍼시스턴트 Levels에 룸 맵을 등록하고 Streaming Method=Blueprint로 설정한다.
2. 퍼시스턴트에 RoomStreamingController 하나를 배치하고 Rooms의 RoomId/Level을 순서대로 등록한다.
3. 룸마다 같은 RoomId의 RoomEntryTrigger 하나를 둔다. 전체 몸통이 함께 들어갈 크기로 만든다.
4. 뒤쪽 차단문은 해당 룸에 두고 EntryBlockerDoor에 연결한다. 퍼즐로 여는 앞쪽 문과 분리할 수 있다.
5. 퍼즐 요소와 컨트롤러는 같은 룸에 두어 언로드 경계를 넘는 참조를 줄인다. 한 룸에 여러 퍼즐 컨트롤러를 둘 수 있다.

현재 N: N/N+1 로드·표시, N+2 숨김 프리로드, 이전 및 그 외 룸 언로드. 클라이언트도 복제된 CurrentRoomIndex로 로컬 스트리밍을 적용한다. 다음 룸 장애물은 Start Active를 따르며 버튼으로 켤 대상은 false로 둔다.

진입 판정은 이벤트를 일으킨 **한 ACMChimera의 모든 활성 몸통 마디**다. 독립된 키메라 Actor 여러 개를 모두 기다리는 구현이 아니다. 차단문이 있고 활성 상태면 닫힘 완료를 기다린다. 문이 없거나 이미 비활성이면 즉시 Commit하므로 **문 미배치가 언로드를 막지는 않는다.**

끝 범위를 넘는 룸은 요청하지 않는다. 다음 순서로 마지막 룸에 Commit하면 서버 OnFinalRoomCommitted가 발생하지만 이 이벤트 자체가 다음 스테이지 Travel은 아니다. 진입 트리거 누락/중복은 검증 경고를 확인한다.

## 7. 확인 순서

1. 서버 입력 → IsTriggered → 명령 전달 → IsObstacleActive 순으로 확인한다. Print만으로 대상 명령 성공을 판단하지 않는다.
2. 직접 연결 오류: StageDirector 수, 빈/중복 PlacementId, 대상 ID, 명령 태그.
3. 압력판: 서버 OnPressureChanged의 무게/Pressed. 문턱을 넘었다면 대상 연결을 확인.
4. 퍼즐: LogChimeraPuzzle의 [Puzzle Step Executed], Accepted Signal, End Behavior.
5. 피해: 액터 Enabled, OtherComp, ChimeraHurtbox 채널, 서버 권한.
6. UI: OnButtonPressed/Released는 서버 게임 이벤트다. 서버/클라이언트 공통 표시는 GetPresentationState와 OnPresentationStateChanged를 사용한다.
7. Reset/룸 재로드/지연 접속 상태 복원을 별도 검증한다.

## 코드 기준

Source/Chimera/Stage 아래 CMStageElementBase, CMStageElementComponent, CMStageDirector, Obstacle/CMStageObstacleBase, Obstacle/CMLaserObstacleBase, Obstacle/Component/CMHazardComponent, Trigger의 버튼·레버·압력판 및 ActivationTriggerComponent, Puzzle/CMStagePuzzleController, Room의 Controller/EntryTrigger 구현을 기준으로 한다.
