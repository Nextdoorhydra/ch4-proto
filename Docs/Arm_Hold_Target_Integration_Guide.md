# 디폴트 암 홀드 대상 연동 가이드

> 2026-08-31 레버 구현 갱신: 아래 레버 연동 예시는 구현 전 설계 기록이다. 현재 `ACMLeverBase`에는 `ICMArmHoldTarget` 연동, 그랩 Pull 결과 구분, 회전 보간이 구현되어 있다. 레버의 실제 설정은 [장애물·버튼 가이드](Chimera_Obstacle_Mechanism_Architecture.md), 최신 이벤트와 UI 계약은 [버튼 UI 가이드](Chimera_Button_UI_Integration.md)를 우선한다. 아래 예시를 현재 클래스에 중복 추가하지 않는다.

## 목적과 담당 범위

디폴트 암은 홀드 입력 시 전방에서 `ICMArmHoldTarget` 대상을 먼저 찾는다.
대상이 홀드를 승인하면 해당 대상을 잡고, 대상이 없거나 모두 거절하면 기존처럼
지면을 짚는다.

- 팔 담당: 대상 탐색, 우선순위 선택, 손 IK 목표, 홀드 시작/종료, 지면 fallback,
  상호작용 후 릴리즈 공격 억제
- 대상 담당: 잡을 위치와 우선순위 제공, 잡힌 동안의 자체 동작, 놓을 때 정리
- 모든 판정과 상태 변경은 서버 권위로 처리한다.

대상 코드는 `Source/Chimera/Parts/Arm/CMArmHoldTarget.h`의 계약만 구현하면 된다.

### 현재 구현 상태

- 구현 완료: 팔 탐색/우선순위, 지면 fallback, 손 IK, Physics Handle,
  해제 및 공격 억제
- 구현 완료: `ACMArmHoldableBox`, `BP_ArmHoldableBox` 물리 이동 예제
- `ACMLeverBase`: 현재 연동 완료. 상단의 최신 레버 문서를 참고한다.
- `ACMPowerCableActor`: 아래 전선 섹션은 연동 설계 예시이며, 이 문서의 레버 갱신에서는 전선 구현 상태를 재검증하지 않았다.

신규 대상은 아래 인터페이스 연동 구조를 참고하되 기존 대상은 현재 코드와 대조한다.

## 현재 팔 구현 구조

연동 전에 대상 담당자가 알아야 할 팔 쪽 흐름은 다음과 같다.

```text
홀드 입력
  -> UCMLineBodyMovementCoordinator::TryBeginArmAnchor()
  -> 팔 앞 Sphere Sweep
  -> ICMArmHoldTarget 후보의 QueryArmHold()
  -> Priority 내림차순, 거리 오름차순 선택
  -> BeginArmHold()
  -> bUsePhysicsHandle에 따라 대상 이동 방식 결정
  -> ACMArmPart::BeginInteractableHold()
```

- 대상이 하나도 승인하지 않으면 기존 지면 Constraint로 fallback한다.
- `bUsePhysicsHandle == true`면 팔 시스템이 `UPhysicsHandleComponent`를 생성하고
  잡은 지점을 현재 몸통 마디의 로컬 좌표로 저장한다. 서버 물리
  갱신마다 이 좌표를 월드 목표점으로 변환하여 물체만 손을 따라오게 한다.
- Physics Handle은 `GrabComponentAtLocation()`을 사용하므로 위치만 추적하고
  물체 회전은 잡은 지점을 중심으로 자유롭다.
- `bUsePhysicsHandle == false`면 팔은 손 IK와 홀드 수명만 관리하고,
  전선·버튼·논리 레버 같은 대상이 자신의 동작을 처리한다.
- 해제 시 `EndArmHold()`와 팔 홀드 상태를 정리하고 Physics Handle/지면
  Constraint를 파괴하며, 이 입력을 디폴트 암 공격으로 연결하지 않는다.

관련 파일:

- `Source/Chimera/Movement/CMLineBodyMovementCoordinator.cpp`: 탐색, 선택,
  Physics Handle, 지면 fallback, 해제
