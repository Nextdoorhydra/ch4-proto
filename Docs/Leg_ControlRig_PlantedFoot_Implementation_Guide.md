# 다리 Control Rig 및 Planted-Foot 상태 머신 구현 판단 지침

> 문서 상태: 구현 계약 확정 · 1차 Control Rig 연결 구현 완료  
> 작성 기준일: 2026-08-31  
> 대상 구현자: GPT-5.6 LUNA XHIGH  
> 검토 방식: 프로젝트 코드·Unreal 애셋 실측 + GPT-5.6 LUNA XHIGH 독립 검토

## 1. 이 문서의 목적

이 문서는 다리 Control Rig와 planted-foot 상태 머신 구현에서 지켜야 할 소유권,
데이터 흐름, 좌표계, 네트워크, 상태 전이, 경사면 처리 및 검증 기준을 고정한다.
현재 1차 구현은 이 계약에 따라 Control Rig 입력과 최종 solver 순서까지 연결되어
있으며, 이후 튜닝은 이 문서의 경계를 유지한다.

후속 구현자는 이 문서를 제안 모음이 아니라 구현 계약으로 사용한다. 애셋 실측 결과가
문서와 다르면 임의로 우회하지 말고 차이를 먼저 보고한다.

이번 작업의 범위는 다음과 같다.

- 플레이어의 실제 다리 버튼 입력으로 시작되는 gameplay step
- 몸통 이동 중 기존 발 고정점이 도달 범위를 벗어났을 때의 visual replant
- `Trace → 지면 위치/Normal → IK Target 계산 → Control Rig 적용` 파이프라인
- 평지와 경사면의 발 위치 및 회전 정렬
- 서버 권위 상태와 클라이언트 렌더링의 분리
- 8마디·16슬롯 테스트 몸체에서의 검증

이번 구현에서 다음은 범위 밖이다.

- Control Rig가 몸통 collision 또는 Chaos body를 이동하는 기능
- 다리 IK를 이용한 몸통 물리 높이 보정
- 움직이는 플랫폼 접지
- 클라이언트 예측형 발걸음
- 기존 몸통 질량, constraint, impulse, collision의 재튜닝

## 2. 구현 전에 확인된 현재 상태

### 2.1 관련 소스와 애셋

| 영역 | 경로 | 확인 내용 |
| --- | --- | --- |
| 생산 키메라 | `Source/Chimera/Player/CMChimera.*` | 8개 body segment, segment당 좌·우 슬롯, 서버 물리 |
| 이동 조정 | `Source/Chimera/Movement/CMLineBodyMovementCoordinator.*` | 다리 입력 승인, 지면 Sweep, 힘 적용 |
| 다리 상태 | `Source/Chimera/Parts/Leg/CMLegPart.*` | step 방향·지면점·Normal·시각 복제 |
| AnimInstance | `Source/Chimera/Animation/CMPartAnimInstance.*` | 월드 지면 데이터를 mesh component space로 변환 |
| 테스트 몸체 | `Source/Chimera/Animation/CMProceduralAnimationTestBody.*` | 8마디·16슬롯, 매 Tick 입력 시뮬레이션 |
| 테스트 BP | `/Game/Chimera/Character/Test/BP_CMProceduralAnimationTestBody` | 독립 테스트 몸체 |
| 다리 BP | `/Game/Chimera/Character/Part/Leg/BP_CMLegPart` | 왼쪽 다리 mesh와 현재 AnimBP 사용 |
| 다리 AnimBP | `/Game/Chimera/ABP_CMLegLProcedural` | 현재 Modify Bone, Two Bone IK, RigidBody 구성 |
| 다리 mesh | `/Game/CoreC/02BaseBody/SKM/male_Leg_L` | 분리된 왼쪽 다리 mesh |
| 다리 skeleton | `/Game/CoreC/02BaseBody/SKM/male_Leg_L_Skeleton` | 89 bones, 소켓 없음 |
| 경사 테스트 | `/Game/Chimera/Environment/Level/Test/SubLevel/Test_Incline` | 경사면 PIE 검증용 |

`BP_CMLegPart`는 Blueprint SCS component를 추가하지 않고 네이티브 component 5개를
사용한다. `PartMesh`의 현재 주요 설정은 다음과 같다.

- Skeletal Mesh: `/Game/CoreC/02BaseBody/SKM/male_Leg_L`
- Anim Class: `/Game/Chimera/ABP_CMLegLProcedural.ABP_CMLegLProcedural_C`
- 상대 위치: 약 `(20, 10, 0)`
- 상대 Yaw: 약 `180도`
- Collision: `NoCollision`
- Animation tick: `AlwaysTickPoseAndRefreshBones`

이 transform은 분리 파츠의 피벗을 실제 슬롯 위치에 맞추기 위한 애셋 보정이다.
Control Rig 구현 중 이 보정을 제거하거나 Actor root를 발 위치로 간주하면 안 된다.

### 2.2 실제 다리 체인

현재 AnimBP가 사용하는 deform chain은 다음과 같다.

```text
pelvis
└─ thigh_l
   └─ calf_l
      └─ foot_l
         └─ ball_l
```

