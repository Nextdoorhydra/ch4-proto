# 키메라 비주얼 보강 구현 계약

> 문서 상태: Phase 5 키메라 이동 궤적 decal stamp pool 구현 완료
> 작성 기준일: 2026-09-07
> 대상 엔진: Unreal Engine 5.7
> 다음 구현 단계: 타겟 메시를 휘감는 촉수

## 1. 목적

이 문서는 `BP_CMChimera`의 머리·몸통·꼬리 표현과 후속 VFX를 추가할 때
기존 물리, 파츠 슬롯, 마디 체력, 네트워크 권한을 보존하기 위한 구현 계약이다.

후속 구현자는 이 문서의 **고정 계약**을 기준으로 작업한다. 최종 모델링 애셋을
임포트했을 때 본, 피벗, 샘플링 영역이 이 문서와 다르면 코드에서 임의로 우회하지
말고 애셋 계약을 먼저 수정하거나 차이를 명시적으로 승인받는다.

## 2. 이번 페이즈의 범위

이번 페이즈에서 확정하는 내용은 다음과 같다.

- 기존 `BodyMesh_n`의 게임플레이 책임
- 머리·몸통·꼬리의 시각 역할과 인덱스 규칙
- 마디 Blueprint와 `TentacleActor`의 소유 및 수명주기
- 모델링, 스켈레톤, Physics Asset, 샘플링 영역 제작 규격
- Death, AnimBP/Control Rig, 표면 촉수, 궤적, Niagara의 시스템 경계
- 다음 페이즈 진입 조건과 회귀 검증 항목

이번 페이즈에서는 C++ 동작, Blueprint, `.uasset`을 변경하지 않는다.

## 3. 확인된 현재 상태

| 영역 | 현재 구현 | 보존해야 할 계약 |
| --- | --- | --- |
| 몸통 마디 | `ACMChimera`가 최대 16개의 `UBoxComponent`를 `BodySegments`로 생성 | `BodyMesh_n`은 렌더 메시가 아니라 물리·이동·마디 위치의 기준이다 |
| 플레이어 비율 | 플레이어당 2마디, 마디당 2슬롯, 최대 8인 | 최대 16마디·32슬롯 주소 체계를 유지한다 |
| 슬롯 주소 | `SegmentIndex * 2 + PartSlotIndex` | 머리·몸통·꼬리 역할이 바뀌어도 주소는 바뀌지 않는다 |
| 마디 체력 | 서버가 `SegmentHealthStates`를 변경·복제 | 시각 파괴는 이 상태를 표현할 뿐 별도 체력을 만들지 않는다 |
| 마디 사망 | 파츠 분리, 조작 잠금 후 `OnSegmentDestroyed` 방송 | 기존 사망 순서와 서버 권한을 유지한다 |
| 네트워크 물리 | 서버가 각 마디 상태를 복제하고 클라이언트가 적용 | 시각 메시가 물리 권한이나 Actor 이동 복제를 소유하지 않는다 |
| 촉수 파츠 부착 | `ACMTentacleSegmentActor`가 서버에서 탐지·예약·당기기를 수행 | 타겟 선택과 파츠 부착 판정은 계속 서버 권한이다 |
| 촉수 생성 | `RefreshTentacleSegments()`가 활성 마디별 Actor를 Spawn/Destroy | 다음 페이즈에서 제작 시점 배치 방식으로 교체할 대상이다 |
| 기존 콘텐츠 | `BP_CMChimera`, `BP_CMTentacleSegment` 존재 | 기존 Blueprint 경로와 파츠 부착 기능을 회귀 검증한다 |
| 새 캐릭터 메시 | 프로젝트 전용 Head/Body/Tail SKM은 아직 없음 | 최종 애셋 경로와 실제 본 목록은 임포트 후 검증해야 한다 |

관련 기준 소스는 다음과 같다.

- `Source/Chimera/Player/CMControlTypes.h`
- `Source/Chimera/Player/CMChimera.h`
- `Source/Chimera/Player/CMChimera.cpp`
- `Source/Chimera/Player/CMChimeraHealth.cpp`
- `Source/Chimera/Parts/Tentacle/CMTentacleSegmentActor.*`
- `Source/Chimera/Tests/CMTentacleAttachmentTests.cpp`