- `Source/Chimera/Parts/Arm/CMArmPart.cpp`: 복제 홀드 상태와 움직이는
  `TargetComponent` 기준 손 IK 좌표
- `Source/Chimera/Animation/CMPartAnimInstance.cpp`: 기존 손 IK 경로에 지면/상호작
  홀드 상태 전달

## 대상 유형별 선택

| 대상 | `TargetComponent` | `bUsePhysicsHandle` | 동작 담당 |
| --- | --- | --- | --- |
| 상자, 떨어진 파츠 등 물리 물체 | 물리 시뮬레이션 컴포넌트 | `true` | 팔 Physics Handle |
| 현재 비물리 전선 끝 | 끝점 Hold Volume | `false` | 전선 액터의 서버 이동 |
| 논리 레버·버튼 | 손잡이 Hold Volume | `false` | 대상의 이동량/상태 로직 |
| 물리 Hinge 레버 | 물리 레버 Mesh | `true` | 레버 Constraint + 팔 Physics Handle |

`bUsePhysicsHandle=true`인 `TargetComponent`는 반드시 유효한 Physics Body가 있고
`IsSimulatingPhysics()`가 `true`여야 한다. Hold Volume은 Sweep 탐색용으로
사용할 수 있지만 Physics Handle의 `TargetComponent`로는 실제 물리 Mesh를
반환해야 한다.

## 공통 인터페이스

```cpp
bool QueryArmHold(ACMArmPart* ArmPart, FCMArmHoldSpec& OutSpec) const;
bool BeginArmHold(ACMArmPart* ArmPart);
void EndArmHold(ACMArmPart* ArmPart);
```

### QueryArmHold

팔이 대상을 후보로 평가할 때 서버에서 호출한다.

```cpp
OutSpec.Priority = 100;
OutSpec.HoldLocation = 잡을_월드_위치;
OutSpec.HoldNormal = 손바닥이_바라볼_월드_노멀;
OutSpec.TargetComponent = 잡을_컴포넌트;
OutSpec.bUsePhysicsHandle = false;
return true;
```

- `Priority`: 높을수록 먼저 선택된다. 같은 값이면 팔과 가까운 대상이 선택된다.
- `HoldLocation`: 손 IK가 사용할 월드 위치다.
- `HoldNormal`: 손 방향 표현에 사용할 월드 노멀이다.
- `TargetComponent`: 움직이는 대상이면 반드시 지정한다. 팔은 이 컴포넌트 기준
  로컬 위치를 저장하므로 대상이 움직여도 손 목표가 따라간다.
- `bUsePhysicsHandle`: 상자처럼 실제 물리 대상을 손 목표점으로
  이동시켜야 할 때만 `true`로 설정한다. 키메라 몸통과 대상을
  직접 Constraint로 묶지 않는다.
- 자체 이동 로직이 있는 전선과, 손 이동량을 읽는 레버는 `false`를
  사용한다.

`QueryArmHold()`가 `false`를 반환하거나 `BeginArmHold()`가 실패하면 팔은 다음
후보를 검사한다. 승인되는 후보가 없으면 지면을 짚는다.

### BeginArmHold

최종 후보로 선택된 뒤 서버에서 한 번 호출된다.

- 이미 다른 팔이 잡고 있으면 `false`
- 연결 완료, 잠김 등 현재 잡을 수 없는 상태면 `false`
- 자체 홀드 상태를 정상적으로 시작했으면 `true`
- 이 함수에서 대상별 Grabber를 기록한다.

### EndArmHold

입력 해제, 팔 파괴, 스태미나 고갈 등 대상이 살아 있는 상태에서 홀드가 끝날 때
서버에서 호출된다. 타이머, Tick, Grabber 및 임시 상태를 여기서 정리한다.
대상 액터가 먼저 파괴되는 경우에는 대상 자신의 `EndPlay()`에서도 같은 임시 상태를
정리해야 한다.

상호작용 홀드가 성립했던 입력은 놓을 때 디폴트 암 공격으로 이어지지 않는다.

## 탐색을 위한 충돌 조건