Skeleton에는 `ik_foot_root → ik_foot_l`도 있지만 deform chain의 자식이 아니다.
최초 구현의 IK 체인은 반드시 `thigh_l → calf_l → foot_l`을 사용한다.
`ik_foot_l`은 필요하면 디버그·retarget marker로만 사용하며, 검증 없이 solver의
deform effector로 바꾸지 않는다. Twist bones는 우선 부모 변환을 상속시키고,
변형 문제가 실측된 뒤에만 별도 처리를 추가한다.

프로젝트는 오른쪽 슬롯에도 왼쪽 파츠를 사용한다. 따라서 `_r` 체인으로 전환하지
않고 `PartSlotIndex` 또는 명시적인 `SideSign`으로 knee pole과 방향만 대칭 처리한다.

### 2.3 현재 AnimGraph와 교체가 필요한 부분

실제 `/Game/Chimera/ABP_CMLegLProcedural`의 흐름은 다음과 같다.

```text
Local Ref Pose
  → Modify Bone(thigh_l)
  → Modify Bone(calf_l)
  → Two Bone IK(foot_l)
  → RigidBody
  → Output Pose
```

확인된 기본값은 다음과 같다.

- `ThighMaxAngle = 18`
- `CalfMaxAngle = 25`
- `StepHeight = 25`
- `LegStepPhysicsWeight = 0.55`
- `LegCalfPhaseDelay = 0.12`
- `LegFootPhaseDelay = 0.24`

현재 Two Bone IK의 Alpha는 `LegLiftAlpha`에 연결되어 있다. 이 값은 발이 들리는
중간 구간에서 커지고 step 종료 시 0이 되므로 planted hold에 사용할 수 없다.
또한 RigidBody가 IK 뒤에 있어 최종 발 고정 포즈를 다시 변경할 수 있다.

후속 구현에서는 다음 결정을 따른다.

1. 기존 Two Bone IK는 새 Control Rig가 동작하는 시점에 제거하거나 Alpha 0으로
   비활성화한다. 같은 체인을 두 solver가 동시에 풀지 않는다.
2. Control Rig를 해당 다리의 **최종 포즈 solver**로 둔다.
3. RigidBody가 필요하면 Control Rig 앞에서만 실행한다.
4. 첫 안정화 단계에서는 solved chain의 RigidBody 영향을 0으로 검증한 뒤,
   의도한 보조 흔들림이 필요할 때만 조금씩 복구한다.

### 2.4 현재 1차 구현 결과

- `/Game/Chimera/Character/Part/Leg/CR_CMLegLProcedural`에 `thigh_l → calf_l → foot_l`
  체인용 `TwoBoneIKSimple` Forward Solve를 구성했다.
- `CTRL_CM_LegHip`, `CTRL_CM_LegFootIK`, `CTRL_CM_LegKneePole`,
  `CTRL_CM_LegPlantAlpha`를 AnimGraph 핀으로 노출했다.
- `UCMPartAnimInstance`가 Trace 결과를 component-space `FTransform`/`FVector`/Alpha로
  제공하고, AnimBlueprint getter가 네 Control Rig 입력 핀에 직접 연결된다.
- AnimGraph 최종 순서는 `TwoBoneIK(기존 procedural) → RigidBody(선택) →
  ComponentToLocal → Control Rig → Output Pose`이며, Control Rig가 최종 solver다.
- Control Rig 노드 설정은 Python에서 노드를 제거·재생성하지 않고,
  프로젝트 플러그인의 안전한 editor console bridge로만 materialize한다.
- 테스트 몸체의 8개 콜리전 마디는 실제 키메라처럼 중력을 사용한다. 시작 시 모든
  마디의 지면 관통량을 검사한 뒤 체인을 한 번만 들어 올리고, 이후에는 Chaos 접촉과
  마디 사이의 제한된 Pitch 회전으로 언덕 굴곡을 따라가게 한다.

## 3. 고정할 아키텍처 결정

### 3.1 상태 머신과 Control Rig의 역할을 분리한다

planted-foot 상태 머신은 game thread의 생산 이동 코드와 `ACMLegPart`가 소유한다.
AnimInstance와 Control Rig는 복제된 상태를 읽어 포즈만 계산한다.

```text
플레이어 다리 입력 / visual replant 조건
  → 서버 상태 머신
  → 서버 Trace
  → GroundPoint + GroundNormal 확정
  → ACMLegPart에 전이 스냅샷 저장·복제
  → UCMPartAnimInstance가 매 프레임 component space 입력 생성
  → ABP의 Control Rig가 최종 bone pose 계산
```

Control Rig는 상태를 전이하거나 Trace하지 않는다. AnimGraph 평가 중 Actor, body
segment, slot 또는 collision transform을 쓰지 않는다. 이는 animation thread와
Chaos physics 사이의 순환 의존을 막기 위한 필수 조건이다.

### 3.2 Gameplay step과 visual replant를 분리한다

다음 두 동작은 같은 swing 표현을 사용할 수 있지만 의미가 다르다.