## 4. 고정 아키텍처 계약

### 4.1 물리 마디와 시각 마디를 분리한다

`BodyMesh_n`은 이름과 무관하게 계속 `UBoxComponent` 물리 프록시다. 새
Skeletal Mesh는 이 컴포넌트를 교체하지 않고 자식 표현 계층에 들어간다.

목표 계층은 다음과 같다.

```text
BP_CMChimera / ACMChimera
└─ BodyMesh_n                         # 기존 UBox, 게임플레이/물리 기준
   ├─ LeftPartSlot_n                  # 기존 슬롯
   ├─ RightPartSlot_n                 # 기존 슬롯
   ├─ SegmentHurtbox_n                # 기존 피격 판정
   └─ SegmentPresentation_n           # 제작 시점에 구성된 ChildActorComponent
      └─ BP_CMChimeraBodySegment
         ├─ BodyVisual                # Head/Body/Tail 공용 SkeletalMeshComponent
         ├─ TentacleActor             # BP_CMTentacleSegment ChildActorComponent
         ├─ IdleTentacleRoot          # 후속 로컬 표현
         └─ UnderbodyArms              # 후속 NiagaraComponent
```

`SegmentPresentation_n`은 최대 16개를 제작 시점에 가진다. 플레이어 수가 줄어든
경우 Actor나 Child Actor를 파괴하지 않고 다음 항목을 비활성화한다.

- 시각 메시 렌더링 및 애니메이션 Tick
- TentacleActor의 탐지 Collision과 Tick
- Idle/Wrap 촉수 갱신
- Niagara 방출

Unreal의 `ChildActorComponent`가 내부적으로 Child Actor 인스턴스를 materialize하는
것은 허용한다. 금지하는 것은 `ACMChimera` 게임 코드가 플레이어 수 변경 때마다
직접 `SpawnActor/Destroy`로 촉수 수명주기를 관리하는 현재 방식이다.

### 4.2 머리·몸통·꼬리는 시각 역할이다

게임플레이 코드에서 세 종류를 별도 체력이나 별도 슬롯 타입으로 취급하지 않는다.
향후 C++ 식별자가 필요하면 `ECMChimeraSegmentVisualRole` 하나만 추가한다.

역할 계산은 활성 마디 수와 인덱스만 사용한다.

```text
SegmentIndex == 0                       -> Head
SegmentIndex == ActiveSegmentCount - 1 -> Tail
그 외                                   -> Body
```

필수 예시는 다음과 같다.

| 플레이어 수 | 활성 마디 수 | 시각 역할 |
| ---: | ---: | --- |
| 1 | 2 | Head, Tail |
| 2 | 4 | Head, Body, Body, Tail |
| 3 | 6 | Head, Body, Body, Body, Body, Tail |
| 8 | 16 | Head, Body × 14, Tail |

정상 게임 흐름에서는 활성 마디가 항상 2개 이상이어야 한다. 현재 코드가 비정상적인
0인 입력을 1마디로 Clamp하는 경로는 이번 비주얼 작업에서 의미를 확장하지 않는다.
해당 상태에서는 0번을 Head로 표시하고 경고 로그를 남기는 방어 동작만 허용한다.

### 4.3 역할 변경은 인스턴스 교체가 아니라 프리셋 변경이다

플레이어 수가 변하면 기존 중간 마디가 Tail에서 Body로, 또는 Body에서 Tail로
바뀔 수 있다. 이때 Child Actor Class를 교체하지 않는다.

`BP_CMChimeraBodySegment`의 안정적인 한 인스턴스에 다음 프리셋을 적용한다.

- `Head`: Head SKM, Head Anim Class, Head VFX 설정
- `Body`: Body SKM, Body Anim Class, Body VFX 설정
- `Tail`: Tail SKM, Tail Anim Class, Tail VFX 설정

세 메시가 공용 Skeleton을 사용하므로 `BodyVisual`은 하나만 유지한다. 역할 변경 시
상태를 정리한 뒤 Skeletal Mesh와 Anim Class를 적용한다. 파츠 슬롯, 체력,
TentacleActor의 `SegmentIndex`는 역할 변경과 무관하게 유지한다.