팔은 홀드 시작 순간 한 번 Sphere Sweep을 한다. 대상 액터가 인터페이스를 구현해도
Sweep에 잡히는 `UPrimitiveComponent`가 없으면 후보가 될 수 없다.

- `Collision Enabled`: `Query Only` 또는 `Query and Physics`
- Object Type: 일반적으로 `WorldDynamic`
- 팔 홀드 Sweep과 겹칠 수 있는 Collision Response
- 액터 자체가 `ICMArmHoldTarget` 구현
- 디폴트 팔의 기본 탐색값: `HoldRange = 120`, `HoldRadius = 40`

## 물리 이동 물체 담당 구현

상자·떨어진 파츠처럼 실제 물리 물체를 옮길 때는 대상을 팔과
Constraint로 직접 묶지 않고 Physics Handle을 요청한다. 현재 검증 예제는
`ACMArmHoldableBox`와 `BP_ArmHoldableBox`이다.

### 1. 물리 컴포넌트

```cpp
PhysicsMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
PhysicsMesh->SetSimulatePhysics(true);
PhysicsMesh->SetEnableGravity(true);
SetReplicateMovement(true);
PhysicsMesh->SetIsReplicated(true);
```

질량과 Damping은 대상 액터의 게임플레이 값으로 관리한다. 팔 시스템은
잡힌 물체의 질량을 바꾸거나 양손 보너스 힘을 추가하지 않는다.

### 2. QueryArmHold

```cpp
const FVector ArmLocation = ArmPart->GetPartMesh()
    ? ArmPart->GetPartMesh()->GetComponentLocation()
    : ArmPart->GetActorLocation();
FVector HoldLocation = PhysicsMesh->Bounds.Origin;
PhysicsMesh->GetClosestPointOnCollision(ArmLocation, HoldLocation);

OutSpec.Priority = 100;
OutSpec.HoldLocation = HoldLocation;
OutSpec.HoldNormal = (ArmLocation - HoldLocation)
    .GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
OutSpec.TargetComponent = PhysicsMesh;
OutSpec.bUsePhysicsHandle = true;
return PhysicsMesh->IsSimulatingPhysics() && !HoldingArm.IsValid();
```

`HoldLocation`은 팔과 가까운 충돌 표면을 사용하면 중심을 잡는 것보다
손 표현이 자연스럽다. Physics Handle은 이 지점을 옮기며 물체 회전은
제한하지 않는다.

### 3. BeginArmHold / EndArmHold

대상은 누가 잡고 있는지만 소유한다. Physics Handle의 생성·갱신·해제는
팔 시스템이 담당한다.

```cpp
bool AMyHoldableActor::BeginArmHold_Implementation(ACMArmPart* ArmPart)
{
    if (!HasAuthority() || !IsValid(ArmPart) || HoldingArm.IsValid())
    {
        return false;
    }

    HoldingArm = ArmPart;
    ForceNetUpdate();
    return true;
}

void AMyHoldableActor::EndArmHold_Implementation(ACMArmPart* ArmPart)
{
    if (HasAuthority() && HoldingArm.Get() == ArmPart)
    {
        HoldingArm.Reset();
        ForceNetUpdate();
    }
}
```

### 물리 물체 완료 조건

- 잡기 전에 물리 시뮬레이션이 활성화되어 있다.
- 잡으면 물체가 손 목표점을 따라오고 키메라는 물체에 고정되지 않는다.
- 물체가 벽에 걸려도 키메라 몸통에 하드 Constraint 반력이 전달되지 않는다.
- 놓으면 Physics Handle이 제거되고 물체는 기존 물리 상태를 유지한다.
- 팔 공격이 발생하지 않고 서버/클라이언트에서 물체 위치가 일치한다.

## 전선 담당 구현

현재 `ACMPowerCableActor`는 물리 케이블이 아니다. `CableStartLocation`은 고정
시작점이고, 전선 액터의 위치가 자유 끝점이며, 그 사이를 SplineMesh로 표현한다.
따라서 물리 Constraint를 추가하지 않고 기존 액터 끝점을 팔을 따라 이동시킨다.

### 1. 인터페이스와 끝점 충돌 추가