| 원인 | 물리 힘 | 플레이어 입력 필요 | 복제 필요 | 목적 |
| --- | --- | --- | --- | --- |
| `PlayerInput` | 기존 서버 leg force 적용 | 필요 | 필요 | 실제 키메라 이동 |
| `ReachRecovery` | 절대 적용하지 않음 | 불필요 | 상태·target만 필요 | 도달 범위 회복용 시각 step |

`StepDirection`은 gameplay force 방향으로 계속 사용한다. `StepDirection=None`을
planted 또는 visual swing으로 암묵 해석하지 않는다. 별도의 plant state와 trigger가
필요하다. Visual replant가 추가 leg force, impulse 또는 cooperative input을 등록하면
안 된다.

### 3.3 Planted anchor는 월드 공간에 저장한다

발이 planted 된 동안 원본 `GroundPoint`와 `GroundNormal`은 월드 공간에서 고정한다.
매 animation update마다 현재 `PartMesh` transform의 역변환으로 component-space
target을 다시 만든다.

component-space target을 장기간 저장하면 부모 segment와 함께 이동하여 foot lock이
깨진다. 반대로 planted target을 매 Tick 현재 slot 아래로 갱신해도 발이 몸통을
따라가므로 planted-foot가 아니다.

### 3.4 몸통 물리와 시각 IK는 단방향이다

`BodyControlTarget` 또는 각 다리의 attached segment/slot transform은 thigh 시작점의
입력으로 사용할 수 있다. 그러나 Control Rig 결과를 다시 테스트 몸통이나 생산
키메라의 physics body transform에 쓰지 않는다.

몸통 높이의 실제 물리 보정이 추후 필요하면 서버 movement coordinator에서 별도
gameplay 기능으로 설계한다. 현재 Control Rig의 reach 오류를 몸통 collision 높이,
mass, constraint, impulse 변경으로 해결하지 않는다.

## 4. Planted-foot 상태 머신 계약

### 4.1 권장 권위 상태

```text
Free → Swing → Landing → Planted → Swing
  ↑                 │
  └──── Recover ←───┘
```

| 상태 | 의미 | IK solver Alpha | target 처리 |
| --- | --- | --- | --- |
| `Free` | 유효한 접지점 없음 | 0으로 완만히 해제 | 현재 pose 유지, zero target 사용 금지 |
| `Swing` | 시작점에서 새 target으로 이동 | 1 | 곡선 보간 + lift arc |
| `Landing` | 최종 지면 재확인과 settle | 1 | landing target/normal로 수렴 |
| `Planted` | 마지막 유효 접지점을 고정 | 1 | 월드 anchor 고정 |
| `Recover` | detach·disable·trace 실패·복제 단절 정리 | 1→0 | 마지막 유효 pose에서 blend out |

`Lift`, `Transfer`, `Lower`는 네트워크 Enum으로 별도 복제하지 않는다. `Swing`의
정규화 phase로 계산하는 표현 sub-phase로 둔다. 상태와 모든 접촉 데이터를 하나의
전이 스냅샷으로 복제해 필드별 도착 순서가 섞이지 않게 한다.

### 4.2 필요한 데이터의 개념 계약

후속 구현자는 이름을 프로젝트 규칙에 맞게 조정할 수 있지만 다음 의미를 보존한다.

```text
ECMLegPlantState
  Free, Swing, Landing, Planted, Recover

ECMLegPlantTrigger
  Initialization, PlayerInput, ReachRecovery, Emergency

FCMLegGroundContact
  bValid
  WorldLocation
  WorldNormal

FCMLegPlantTransition
  State
  Trigger
  StartContact
  TargetContact
  ServerStartTime
  Duration
  Sequence
```

현재의 `StepGroundLocation(FVector_NetQuantize10)`과
`StepGroundNormal(FVector_NetQuantizeNormal)` 정밀도는 유지할 수 있다. 다만
`StepGround*`을 swing target으로만 사용할지, 새 구조체로 합칠지는 원자적 복제가
보장되는 쪽을 선택한다.

`EndProceduralStep()`은 gameplay force window만 끝내야 한다. 성공한 landing target을
planted anchor로 승격한 뒤 target을 지우면 안 된다.

### 4.3 정상 전이

| 현재 상태 | 조건 | 다음 상태 | 처리 |
| --- | --- | --- | --- |
| `Free` | 초기 접지 Trace 성공 | `Planted` | 시각적인 큰 swing 없이 anchor 획득 |
| `Free` | 입력 target Trace 성공 | `Swing` | 입력 step과 함께 시작 |
| `Planted` | 플레이어 입력 + 유효 target | `Swing` | gameplay force와 visual step 동시 시작 |
| `Planted` | planar reach 오차가 release 임계 초과 | `Swing` | `ReachRecovery`, force 없음 |
| `Swing` | phase가 landing 구간 진입 | `Landing` | target 지면을 game thread에서 재검증 |
| `Landing` | 최종 contact 유효 + phase 종료 | `Planted` | target을 새 월드 anchor로 승격 |
| `Landing` | contact 무효 + grace 초과 | `Recover` | 마지막 pose에서 IK 해제 |
| `Recover` | blend out 종료 | `Free` | stale target 폐기 |