### 4.4 네트워크 권한을 분리한다

| 기능 | 권한/동기화 |
| --- | --- |
| 활성 마디 수 | 기존 `ActiveSegmentCount` 서버 복제 |
| Head/Body/Tail 역할 | 각 클라이언트가 복제된 활성 마디 수와 고정 인덱스로 결정 |
| 마디 체력·사망 | 기존 `SegmentHealthStates` 서버 복제 |
| 파츠 탐지·예약·당기기 | 기존 TentacleActor 서버 권한 및 상태 복제 |
| Idle 촉수 | 로컬 cosmetic, seed만 결정적으로 생성 가능 |
| 타겟 휘감기 촉수 | 기본은 로컬 cosmetic, 실행 여부와 타겟이 게임플레이에 영향 없음 |
| 궤적 데칼 | 로컬 cosmetic, 복제하지 않음 |
| 하부 팔 Niagara | 로컬 cosmetic, 복제하지 않음 |
| 외피 래그돌 | 사망 상태를 받아 각 클라이언트가 표현, 물리 결과 자체는 복제하지 않음 |

휘감기 촉수가 타겟 이동이나 판정에 영향을 주는 기능으로 변경될 경우 이 계약을
사용할 수 없다. 그 경우 별도의 서버 상태와 복제 설계를 먼저 추가한다.

## 5. 모델링 및 임포트 계약

### 5.1 공통 규격

- Unreal 좌표 기준은 `+X` 전방, `+Y` 우측, `+Z` 상단으로 맞춘다.
- 단위는 centimeter, 임포트 Scale은 `(1, 1, 1)`로 맞춘다.
- Head/Body/Tail의 마디 연결 기준 원점과 전방축을 동일하게 유지한다.
- 세 메시의 bind pose와 기준 root hierarchy는 동일해야 한다.
- 세 메시를 하나의 공용 Skeleton에 연결할 수 있어야 한다.
- 슬롯 위치 보정을 메시 피벗에 숨기지 않는다. 필요한 오프셋은 마디 프리셋에 둔다.
- Gameplay Collision은 만들지 않는다. 캐릭터 충돌은 기존 `BodyMesh_n`이 담당한다.
- Material Slot 이름과 순서는 LOD 전체에서 유지한다.

권장 콘텐츠 경로는 기능 응집성 규칙에 따라 다음과 같이 둔다.

```text
/Game/Chimera/Character/Chimera/Blueprint/BP_CMChimeraBodySegment
/Game/Chimera/Character/Chimera/Mesh/SK_CMChimeraHead
/Game/Chimera/Character/Chimera/Mesh/SK_CMChimeraBody
/Game/Chimera/Character/Chimera/Mesh/SK_CMChimeraTail
/Game/Chimera/Character/Chimera/Animation/ABP_CMChimeraSegment
/Game/Chimera/Character/Chimera/Animation/CR_CMChimeraSegment
/Game/Chimera/Character/Chimera/Niagara/NS_CMChimeraUnderbodyArms
```

실제 애셋이 생성되기 전에는 위 경로를 코드의 Hard Reference로 추가하지 않는다.

### 5.2 Death용 외피와 심지

각 마디는 가운데 촉수 심지와 서로 분리 가능한 외피 3조각으로 제작한다.
공용 Skeleton에는 최소한 다음 root bone 계약이 필요하다.

```text
root
├─ core_root
├─ shell_a_root
├─ shell_b_root
└─ shell_c_root
```

- 심지 vertex는 `core_root` 계열에만 weight한다.
- 각 외피의 vertex는 대응하는 `shell_*_root` 계열에만 weight한다.
- 외피 사이에 공유되거나 용접된 vertex island를 만들지 않는다.
- 외피가 제거된 뒤 내부에서 심지가 완전히 보이도록 모델링한다.
- Physics Asset은 외피 3개에 독립적인 body와 break 가능한 constraint를 제공한다.
- `core_root`는 외피가 분리돼도 마디 기준 포즈를 유지한다.
- Chaos 파괴는 시각 Skeletal Mesh 안에서만 수행한다. `BodyMesh_n`과 마디 사이의
  기존 Physics Constraint는 파괴 대상이 아니다.