`ACMPowerCableActor`가 `ICMArmHoldTarget`을 구현하게 한다.

```cpp
class CHIMERA_API ACMPowerCableActor
    : public AActor
    , public ICMArmHoldTarget
```

탐색용 Sphere를 추가한다. 기존 `CableSpline` 루트는 유지하고 Sphere를 자식으로
붙이면 기존 블루프린트 계층 변경을 줄일 수 있다.

```cpp
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Power")
TObjectPtr<USphereComponent> CableEndHoldVolume;

UPROPERTY(Transient)
TWeakObjectPtr<ACMArmPart> HoldingArm;

FVector GrabOffsetFromArm = FVector::ZeroVector;
```

생성자 설정 예시:

```cpp
CableEndHoldVolume = CreateDefaultSubobject<USphereComponent>(
    TEXT("CableEndHoldVolume"));
CableEndHoldVolume->SetupAttachment(CableSpline);
CableEndHoldVolume->SetSphereRadius(30.0f);
CableEndHoldVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
CableEndHoldVolume->SetCollisionObjectType(ECC_WorldDynamic);
CableEndHoldVolume->SetGenerateOverlapEvents(true);
```

### 2. QueryArmHold

```cpp
bool ACMPowerCableActor::QueryArmHold_Implementation(
    ACMArmPart* ArmPart,
    FCMArmHoldSpec& OutSpec) const
{
    if (!IsValid(ArmPart) || IsConnected() || IsGrabbed()
        || !CableEndHoldVolume)
    {
        return false;
    }

    const FVector ArmLocation = ArmPart->GetPartMesh()
        ? ArmPart->GetPartMesh()->GetComponentLocation()
        : ArmPart->GetActorLocation();

    OutSpec.Priority = 100;
    OutSpec.HoldLocation = GetCableEndLocation();
    OutSpec.HoldNormal = (ArmLocation - OutSpec.HoldLocation)
        .GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
    OutSpec.TargetComponent = CableEndHoldVolume;
    OutSpec.bUsePhysicsHandle = false;
    return true;
}
```

### 3. BeginArmHold / EndArmHold

기존 `BeginGrab()`과 `ReleaseGrab()`을 재사용한다.

```cpp
bool ACMPowerCableActor::BeginArmHold_Implementation(ACMArmPart* ArmPart)
{
    if (!BeginGrab(ArmPart))
    {
        return false;
    }

    HoldingArm = ArmPart;
    const FVector ArmLocation = ArmPart->GetPartMesh()
        ? ArmPart->GetPartMesh()->GetComponentLocation()
        : ArmPart->GetActorLocation();
    GrabOffsetFromArm = GetActorLocation() - ArmLocation;
    return true;
}

void ACMPowerCableActor::EndArmHold_Implementation(ACMArmPart* ArmPart)
{
    if (HoldingArm.Get() == ArmPart)
    {
        HoldingArm.Reset();
        ReleaseGrab();
    }
}
```

### 4. 서버 Tick에서 끝점 이동

기존 `Tick()`의 시각 갱신 전에 서버 위치를 갱신한다.

```cpp
if (HasAuthority() && HoldingArm.IsValid()
    && Grabber == HoldingArm.Get() && !IsConnected())
{
    ACMArmPart* ArmPart = HoldingArm.Get();
    const FVector ArmLocation = ArmPart->GetPartMesh()
        ? ArmPart->GetPartMesh()->GetComponentLocation()
        : ArmPart->GetActorLocation();
    SetActorLocation(ArmLocation + GrabOffsetFromArm);
}
```

`SetReplicateMovement(true)`가 이미 설정되어 있으므로 서버에서 이동시킨 액터 위치는
클라이언트에 전달된다.

전선 길이 제한이 필요하면 다음 속성을 추가하고 `CableStartLocation` 기준으로
DesiredLocation을 Clamp한다. 스펙에 길이 제한이 없다면 최초 버전에서는 생략한다.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Power",
    meta = (ClampMin = "0.0"))