사망, disabled, detach, 파츠 파괴, attached segment 비활성화는 모든 상태에서
`Recover`로 전이한다. Actor teleport 또는 reach의 절대 한계를 넘는 순간 이동은
일반 swing으로 억지 연결하지 말고 `Recover → Free/Planted 재획득`으로 처리한다.

### 4.4 임계값과 hysteresis

planted anchor와 현재 slot 기반 발 home 후보의 차이를 지면 접선 평면에 투영한다.

```text
PlanarError = Length(ProjectOnPlane(HomePoint - PlantedPoint, GroundNormal))
```

- `PlanarError > ReplantReleaseDistance`이면 visual replant 후보가 된다.
- 새 anchor 획득 뒤 `PlanarError < ReplantSettleDistance`여야 안정 상태로 본다.
- `ReplantSettleDistance`는 반드시 release 값보다 작게 둬 경계 떨림을 막는다.
- chain 최대 도달거리 비율도 별도로 검사한다. planar 거리만으로 vertical reach를
  놓치면 안 된다.

초기값은 고정 cm보다 실제 `thigh_l/calf_l` chain 길이의 비율로 산정한다.

- release: 최대 reach의 약 `45~55%`
- settle: release의 약 `60~70%`
- emergency: 최대 reach의 약 `90~95%`

이 값은 시작점일 뿐이며 에디터 조정값으로 노출한다.

### 4.5 swing 중 새 입력

동일 다리가 이미 visual swing 중이어도 승인된 gameplay 입력의 물리 힘은 기존
규칙대로 적용한다. 시각 target은 다음처럼 처리한다.

- landing 시작 전: 새 authoritative target으로 한 번 retarget하되 현재 foot 위치를
  새 시작점으로 캡처하여 튀지 않게 한다.
- landing 시작 후: 현재 landing을 끝낸 뒤 실행할 최신 target 하나만 보관한다.
- phase를 0으로 무조건 재시작하거나 여러 target을 무제한 queue하지 않는다.

여러 다리의 visual replant가 동시에 발생하는 것이 보기 나쁘면 segment와 slot
address로 결정적인 짧은 stagger를 적용할 수 있다. 이는 시각 스케줄링일 뿐 입력
force 타이밍을 늦추면 안 된다.

## 5. 필수 파이프라인: Trace → Ground → IK Target → Control Rig

### 5.1 1단계: 서버 Trace

생산 구현의 `UCMLineBodyMovementCoordinator::TraceGroundAtPoint()`를 기준으로 한다.

- 시작: `DesiredFootPoint + WorldUp * LegStepTraceHeight`
- 끝: `DesiredFootPoint - WorldUp * LegStepTraceDepth`
- shape: sphere sweep
- radius: `GroundCheckRadius`
- channel: 기존 `GroundTraceChannel`
- fallback: `WorldStatic` object sweep
- ignore: 키메라, 대상 다리, 키메라가 권위 상태로 관리하는 모든 부착 파츠
- walkable: `ImpactNormal.Z >= MinimumGroundNormalZ`

현재 기본값은 다음과 같다.

| 값 | 현재 기본값 | 판단 |
| --- | ---: | --- |
| `LegStepTraceHeight` | 60 | 유지, 중복 상수 생성 금지 |
| `LegStepTraceDepth` | 140 | 유지, 중복 상수 생성 금지 |
| `GroundCheckRadius` | 12 | 유지 |
| `MinimumGroundNormalZ` | 0.5 | 약 60도까지 허용 |
| `LegStepLength` | 100 | gameplay target 산정에 유지 |

다음 hit는 거부한다.

- `bStartPenetrating == true`
- 위치 또는 Normal에 NaN/Inf가 있음
- Normal 길이가 거의 0
- `Normal.Z < MinimumGroundNormalZ`
- 자기 몸통·부착 파츠에서 발생한 hit

Multi Sweep 결과는 반환 배열 순서를 맹신하지 말고 sweep `Time` 또는 거리 기준으로
가장 가까운 유효 walkable hit를 선택한다.

### 5.2 Trace 호출 빈도

Trace를 AnimGraph나 Control Rig의 매 평가에서 실행하지 않는다.

- 초기 anchor 획득 시 1회
- 플레이어 입력 target을 정할 때 1회
- visual replant target을 정할 때 1회
- `Landing` 구간에서 최종 contact 검증을 위해 제한된 주기로 재실행 가능
- `Planted` 상태의 저주기 검증 Trace는 anchor를 움직이지 않고 유효성만 확인

Planted 중 Trace 결과로 매 Tick anchor를 덮어쓰면 foot lock이 사라진다.

테스트 몸통의 현재 단순 line trace는 생산 sphere sweep과 결과가 다를 수 있다.
후속 구현은 공용 ground-contact helper 또는 동일 계약을 사용해 테스트 harness와
생산 키메라가 같은 지면 판단을 하게 한다.

### 5.3 2단계: Ground contact 확정

동일한 hit에서 다음 한 쌍을 확정한다.

```text
GroundPoint  = Hit.ImpactPoint
GroundNormal = Normalize(Hit.ImpactNormal)
```