최종 본 이름이 위 이름과 달라야 한다면 임포트 전에 이 문서와 구현 상수를 함께
갱신한다. 표시 이름만 비슷한 본을 런타임에 검색하는 fallback은 만들지 않는다.

### 5.3 머리와 장식 사지

Head에는 다음 체인이 개별 bone chain으로 존재해야 한다.

- 포개진 팔 6개
- 좌·우 촉수 날개 2개
- 상체와 머리카락을 위한 필요한 deform chain

정확한 본 이름은 최종 리그와 함께 별도 표로 채우되 다음 원칙을 지킨다.

- 팔과 날개마다 독립적인 chain root를 제공한다.
- spring 흔들림 대상과 실제 지면 IK 대상을 같은 chain으로 겹치지 않는다.
- 장식 사지는 Collision이나 gameplay trace의 기준으로 사용하지 않는다.
- 머리카락을 받치는 날개와 머리카락 사이 물리 충돌이 필요하면 Physics Asset에서
  전용 body를 사용하고 기존 몸통 충돌 채널을 사용하지 않는다.

### 5.4 지면형·끌림형 팔과 다리

무수한 사지 가운데 실제 bone animation을 수행하는 사지만 리그 체인을 만든다.
나머지 밀도 표현은 Niagara가 담당한다.

각 리그 체인은 Blueprint/Data Asset에서 다음 메타데이터로 분류할 수 있어야 한다.

- `Planted`: 기존 Leg의 Trace와 planted-foot 수학을 재사용하는 지면 고정 사지
- `Dragged`: 지면 Trace 결과를 따라가되 관성·지연을 적용하는 끌림 사지
- `Secondary`: AnimDynamics/RigidBody만 적용하는 장식 사지

기존 `ACMLegPart` Actor를 장식 사지 수만큼 생성하지 않는다. 기존 구현에서는
좌표 변환, 경사면 frame, 목표점 계산 같은 수학과 검증된 solver 순서만 재사용한다.

### 5.5 표면 샘플링 데이터

Head/Body/Tail SKM에는 최소 두 개의 Skeletal Mesh Sampling Region을 준비한다.

| Region | 용도 | 포함 범위 |
| --- | --- | --- |
| `IdleTentacleTop` | 마디 Idle 촉수 시작점 | 외피 상단, 안쪽/바닥/심지 제외 |
| `UnderbodyArms` | Niagara 하부 팔 방출 | 외피 하단과 측하단, 상단 제외 |

- Region은 LOD가 바뀌어도 의미가 유지되어야 한다.
- Idle 샘플은 최소 거리 필터를 거치므로 충분한 면적과 triangle 밀도가 필요하다.
- 래그돌로 분리되는 외피에서도 triangle과 bone weight가 유효해야 한다.
- Runtime CPU 샘플링이 필요한 경로는 임포트 검증 후 baked sample 데이터 사용 여부를
  결정한다. 모든 프레임에 전체 skinned vertex를 읽는 구현은 허용하지 않는다.

외부 타겟 Static/Skeletal Mesh의 휘감기는 전용 Region을 필수로 요구하지 않는다.
다만 CPU 접근이 불가능하거나 triangle 수가 과도한 애셋은 baked surface data 또는
단순 collision proxy를 지정해야 한다.

## 6. 애니메이션 계약

기본 AnimGraph 순서는 다음으로 고정한다.

```text
Base Pose
→ AnimDynamics/RigidBody secondary motion
→ Control Rig ground/drag IK
→ Output Pose
```

- IK Rig는 주로 retargeting에 사용한다.
- 런타임 지면 고정과 끌림 solver는 Control Rig가 담당한다.
- 동일 bone chain을 RigidBody와 Control Rig가 동시에 최종 제어하지 않는다.
- 지면 IK는 기존 `Leg_ControlRig_PlantedFoot_Implementation_Guide.md`의 좌표계,
  경사면 frame, final-solver 원칙을 재사용한다.
- 첫 수직 슬라이스에서는 Head의 Secondary 체인 1개, Planted 체인 1개,
  Dragged 체인 1개만 연결해 안정성을 검증한 뒤 개수를 확장한다.

## 7. VFX 시스템 경계

### 7.1 Idle 촉수와 휘감기 촉수