float MaximumCableLength = 0.0f; // 0은 무제한
```

### 5. 소켓 접속

기존 `TryConnectToSocket()`과 `UCMPowerSocketComponent::TryConnectCable()`을 유지한다.
접속 성공 시 `SetConnectedSocket()`이 Grabber를 비우고 액터를 소켓 위치로 옮긴다.

`BP_PowerCable`의 `ActorBeginOverlap`에서 소켓 접속을 처리하고 있다면 새
`CableEndHoldVolume`의 Overlap이 기존 그래프에 들어가는지 확인한다. 접속 그래프가
다른 충돌 컴포넌트를 전제로 한다면 그 컴포넌트는 유지한다.

### 전선 완료 조건

- 빈 전선 끝을 잡으면 지면 대신 전선 홀드가 시작된다.
- 다른 팔이 이미 잡았거나 소켓에 연결된 전선은 잡히지 않는다.
- 키메라가 이동하면 전선 끝이 최초 Grab Offset을 유지하며 따라온다.
- 놓으면 전선은 마지막 위치에 남고 팔 공격이 발생하지 않는다.
- 일치하는 채널의 소켓 반경 안에서 기존 접속이 성공한다.
- 리슨 서버와 원격 클라이언트에서 전선 끝 및 접속 상태가 동일하다.

## 레버 담당 구현

목표는 물리 Hinge가 아니라 잡은 팔의 앞뒤 이동을 레버 축에 투영하여 양방향
레버를 구동하는 것이다. 키메라 전체가 Chaos 물리이므로 몸통과 레버를 하드
Constraint로 연결하지 않는다.

이 섹션은 현재 `ACMLeverBase`를 물리 액터로 바꾸지 않고 적용할 수 있는
안정적인 논리 구동형이다. 실제 회전 물리가 필요하면 아래의
`Physics Handle + Hinge 레버` 대안을 선택한다. 두 방식을 동시에 적용하지 않는다.

### 1. 인터페이스와 홀드 볼륨 추가

`ACMLeverBase`가 기존 `ICMGrabPullTarget`과 함께 `ICMArmHoldTarget`을 구현하게 한다.

```cpp
class CHIMERA_API ACMLeverBase
    : public ACMStageButtonBase
    , public ICMGrabPullTarget
    , public ICMArmHoldTarget
```

레버 손잡이 위치에 Query 전용 Sphere를 둔다. 블루프린트의 손잡이 메시가 확실한
Collision을 제공한다면 그 컴포넌트를 대신 사용할 수 있지만, 명시적 Hold Volume이
에셋 설정에 덜 민감하다.

```cpp
UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
    Category = "Chimera|Mechanism|Lever")
TObjectPtr<USphereComponent> ArmHoldVolume;

UPROPERTY(Transient)
TWeakObjectPtr<ACMArmPart> HoldingArm;
```

### 2. 양방향 레버 상태

```cpp
UENUM(BlueprintType)
enum class ECMLeverPosition : uint8
{
    Negative,
    Positive
};

UPROPERTY(ReplicatedUsing = OnRep_LeverAlpha, VisibleInstanceOnly,
    BlueprintReadOnly, Category = "Chimera|Mechanism|Lever")
float LeverAlpha = -1.0f;

UPROPERTY(EditAnywhere, BlueprintReadOnly,
    Category = "Chimera|Mechanism|Lever", meta = (ClampMin = "1.0"))
float FullTravelDistance = 100.0f;

UPROPERTY(EditAnywhere, BlueprintReadOnly,
    Category = "Chimera|Mechanism|Lever",
    meta = (ClampMin = "0.0", ClampMax = "1.0"))
float SwitchThreshold = 0.8f;

ECMLeverPosition LeverPosition = ECMLeverPosition::Negative;
FVector GrabStartArmLocation = FVector::ZeroVector;
float GrabStartAlpha = -1.0f;
```

`LocalPullAxis`는 기존 속성을 그대로 사용한다. 월드 양의 축 방향으로 몸이 이동하면
Positive, 반대 방향으로 이동하면 Negative가 된다.

### 3. QueryArmHold

```cpp
OutSpec.Priority = 100;
OutSpec.HoldLocation = ArmHoldVolume->GetComponentLocation();
OutSpec.HoldNormal = -GetActorTransform()
    .TransformVectorNoScale(LocalPullAxis).GetSafeNormal();