Normal만 복제한 뒤 위치를 재추정하거나, 서로 다른 두 Trace에서 위치와 Normal을
섞지 않는다. 최초 범위는 WorldStatic 접지다. 움직이는 플랫폼은 component-local
anchor와 replicated component reference가 필요하므로 별도 요구사항 전에는 구현하지
않는다.

### 5.4 3단계: 월드 IK target 계산

#### 위치

발 bone origin과 실제 발바닥의 차이를 `SoleContactOffset`으로 둔다.

```text
ContactPositionWS = GroundPoint + GroundNormal * SoleContactOffset
```

오프셋은 반드시 `foot_l`과 실제 sole 위치를 Control Rig 에디터에서 측정해 결정한다.
현재 `GroundContactDistance = 8`은 다른 의미의 기존 값이며 실측 없이 sole offset으로
재사용하지 않는다.

Swing target은 시작 contact와 target contact 사이에서 계산한다.

```text
MoveAlpha = SmoothStep(0, 1, Phase)
Base      = Lerp(StartPoint, TargetPoint, MoveAlpha)
Lift      = sin(PI * Phase) * StepHeight
LiftAxis  = Normalize(Lerp(StartNormal, TargetNormal, MoveAlpha))
FootPosWS = Base + LiftAxis * Lift
```

`LiftAxis`가 퇴화하면 World Up을 사용한다. 시작·끝 Normal 모두 walkable 범위이므로
보간 Normal이 아래를 향하지 않도록 검증한다.

#### 경사면 회전

`asin(Normal.Z)`만으로 pitch/roll을 만들지 않는다. 몸통의 진행 yaw를 보존하는 전체
접선 기저가 필요하다.

```text
Up      = Normalize(GroundNormal)
Forward = ProjectOnPlane(DesiredLegForward, Up)
Forward = Normalize(Forward)
Right   = Normalize(Cross(Up, Forward))
Forward = Normalize(Cross(Right, Up))
```

`Forward`가 거의 0이면 다음 순서로 fallback한다.

1. 이전 유효 foot forward를 새 Normal 평면에 투영
2. 현재 skeletal component forward를 투영
3. 마지막으로 임의 직교축 생성

위 기저로 ground frame Quaternion을 만든 뒤 foot bone의 reference 축과 sole 축을
맞추는 `FootCalibrationRotation`을 곱한다. `male_Leg_L`의 bone local X/Y/Z를 일반
인체 스켈레톤이라고 가정하지 않는다. flat-ground preview에서 calibration을 먼저
저장한 뒤 경사면으로 간다.

오른쪽 슬롯도 왼쪽 mesh를 사용하므로 음수 scale 또는 180도 회전 여부를 실측한다.
Quaternion 생성 전에 transform determinant와 `SideSign`을 확인하여 Normal, tangent,
knee 방향이 반전되지 않게 한다.

#### knee pole

Effector와 knee pole은 별개다.

- Effector: `FootPosWS` 및 foot target rotation
- Knee pole: thigh-calf 체인의 굽힘 평면을 결정하는 위치

pole 방향은 현재 또는 reference pose의 무릎 방향을 기준으로 계산하고, slot side로
대칭시킨다. `GroundNormal`, `StepHeight`, 고정된 component Y값을 knee pole 위치로
직접 사용하지 않는다. Hip과 effector가 거의 일직선이어도 이전 유효 pole을 유지해
무릎 flip을 막는다.

#### reach 제한

실측한 upper/lower chain 길이로 최대 reach를 계산한다.

- solver stretch는 최초 버전에서 끈다.
- target은 약 `0.95 * MaxReach` 안으로 clamp한다.
- emergency reach를 넘으면 visual replant 또는 `Recover`로 전환한다.
- IK로 몸통을 당기거나 다리 scale을 무한히 늘이지 않는다.

### 5.5 4단계: component/rig space 변환

월드 target 계산을 모두 끝낸 뒤 현재 `PartMesh` transform으로 한 번 변환한다.

```cpp
FootPositionCS = MeshTransform.InverseTransformPosition(FootPositionWS);
GroundNormalCS = MeshTransform.InverseTransformVectorNoScale(GroundNormalWS);
FootRotationCS = MeshTransform.InverseTransformRotation(FootRotationWS);
KneePoleCS     = MeshTransform.InverseTransformPosition(KneePoleWS);
```

현재 `UCMPartAnimInstance`의 `WorldLocationToComponent()`와
`WorldDirectionToComponent()` 경로를 재사용한다. 생산 `ACMChimera`의 Actor root는
segment 복제의 기준이 아니므로 body frame으로 사용하지 않는다.

### 5.6 5단계: Control Rig 적용

기능 응집 원칙에 따른 권장 새 애셋 경로는 다음과 같다.

```text
/Game/Chimera/Character/Part/Leg/CR_CMLegLProcedural
```

기존 AnimBP를 옮기는 작업은 이번 범위가 아니다. Redirector를 만들기 위해
`/Game/Chimera/ABP_CMLegLProcedural`을 임의 이동하지 않는다.

Control Rig에는 최소한 다음 의미의 입력을 둔다.

- thigh/hip 기준 transform
- foot effector transform
- knee pole position
- solver Alpha
- `SideSign`