두 기능은 공용 표면 샘플러와 공용 spline pool을 사용한다. 구현 순서는 표면 샘플러,
Idle, 휘감기 순이다.

Idle 촉수의 한 생명주기는 다음과 같다.

```text
표면점 선택
→ 최소 거리 검사
→ 표면 Normal 방향으로 제어점 생성
→ 사인파형 흔들림
→ 시작점 방향으로 수축
→ pool 반환
```

휘감기는 촉수마디에 가까운 표면 후보부터 연결하며, 타겟의 현재 pose를 추종한다.
개수, 반경, 경로 굴곡, 교차 허용량, 활성 거리, 수축 시간은 Blueprint 프리셋으로
노출한다. 모든 값을 네트워크 복제 변수로 만들지는 않는다.

### 7.2 궤적

첫 구현은 이동 거리 기반의 겹치는 decal stamp pool을 사용한다.

- 시간 Tick이 아니라 누적 이동 거리를 기준으로 stamp한다.
- 수명과 fade를 적용한다.
- 정지, 순간이동, 비활성 상태에서 생성을 중단한다.
- `CMGore`의 stroke 개념은 참고할 수 있지만 키메라 이동 궤적은 별도 cosmetic
  component가 소유한다.
- Render Target 페인트는 decal 방식이 품질 또는 성능 목표를 충족하지 못한 경우에만
  별도 페이즈로 검토한다.

### 7.3 하부 팔 Niagara

- `UnderbodyArms` Sampling Region에서 방출한다.
- 몸통 속도와 이동 여부로 spawn rate와 reach를 제어한다.
- 개별 팔을 replicated Actor 또는 SkeletalMeshComponent로 생성하지 않는다.
- 마디 비활성 또는 사망 시 방출을 중단하고 기존 particle을 정리한다.
- Niagara Scalability와 거리별 예산을 필수로 제공한다.

## 8. Phase 2 몸통 컴포지션 구현 결과

최종 메시나 VFX 없이 placeholder 상태로 다음 몸통 컴포지션을 구현했다.

1. `ACMChimeraBodySegmentActor` C++ 기반과 Blueprint 이벤트 확장 지점을 추가했다.
2. 최대 16개 `SegmentPresentation_n`을 기존 `BodyMesh_n`에 1:1로 구성했다.
3. `/Game/Chimera/Character/Chimera/Blueprint/BP_CMChimeraBodySegment`를 생성했다.
4. 각 presentation에 `BodyVisual`과 `TentacleActor` ChildActorComponent를 구성했다.
5. 서버 시작 시 각 TentacleActor를 기존 `InitializeForSegment()` 계약으로 초기화한다.
6. 활성 수 변경은 Spawn/Destroy 대신 Tick, Collision, Visibility 활성 상태를 전환한다.
7. Head/Body/Tail 역할 계산을 순수 함수로 추가하고 자동화 테스트를 작성했다.
8. `RefreshTentacleSegments()`의 직접 `SpawnActor/Destroy` 코드를 제거하고 고정
   presentation 동기화 경로로 교체했다.

Phase 2에서 하지 않은 작업은 다음과 같다.

- 최종 Skeletal Mesh 임포트
- Death 래그돌
- Control Rig와 AnimBP 제작
- 표면 샘플링 및 spline 촉수
- 궤적 decal과 Niagara 하부 팔
- 기존 몸통 질량, 크기, constraint 튜닝

### 8.1 Phase 3 메시 정의 및 검증 결과

최종 원본 메시가 아직 없으므로 가짜 Skeletal Mesh나 추측 Object Path는 만들지
않았다. 대신 실제 애셋 투입 시 런타임 연결과 검수를 한 번에 수행할 수 있도록
다음을 구현했다.

1. `UCMChimeraVisualDefinition` Data Asset 타입을 추가했다.
2. Head/Body/Tail 각각의 Skeletal Mesh, 선택적 Anim Class, 상대 Transform을
   하나의 역할별 프리셋으로 정의했다.
3. `BP_CMChimeraBodySegment`의 안정적인 `BodyVisual`이 역할 변경 시 대응
   프리셋의 Mesh와 Anim Class를 적용하도록 연결했다.