OutSpec.TargetComponent = ArmHoldVolume;
OutSpec.bUsePhysicsHandle = false;
return HoldingArm.IsValid() == false;
```

### 4. BeginArmHold

잡는 순간 `PressButton()`을 호출하지 않는다.

```cpp
bool ACMLeverBase::BeginArmHold_Implementation(ACMArmPart* ArmPart)
{
    if (!HasAuthority() || !IsValid(ArmPart) || HoldingArm.IsValid())
    {
        return false;
    }

    HoldingArm = ArmPart;
    GrabStartArmLocation = ArmPart->GetPartMesh()
        ? ArmPart->GetPartMesh()->GetComponentLocation()
        : ArmPart->GetActorLocation();
    GrabStartAlpha = LeverPosition == ECMLeverPosition::Positive
        ? 1.0f : -1.0f;
    SetActorTickEnabled(true);
    return true;
}
```

### 5. 홀드 중 앞뒤 이동 판정

서버 Tick에서 팔의 이동량을 `LocalPullAxis`에 투영한다.

```cpp
const FVector CurrentArmLocation = HoldingArm->GetPartMesh()
    ? HoldingArm->GetPartMesh()->GetComponentLocation()
    : HoldingArm->GetActorLocation();
const FVector WorldPullAxis = GetActorTransform()
    .TransformVectorNoScale(LocalPullAxis).GetSafeNormal();
const float SignedDistance = FVector::DotProduct(
    CurrentArmLocation - GrabStartArmLocation,
    WorldPullAxis);

LeverAlpha = FMath::Clamp(
    GrabStartAlpha + 2.0f * SignedDistance / FullTravelDistance,
    -1.0f,
    1.0f);
```

임계값을 넘을 때만 상태를 변경한다.

```cpp
if (LeverAlpha >= SwitchThreshold
    && LeverPosition != ECMLeverPosition::Positive)
{
    LeverPosition = ECMLeverPosition::Positive;
    PressButton(HoldingArm.Get());
}
else if (LeverAlpha <= -SwitchThreshold
    && LeverPosition != ECMLeverPosition::Negative)
{
    LeverPosition = ECMLeverPosition::Negative;
    ReleaseButton(HoldingArm.Get());
}
```

상태 비교가 같은 임계점의 반복 호출을 막는다. 한 번 잡은 상태에서 양쪽 임계점을
오가면 레버를 다시 반대 방향으로 전환할 수 있다.

`LeverAlpha`가 바뀌면 서버와 `OnRep_LeverAlpha()`에서 동일한 BP 이벤트를 호출한다.

```cpp
UFUNCTION(BlueprintImplementableEvent,
    Category = "Chimera|Mechanism|Lever")
void OnLeverAlphaChanged(float NewAlpha);
```

블루프린트는 `-1 ~ +1` 값을 손잡이의 양 끝 회전각으로 변환한다.

### 6. EndArmHold

```cpp
void ACMLeverBase::EndArmHold_Implementation(ACMArmPart* ArmPart)
{
    if (HoldingArm.Get() != ArmPart)
    {
        return;
    }

    HoldingArm.Reset();
    LeverAlpha = LeverPosition == ECMLeverPosition::Positive
        ? 1.0f : -1.0f;
    OnLeverAlphaChanged(LeverAlpha);
    SetActorTickEnabled(false);
    ForceNetUpdate();
}
```

놓는 순간에는 `PressButton()`이나 `ReleaseButton()`을 호출하지 않는다. 레버는 마지막
방향에 고정된다.

### 7. 기존 SpringArm 경로

기존 `TryHandlePull_Implementation()`은 SpringArm이 한쪽 방향으로 레버를 당기는
별도 경로다. 유지한다면 Positive 전환과 동일한 내부 함수로 합쳐 중복 이벤트를
막는다. SpringArm으로 양방향 조작할 요구가 없다면 현재처럼 Positive 전환만
지원해도 된다.

### 레버 완료 조건

- 레버를 잡는 순간에는 Stage Trigger가 작동하지 않는다.
- 레버 축의 양/음 방향 이동만 반영되고 옆 방향 이동은 무시된다.
- Positive 임계점에서 `PressButton()`, Negative 임계점에서 `ReleaseButton()`이
  각각 한 번 호출된다.
- 놓아도 마지막 레버 방향과 Stage 상태가 유지된다.
- 레버를 잡고 놓은 입력은 디폴트 암 공격으로 이어지지 않는다.
- `LeverAlpha`와 최종 Stage 상태가 리슨 서버 및 원격 클라이언트에서 동일하다.

## Physics Handle + Hinge 레버 대안

레버의 물리적인 무게감과 반동이 필요하면 손과 레버를 하드 Constraint로
묶지 않고 다음과 같이 역할을 나눈다.

```text
World/LeverBase
  -> Physics Constraint: 레버의 이동 잠금, 회전 1축만 Limited
  -> Simulating LeverMesh
  -> Arm Physics Handle: 손 목표점으로 LeverMesh를 당김