권장 control 이름은 다음과 같으며 프로젝트 에셋에서 충돌 시 의미를 유지한 채
조정할 수 있다.

```text
CTRL_CM_LegHip
CTRL_CM_LegFootIK
CTRL_CM_LegKneePole
```

Forward Solve 순서는 다음과 같다.

1. 입력 target 유효성, finite 값 및 Alpha 확인
2. `thigh_l` 시작 transform에 body/slot 기반 hip target 반영
3. `thigh_l → calf_l → foot_l` IK solve
4. `foot_l`의 최종 회전을 slope-aligned target으로 설정
5. 결과를 입력 pose에 blend

Backward Solve는 최초 런타임 구현에 필요하지 않다. Control Rig가 body transform이나
planted 상태를 쓰는 역방향 경로를 만들지 않는다.

권장 최종 AnimGraph는 다음과 같다.

```text
Base Pose
  → 기존 procedural motion 중 유지하기로 검증한 부분
  → RigidBody(선택, solved chain 영향 제한)
  → Control Rig(planted-foot 최종 solve)
  → Output Pose
```

solver Alpha는 `LegLiftAlpha`가 아니다.

- `Planted`, `Swing`, `Landing`: 기본 1
- `Free`: 0
- `Recover`: 짧은 시간에 1→0

Swing에서도 solver가 계산한 곡선 target을 따라야 하므로 Alpha를 lift 곡선으로
껐다 켜지 않는다.

## 6. 생산 코드에 요구되는 책임 분배

### 6.1 `UCMLineBodyMovementCoordinator`

- gameplay 다리 입력 승인과 기존 horizontal force를 계속 소유한다.
- 지면 Trace 및 접촉 검증을 소유한다.
- visual replant 조건을 평가하되 force를 생성하지 않는다.
- slope tangent를 gameplay force에 사용하지 않는다. 현재 코드처럼 힘은 수평으로
  유지한다. Ground Normal은 시각 IK 정렬에만 사용한다.

이 구분은 경사 접선 force의 위쪽 성분으로 몸통이 발사되던 문제를 재발시키지 않기
위한 필수 조건이다.

### 6.2 `ACMLegPart`

- 권위 plant transition snapshot을 저장하고 복제한다.
- gameplay `StepDirection`과 visual plant state를 별도로 유지한다.
- 성공한 landing target을 planted anchor로 보존한다.
- detach, death, disabled, segment inactive에서 상태를 정리한다.
- 상태 변경 시 `ForceNetUpdate()`와 하나의 `OnRep` 경로를 사용한다.

### 6.3 `UCMPartAnimInstance`

- `ACMLegPart`의 snapshot을 읽는다.
- 서버 기준 phase를 계산한다.
- 월드 target을 현재 mesh component space로 변환한다.
- Control Rig에 필요한 위치, 회전, pole, Alpha를 BlueprintReadOnly 입력으로 제공한다.
- 상태 전이, Trace, force 또는 Actor transform 변경은 하지 않는다.

현재 `GetStepPhase()`는 로컬 `UWorld::GetTimeSeconds()`와 복제된 서버 시작시각을
직접 비교한다. 원격 client phase를 안정화하려면 `GameState`의 동기화된 서버 시간
등 명시적인 server-clock 기준을 사용한다. per-frame phase를 복제하지 않는다.

### 6.4 테스트 몸체

- 매 Tick의 입력 시뮬레이션은 실제 다리 입력 API와 같은 경로를 호출한다.
- 애니메이션만 시작하고 몸통 force를 생략하는 별도 경로를 만들지 않는다.
- 8개 segment의 16개 slot은 각각 자신의 attached segment transform을 사용한다.
- visual replant는 같은 상태 및 target 계산 helper를 사용하되 force를 적용하지 않는다.
- `BodyControlTarget`은 hip/thigh 입력 또는 디버그 기준점으로만 사용한다.
- 기존 body collision 높이, 질량, constraint, skeletal visual 계층은 변경하지 않는다.

## 7. 멀티플레이 계약

- gameplay 입력 검증, Trace, 상태 전이 및 target 확정은 서버 전용이다.
- 클라이언트 Control Rig는 복제 snapshot으로 렌더링한다.
- 매 프레임 IK transform, smoothing 결과 또는 Trace hit를 복제하지 않는다.
- transition snapshot에는 증가하는 `Sequence`를 넣어 오래된 packet을 무시한다.
- 위치, Normal, state, trigger, start time, duration은 한 묶음으로 갱신한다.
- 새 snapshot의 작은 target 오차는 약 `0.05~0.10초` 동안 시각 보간한다.
- teleport 수준 오차는 이전 anchor로 긴 보간하지 않고 `Recover`로 재획득한다.
- 초기 구현에는 client-side target prediction을 추가하지 않는다.
- dedicated server에서 Control Rig/AnimGraph 평가가 gameplay 상태의 필수 조건이면 안 된다.
- 복제 단절 timeout 후 마지막 planted target을 영구 유지하지 않는다.

테스트 순서는 listen server의 host, owning client, remote observer를 모두 포함한다.
packet delay/loss 환경에서 state가 영구 `Swing` 또는 `Planted`로 고정되지 않아야 한다.