4. Content Browser의 `Validate Assets`에서 다음 항목을 자동 검사한다.
   - 세 역할의 Mesh 할당 여부와 공용 Skeleton 일치
   - 전체 reference skeleton의 본 순서·부모 hierarchy·bind pose 일치
   - `root`, `core_root`, `shell_a_root`, `shell_b_root`, `shell_c_root`
   - `IdleTentacleTop`, `UnderbodyArms` Sampling Region과 유효한 LOD
   - core/외피 3개의 Physics Body
   - 각 외피 본에 연결된 linear 또는 angular breakable constraint
   - 역할 프리셋 Scale `(1, 1, 1)` 유지
5. 정의 애셋이 비어 있을 때 역할별 누락 오류 3개를 반환하는 자동화 테스트를
   추가했다.

빈 정의 애셋은 의도적으로 생성하지 않았다. 원본 메시가 없는 상태에서 프로젝트
전체 Data Validation을 항상 실패시키는 불완전 애셋을 남기지 않기 위함이다.

원본 전달 후 에디터 작업 순서는 다음으로 고정한다.

1. Head를 `/Game/Chimera/Character/Chimera/Mesh`에 임포트해 공용 Skeleton을
   생성한다.
2. Body와 Tail은 반드시 같은 Skeleton을 선택해 임포트한다.
3. Physics Asset에 core와 외피 3개 body 및 외피별 breakable constraint를 만든다.
4. 각 SKM에 두 Sampling Region을 생성하고 올바른 LOD와 필터를 지정한다.
5. `CMChimeraVisualDefinition` 타입의 Data Asset을 생성해 세 역할을 연결한다.
6. `BP_CMChimeraBodySegment`의 `Visual Definition`에 해당 애셋을 지정한다.
7. Data Asset에서 `Asset Actions > Validate Assets`를 실행하고 오류가 0인지
   확인한 뒤 1인/2인 PIE에서 Head-Tail 및 Head-Body-Body-Tail을 확인한다.

### 8.2 Phase 4 표면 샘플러 및 Idle 촉수 결과

`UCMChimeraIdleTentacleComponent`를 모든 몸통 presentation의 `BodyVisual` 아래에
고정 서브오브젝트로 추가했다. 이 컴포넌트는 로컬 cosmetic이며 전용 서버에서는
활성화되지 않는다.

구현된 동작은 다음과 같다.

1. 활성화 또는 Head/Body/Tail 역할 변경 시 `IdleTentacleTop` Sampling Region의
   built triangle 목록을 읽는다.
2. 해당 시점의 CPU skinned vertex를 한 번만 계산해 triangle 내부 barycentric
   표면 후보와 보간 normal을 만든 뒤 원본 vertex 배열을 보관하지 않는다.
3. 활성 촉수 anchor 사이에 `MinimumSampleDistance`를 적용한다.
4. 각 촉수는 `SplinePointCount`개의 점과 `SplinePointCount - 1`개의
   `USplineMeshComponent`로 구성한다.
5. normal 방향 기본 곡선에 직교축 사인파를 더해 꿈틀거리고, `IdleDuration` 뒤
   `RetractionDuration` 동안 모든 점이 anchor로 수축한다.
6. 촉수와 spline mesh는 매 주기 생성·파괴하지 않고 `TentacleCount` 크기의 풀에서
   재사용한다.
7. 비활성 마디에서는 Tick과 렌더링을 끄며, 같은 활성 상태가 다시 전달돼도
   수명주기를 재시작하지 않는다.
8. 기본 렌더링은 기존 촉수 표현과 동일한 `SM_VFX_Arm_03` 및
   `MI_VFX_Goo_Arm_01`을 재사용하고 Blueprint에서 교체할 수 있다.

설정 가능한 핵심 값은 `TentacleCount`, `MinimumSampleDistance`,
`SplinePointCount`, `TentacleLength`, 사인파 진폭·주기·속도, 대기·수축·재생성
시간과 Mesh/Material이다.

현재 anchor는 표면을 샘플링한 시점의 local pose를 사용한다. 이후에는
`BodyVisual`의 component transform을 따르지만 매 프레임 skinned vertex 전체를
다시 읽지 않는다. 최종 메시의 body deformation이 큰 경우에만 실제 리그 검증 후
triangle-to-bone 바인딩 또는 baked surface data를 추가한다.