```

- `LeverMesh`만 물리 시뮬레이션하고 레버 베이스는 고정한다.
- 레버의 Physics Constraint가 회전축, 양끝 Angular Limit, Damping을 결정한다.
- 팔은 위치만 추적하므로 레버의 회전 자유도와 부딪히지 않는다.
- 레버와 키메라 몸통을 Constraint로 연결하지 않는다. 레버가 키메라의
  앙커가 되어 전신을 당기거나 회전시키는 경로가 없어야 한다.

Query에서는 탐색용 Hold Volume의 위치를 잡는 지점으로 사용하되,
Physics Handle이 잡을 대상으로는 실제 `LeverMesh`를 반환한다.

```cpp
OutSpec.Priority = 100;
OutSpec.HoldLocation = ArmHoldVolume->GetComponentLocation();
OutSpec.HoldNormal = (ArmLocation - OutSpec.HoldLocation)
    .GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
OutSpec.TargetComponent = LeverMesh;
OutSpec.bUsePhysicsHandle = true;
return LeverMesh->IsSimulatingPhysics() && !HoldingArm.IsValid();
```

서버는 레버의 Constraint Angle이 임계값을 넘을 때 `PressButton()`/
`ReleaseButton()`을 각각 한 번만 호출한다. 끝에서 떨림이 생기면 팔의
Physics Handle보다 먼저 레버 Constraint의 Angular Limit과 Damping을 조정한다.

### 물리 Hinge 레버 완료 조건

- 키메라가 앞뒤로 이동하면 레버가 허용된 회전축으로만 움직인다.
- Angular Limit 밖으로 빠지거나 레버 전체가 이동하지 않는다.
- 레버가 벽에 고정된 앙커처럼 키메라 몸통을 당기지 않는다.
- 레버 각도가 임계값을 넘을 때만 Stage 상태가 한 번 전환된다.
- 놓으면 Physics Handle이 해제되고 레버 자체 Constraint는 유지된다.

## 통합 테스트 순서

1. 대상 없이 홀드하여 기존 지면 짚기가 유지되는지 확인한다.
2. 지면과 대상이 동시에 범위에 있을 때 대상이 우선되는지 확인한다.
3. 두 대상이 겹칠 때 높은 Priority, 같은 Priority에서는 가까운 대상이 선택되는지
   확인한다.
4. 대상 홀드 해제 후 팔 공격이 발생하지 않는지 확인한다.
5. Physics Handle 물체를 잡고 다리를 움직일 때 물체는 손을 따라오고
   키메라가 물체를 앙커로 회전하지 않는지 확인한다.
6. Physics Handle 물체가 벽에 걸린 상태에서도 키메라 전신에 폭발적인
   속도나 회전이 생기지 않는지 확인한다.
7. 홀드 중 팔이 파괴될 때 `EndArmHold()`가 실행되고, 대상이 먼저 파괴될 때는
   대상의 `EndPlay()` 정리가 실행되는지 확인한다.
8. 리슨 서버와 원격 클라이언트에서 손 목표, 물리 물체, 전선 위치,
   레버 상태를 비교한다.