## 8. 튜닝 파라미터 판단

| 파라미터 | 시작 판단 | 비고 |
| --- | --- | --- |
| `StepHeight` | 현재 25 유지 | 경사·장애물 clearance 후 조정 |
| `ActionDuration` | 런타임 part data 사용 | C++ 기본 0.25, 현재 BP CDO는 0.5로 확인됨 |
| `LandingStartRatio` | 약 0.75 | 절대 초가 아닌 정규화 phase |
| `SoleContactOffset` | 실측 전 0 | `foot_l` bone origin을 에디터에서 측정 |
| `MaxReachRatio` | 약 0.95 | stretch 비활성 상태 |
| `ReplantReleaseDistance` | reach의 45~55% | 에디터 조정 |
| `ReplantSettleDistance` | release의 60~70% | release보다 작아야 함 |
| `InvalidContactGrace` | 0.05~0.10초부터 검증 | 한 frame trace 실패로 풀리지 않게 함 |
| `TargetReconcileTime` | 0.05~0.10초 | 큰 오차에는 사용하지 않음 |
| `NormalSmoothing` | 지수 보간형 | planted anchor 원본은 바꾸지 않음 |
| `KneePoleOffset` | skeleton 실측 | slot side로 대칭 |

`LegCalfPhaseDelay`, `LegFootPhaseDelay`, `ThighMaxAngle`, `CalfMaxAngle`은 기존 표현과
Control Rig 결과를 비교한 뒤 유지 여부를 판단한다. 의미가 겹치는 값을 새 Rig에
그대로 복제하지 않는다.

## 9. 디버그 표현 요구사항

개발 빌드에서 다리별로 다음을 선택적으로 표시할 수 있어야 한다.

- Trace start/end와 sphere radius
- 선택된 `GroundPoint` 점
- `GroundNormal` 선
- planted anchor와 landing target을 다른 색으로 표시
- foot IK target transform 축
- knee pole 위치와 hip-knee-pole 선
- current state, trigger, phase, sequence, reach ratio 텍스트
- rejected hit의 이유: penetration, slope, self/part, invalid normal, no ground

디버그 draw는 서버 결과와 client 렌더 target을 색으로 구분한다. Shipping에서는
비용이 없어야 하며, 로그는 상태 전이 시에만 남기고 매 Tick spam을 만들지 않는다.

## 10. 구현 단계

후속 LUNA XHIGH 구현자는 아래 순서를 바꾸지 않는다.

### 단계 0: Preflight

1. 이 문서를 끝까지 읽는다.
2. dirty worktree의 기존 사용자 변경을 확인하고 보존한다.
3. Control Rig/RigVM 플러그인 상태를 확인한다.
4. `foot_l` 축, sole 위치, 왼쪽 mesh의 오른쪽 슬롯 mirror 방식을 에디터에서 실측한다.
5. 현재 AnimGraph 연결과 BP/DataTable의 실제 runtime duration을 다시 기록한다.
6. 문서와 다른 사실이 있으면 구현 전에 보고한다.

### 단계 1: 순수 데이터와 수학

1. plant/contact/transition 타입과 서버 시간 기준을 추가한다.
2. walkable hit 검증, slope frame, swing curve, reach clamp, knee pole 계산을 가능한 한
   순수 함수로 분리한다.
3. 단위 테스트로 NaN, 경계 Normal, hysteresis, phase를 먼저 검증한다.

### 단계 2: 서버 상태 머신

1. 기존 player input step을 새 plant transition과 연결한다.
2. gameplay force와 visual replant를 분리한다.
3. landing 승격, trace 실패, detach/disable/death 정리를 구현한다.
4. 단일 replicated snapshot과 sequence를 검증한다.

### 단계 3: Control Rig 한 다리 검증

1. 왼쪽 다리용 Control Rig를 생성한다.
2. flat ground에서 chain, foot calibration, knee pole을 검증한다.
3. 기존 Two Bone IK 중복을 제거한다.
4. RigidBody 앞뒤 순서를 바로잡고 Control Rig를 최종 solver로 둔다.
5. flat ground에서 planted hold가 된 뒤 경사면을 검증한다.

### 단계 4: 테스트 몸체 확장

1. 테스트 몸체가 production과 동일한 ground-contact 계약을 사용하게 한다.
2. 실제 버튼 입력 시뮬레이션이 force와 visual step을 모두 발생시키는지 확인한다.
3. 8마디·16슬롯 각각이 자신의 segment 기준으로 동작하는지 확인한다.
4. 전진과 좌·우 회전 delay 시뮬레이션에서 anchor와 knee가 섞이지 않는지 확인한다.

### 단계 5: 네트워크 및 실패 경로

1. listen server, owning client, remote observer를 비교한다.
2. packet delay/loss, no ground, steep slope, reach 초과, teleport를 시험한다.
3. death, disabled, detach, segment inactive 정리를 확인한다.
4. 기존 몸통 collision, constraint, physics replication 회귀 테스트를 실행한다.

각 단계가 실패하면 다음 단계로 진행하지 않는다. Control Rig 튜닝으로 서버 상태 또는
collision 문제를 가리지 않는다.