최종 Head/Body/Tail 메시가 아직 없으므로 다음 항목은 애셋 입수 후 PIE에서
확인해야 한다.

- `IdleTentacleTop`이 실제 외피 상단만 포함하는지
- 최소 거리 55 cm에서 기본 4개 anchor가 안정적으로 선택되는지
- 마디별 길이·폭과 기존 Goo Arm Mesh의 연결부 스케일
- 역할 전환 및 사망 pose에서 anchor가 외피 밖에 유지되는지

### 8.3 Phase 5 키메라 이동 궤적 결과

`UCMChimeraTrailComponent`를 `ACMChimera`의 고정 서브오브젝트로 추가했다.
컴포넌트는 복제하지 않는 로컬 cosmetic이며 전용 서버에서는 Tick하지 않는다.

구현된 동작은 다음과 같다.

1. Tick 시간이 아니라 각 활성 `BodyMesh_n` 촉수마디의 누적 평면 이동 거리를
   독립적으로 추적해 스탬프한다. 활성 마디가 8개이면 8개 위치에서 동시에 칠한다.
2. 각 마디의 프레임 사이 이동 구간을 보간해 `StampSpacing` 간격으로 데칼을 배치한다.
3. 지면 line trace의 충돌점과 normal에 데칼을 정렬하고 이동 방향을 접선으로 사용한다.
4. `PoolCapacity` 크기의 `UDecalComponent`를 한 번 만들고 수명 만료 또는 순환 시
   재사용한다.
5. 기본 수명은 60초이며 마지막 20초 동안 스탬프별 동적 머티리얼의 `Opacity`를
   1에서 0까지 선형으로 낮춰 흔적이 서서히 말라 사라지는 인상을 만든다.
6. 순간이동은 `TeleportDistance`로 판별해 중간 경로를 칠하지 않으며, 비활성·숨김
   상태에서는 누적 이동을 초기화하고 새 스탬프를 만들지 않는다.
7. 한 프레임의 생성 수를 `MaxStampsPerFrame`으로 제한하고 제한 시 최신 경로를
   우선한다.
8. 바닥 궤적은 CMGore/Blood Definition 재료와 분리된 전용
   `M_CMChimeraTrailDecal`을 사용한다. 이 재료는 `BrushTexture`의 R 채널을 데칼
   opacity에 직접 곱하므로 `/Game/TPBDMat/Textures/T_splat0_wall_v2`의 모양을
   변경하지 않는다. 표면 normal을 로컬 X 투영축, 이동 tangent를 로컬 Z축으로
   사용하며 정사각형 UV 비율을 유지한다.
9. 기본 데칼 폭과 길이는 각각 37.5cm이며,
   `TrailMaterial`, `BrushTexture`, 폭, 길이, 간격, 수명, trace 범위를 Blueprint에서
   교체하거나 조정할 수 있다.
10. 8개 마디의 60초 흔적을 가능한 한 유지하도록 기본 풀은 8192개이며, 급격한
    생성 폭증은 프레임당 최대 64개로 제한한다.

자동화 테스트는 거리 누적·프레임 제한·순간이동 판정, 비복제 소유권과 실제 transient
게임 월드에서 100cm 이동 시 두 개의 데칼 스탬프가 활성화되는 경로를 검증한다.
또한 60초/20초 수명 설정, 정사각형 브러시 크기, 8192개 풀, 표면 normal/tangent
투영축, 전용 머티리얼과 요청 브러시, 초기 `Opacity=1`과 페이드 중간 시점의
`Opacity=0.5` 적용을 검증한다.

## 9. 기준선 및 완료 검증

### 9.1 자동화 테스트

리팩토링 전후에 최소한 다음 기존 테스트가 통과해야 한다.

```text
Chimera.Tentacle
Chimera.Multiplayer.ControlAssignments
Chimera.Animation.Part
```

몸통 컴포지션 페이즈에서 추가한 테스트는 다음을 검증한다.