## 11. 검증 기준

### 11.1 자동화 테스트

- World → Component → World transform round trip 오차가 허용 범위 안이다.
- ground tangent frame의 Forward/Right/Up이 정규화·직교한다.
- zero/invalid Normal이 안전하게 거부된다.
- `Normal.Z == 0.5` 경계는 수용되고 더 작은 값은 거부된다.
- phase가 0, landing 경계, 1에서 올바른 상태를 만든다.
- planted 중 원본 월드 target이 부모 segment 이동에도 바뀌지 않는다.
- release/settle hysteresis가 경계에서 반복 전이하지 않는다.
- reach 초과 target이 clamp되고 NaN을 만들지 않는다.
- 오래된 sequence가 최신 plant 상태를 덮지 않는다.
- visual replant가 gameplay force를 생성하지 않는다.

### 11.2 PIE 수동 테스트

1. 평지에서 정지한 발이 10초 동안 눈에 띄게 drift하지 않는다.
2. 몸통 이동이 release 임계 미만이면 foot는 고정되고 thigh/calf 관절만 움직인다.
3. 임계를 넘으면 foot가 한 번 올라갔다 새 target에 착지하며 chatter하지 않는다.
4. `Test_Incline`에서 발바닥 위치가 지면에 닿고 foot Up이 Normal과 정렬된다.
5. 약 15/30/45도 경사에서 정상이며 최대 slope 경계 바로 위는 거부된다.
6. 오른쪽 슬롯에서 knee가 안쪽으로 뒤집히거나 foot tangent가 반전되지 않는다.
7. 서로 다른 segment의 다리가 동시에 움직여도 state와 target이 섞이지 않는다.
8. 플레이어 입력 step은 기존과 같은 방향의 실제 body movement를 만든다.
9. visual replant만으로 body가 이동하거나 회전하지 않는다.
10. no ground와 ledge에서 stale target, 무한 stretch, 순간이동이 없다.
11. 몸통이 하늘로 튀거나 폭발적인 각속도를 얻지 않는다.
12. listen server와 원격 client에서 착지점과 상태 전이가 허용 오차 안에서 일치한다.

### 11.3 완료 조건

- 8개 segment와 16개 slot의 다리가 개별 attached segment 기준으로 동작한다.
- 경사면에서 동일 Trace의 위치와 Normal로 IK target을 만든다.
- planted 발은 부모 segment 이동과 독립된 월드 anchor를 유지한다.
- Control Rig가 `thigh_l → calf_l → foot_l`의 유일한 최종 IK solver다.
- 서버가 Trace와 state를 소유하며 per-frame IK를 복제하지 않는다.
- Control Rig가 body physics나 collision을 변경하지 않는다.
- 기존 몸통 이동, 충돌, constraint, segment replication 테스트가 계속 통과한다.
- 실패 경로에 stale target, NaN, crash 또는 영구 lock이 없다.

## 12. 금지 사항

- Control Rig에서 Trace하거나 gameplay state를 전이하지 않는다.
- Control Rig 결과로 body segment, Actor root 또는 collision을 이동하지 않는다.
- client Trace를 권위 gameplay 결과로 사용하지 않는다.
- 기존 Two Bone IK와 Control Rig를 같은 chain에 full Alpha로 중복 적용하지 않는다.
- RigidBody를 최종 Control Rig 뒤에 두지 않는다.
- world-space point를 component-space pin에 직접 연결하지 않는다.
- `asin(Normal.Z)`만으로 경사면 foot rotation을 만들지 않는다.
- `GroundNormal` 또는 `StepHeight`를 knee pole로 재사용하지 않는다.
- `StepDirection=None`을 planted 상태로 간주하지 않는다.
- planted anchor를 현재 slot 위치로 매 Tick 덮어쓰지 않는다.
- visual replant에서 impulse 또는 movement force를 적용하지 않는다.
- reach 초과를 bone scale 또는 무제한 stretch로 숨기지 않는다.
- `GroundContactDistance`를 실측 없이 sole offset으로 재사용하지 않는다.
- 오른쪽 슬롯의 축·mirror를 `_r` bone을 사용하면 된다고 추측하지 않는다.
- 리그 튜닝을 이유로 현재 적절한 몸통 collision, mass, constraint를 변경하지 않는다.
- moving platform 지원을 component-local anchor 없이 흉내 내지 않는다.

## 13. 후속 LUNA XHIGH 작업 보고 형식

후속 구현자는 각 단계 종료 시 다음을 짧게 보고한다.

```text
1. 이번 단계에서 구현한 계약
2. 실제 수정 파일/애셋
3. 문서와 달랐던 프로젝트 사실
4. 실행한 자동화·PIE·네트워크 검증과 결과
5. 다음 단계 진입 조건 충족 여부
6. 남은 위험 또는 에디터 수동 확인 항목
```

문서와 다른 결정을 내려야 한다면 변경 이유, 대안, physics/network 영향과 검증법을
먼저 제시한다. 단순히 애니메이션이 자연스러워 보인다는 이유로 권한 경계나 충돌
안정성 결정을 바꾸지 않는다.