- 활성 마디가 2일 때 역할은 정확히 Head, Tail이다.
- 활성 마디가 4일 때 역할은 Head, Body, Body, Tail이다.
- 활성 마디가 16일 때 Head 1, Body 14, Tail 1이다.
- 최대 마디마다 presentation과 TentacleActor가 정확히 하나 존재한다.
- 활성 수 감소 후 Actor 수는 변하지 않고 비활성 Tick/Collision만 꺼진다.
- 다시 증가하면 같은 인스턴스가 같은 `SegmentIndex`로 재활성화된다.
- 활성 마디마다 TentacleActor가 서로 다른 `BodyMesh_n`에 매핑된다.
- 기존 파츠 예약과 당기기 테스트 결과가 변하지 않는다.

### 9.2 PIE 검증 행렬

| 시나리오 | 기대 결과 |
| --- | --- |
| 1인 | 2마디, Head-Tail, 총 4슬롯 |
| 2인 | 4마디, Head-Body-Body-Tail, 총 8슬롯 |
| 8인 | 16마디, Head-Body×14-Tail, 총 32슬롯 |
| 2→1인 변경 | 이전 Tail 후보가 비활성화되고 1번 마디가 Tail로 변경 |
| 1→2인 변경 | 1번 마디가 Body로 변경되고 3번 마디가 Tail로 활성화 |
| 마디 사망 | 해당 두 슬롯 분리·잠금, Tentacle 당기기 중단, 시각 사망 이벤트 1회 |
| Late Join | 활성 수, 역할, 마디 사망, Tentacle 상태가 서버와 일치 |
| 체크포인트 복구 | 기존 건강한 마디 복구 규칙과 presentation 상태가 일치 |

### 9.3 성능 기준선

다음 페이즈에서는 VFX 성능을 최적화하지 않지만 이후 비교를 위해 다음 값을 기록한다.

- 16마디 상태의 Game Thread와 GPU frame time
- 활성 및 비활성 presentation의 Tick 수
- TentacleActor 수와 활성 DetectionSphere 수
- Skeletal Mesh component 수
- 네트워크 Actor 수와 초당 전송량

### 9.4 2026-09-06 기능 기준선 결과

UE 5.7 `UnrealEditor-Cmd`의 `-NullRHI` 환경에서 다음 결과를 확인했다.

| 필터 | 결과 |
| --- | --- |
| `Chimera.BodySegment` | 3/3 성공 |
| `Chimera.Tentacle` | 3/3 성공 |
| `Chimera.Multiplayer.ControlAssignments` | 2/2 성공 |
| `Chimera.Animation.Part` | 2/2 성공 |

각 실행은 성공으로 종료됐고 `ChimeraEditor Win64 Development` 빌드도 성공했다.
`Chimera.BodySegment.VisualDefinitionContract`는 필수 이름, 역할별 프리셋 선택,
빈 정의의 정밀 오류 보고를 검증한다. `IdleTentacleMath`는 대기·수축 alpha,
완전 수축 위치, 최소 anchor 간격, 모든 마디의 고정 Idle 컴포넌트 소유를 검증한다.
실제 메시가 들어오면 정의 애셋 자체의 Content Validation과 PIE 외형 결과를 이
기준선에 추가한다.

## 10. 미확정 항목과 진입 조건

최종 모델링 애셋이 아직 없으므로 다음 값은 임포트 시 반드시 채워야 한다.

- Head/Body/Tail의 실제 Object Path
- 공용 Skeleton Object Path
- 전체 본 목록과 Head 6개 팔·2개 날개 chain 표
- Planted/Dragged/Secondary 사지 chain 표
- Physics Asset Object Path와 외피 constraint 이름
- `IdleTentacleTop`, `UnderbodyArms` Sampling Region 존재 여부
- LOD별 triangle, bone, material slot 수

몸통 컴포지션 리팩토링은 placeholder로 먼저 진행할 수 있다. Death, 실제 AnimBP,
표면 샘플링 구현은 위 애셋 계약이 검증되기 전에는 최종 구현으로 간주하지 않는다.

## 11. 범위 밖 의존성

현재 최대 마디와 슬롯 주소 체계는 8인을 지원한다. 다만 플레이어 수별 시작 파츠
배치 데이터가 모든 인원 구성을 지원하는지는 별도 게임플레이 작업이다. 이 문서의
Head/Body/Tail 표시가 5~8인에서 정상이어도 시작 파츠 구성까지 완료됐음을 의미하지
않는다.
