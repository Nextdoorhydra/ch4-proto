# 버튼·압력판 UI 공통 API 인계 가이드

기준: 2026-08-31. 공통 상태 조회/변경 이벤트 및 표시용 상태 복제를 구현했다.
위젯, UI ActorComponent, 메시 눌림 애니메이션, 머티리얼/사운드 연출은 아직 만들지 않았다.
배치·퍼즐은 [구성 가이드](Chimera_Obstacle_Mechanism_Architecture.md)를 참고한다.

## 1. UI 담당자가 사용할 두 진입점

소유 타입은 **ACMStageTriggerBase**이며 기본 버튼·압력판·레버가 상속한다.

| API | 사용 |
|---|---|
| GetPresentationState() | BlueprintPure. 초기 표시/재연결 시 FCMTriggerPresentationState 조회 |
| OnPresentationStateChanged(State) | BlueprintAssignable. 서버 상태 변경과 클라이언트 복제 수신의 공통 이벤트 |

서버/클라이언트 분기 없이 동일한 API를 사용한다. 일반 버튼은 무게 표시를 끄고 압력판은 같은 구조체의 무게 필드를 사용한다. 클라이언트 UI는 다른 개별 게임 상태와 섞지 말고 **이 스냅샷 하나**로 그린다.

## 2. FCMTriggerPresentationState 필드

| 필드 | 의미 |
|---|---|
| bReady | 서버에서 초기 상태가 만들어졌거나 클라이언트가 해당 상태를 수신했는지 |
| bEnabled | 트리거 사용 가능 상태. 눌림 여부와 다름 |
| bTriggered | 버튼 ON/눌림 상태 |
| bCanActivate | 다음 활성화 허용 여부. One Shot 이력 포함 |
| bSupportsWeight | 압력판이면 true. false인 일반 버튼은 무게 필드를 표시하지 않음 |
| CurrentWeight | 현재 합산 데이터 무게 |
| RequiredWeight | ON 전환 목표치 |
| ReleaseWeight | OFF 전환 기준 |

bCanActivate는 ‘현재 안 눌림’이 아니다. One Shot=false이면 이미 눌렸어도 true일 수 있다. Press가 동일 상태를 다시 바꾸는 것은 아니다.

버튼 ON과 대상 레이저 OFF는 동시에 성립한다. 빨간 램프가 버튼 눌림을 뜻하면 bTriggered를 사용한다. 대상 위험 상태를 표시하려면 대상의 IsObstacleActive()가 별도 원본이다.

## 3. BP 연결 순서

1. UI 컴포넌트에 담당 Trigger Actor 참조를 명시적으로 전달한다.
2. 이전 대상이 있으면 해당 대상 이벤트에서 자신의 바인딩만 해제한다.
3. 대상 OnPresentationStateChanged에 사용자 이벤트를 Bind한다.
4. 즉시 GetPresentationState를 호출하여 같은 화면 갱신 함수로 전달한다.
5. State.bReady=false이면 로딩/미준비 표시를 사용한다.
6. bTriggered로 버튼 눌림/Emissive, bEnabled로 사용 불가 표현, bSupportsWeight로 무게 UI 표시 여부를 결정한다.
7. EndPlay 또는 대상 교체 시 바인딩을 해제한다.

예시의 ApplyPresentation은 **UI 담당자가 구현할 함수 이름**이며 이번에 추가된 엔진 API가 아니다.

```text
Bind OnPresentationStateChanged → ApplyPresentation(State)
GetPresentationState           → ApplyPresentation(State)

ApplyPresentation:
  !Ready          → 초기 대기 표시
  Triggered       → 눌림 위치 + 램프 ON
  !Triggered      → 원위치 + 램프 OFF
  SupportsWeight  → 현재/목표 텍스트 및 게이지 표시
```

바인딩 전에 발생한 이벤트는 재생하지 않는다. 따라서 Bind + 최초 Get을 함께 사용한다. UI가 월드의 첫 버튼을 자동 선택하거나 두 번째 ActivationTrigger 컴포넌트를 추가해서는 안 된다.

### C++ 수신 함수 형태

```cpp
// UCLASS로 선언한 UI 컴포넌트의 헤더에 작성할 예시
UFUNCTION()
void HandlePresentation(const FCMTriggerPresentationState& State);

// 담당 Trigger 연결 시
Trigger->OnPresentationStateChanged.AddUniqueDynamic(
    this, &ThisClass::HandlePresentation);
HandlePresentation(Trigger->GetPresentationState());

// 대상 해제 시 (유효한 대상에 한해)
Trigger->OnPresentationStateChanged.RemoveDynamic(
    this, &ThisClass::HandlePresentation);
```

include는 Stage/Trigger/CMStageTriggerBase.h와 Stage/Trigger/CMTriggerPresentationState.h를 사용한다. 수신자 내부의 대상 참조는 수명/언로드를 고려해 관리한다.

## 4. 압력판 표시

일반 UI는 공통 State만 읽으면 된다. 압력판 전용 코드에는 다음 조회 함수도 제공한다.

- GetCurrentWeight()
- GetRequiredWeight()
- GetReleaseWeight()

서버에서는 실제 값을, 클라이언트에서는 복제받은 PresentationState 값을 반환한다. 클라이언트에서 첫 수신 전에는 0일 수 있으므로 bReady를 먼저 확인한다. 별도 값들을 읽는 것보다 공통 State 하나를 받아 사용하는 쪽이 권장 경로다.

```text
텍스트 = 현재 무게 75 / 목표 무게 100
게이지 = RequiredWeight > 0 ? Clamp(CurrentWeight / RequiredWeight, 0, 1) : 1
눌림 램프 = State.bTriggered
안내 = 무게가 ReleaseWeight 미만이면 해제
```

Required=100, Release=90이면 100에서 ON, 95/90에서 유지, 89에서 OFF다. 95% 게이지라고 OFF로 추측하지 않는다. 게이지는 포화해도 텍스트에는 150/100처럼 실제 합계를 표시할 수 있다.

현재 무게는 Actor별 데이터 무게이며 같은 Actor의 여러 Collider는 한 번만 합산한다. 서버 오버랩 재계산 시 무게만 바뀌어도 새 스냅샷을 통지한다. Reset 시 무게/눌림도 갱신한다.

목표/해제 값도 스냅샷에 포함되지만, 런타임 밸런스 편집용 setter는 이번 범위에 없다. BP/레벨 설정을 기본으로 사용한다. 영역 안 무게 변경의 자동 재계산 및 Reset 후 기존 오버랩 재검색 역시 기존 제한을 유지한다.

## 5. 네트워크 및 게임 로직 경계

```text
서버 ActivationTrigger 변경 / 압력판 무게 재계산
 → FillPresentationState
 → 변경 시 PresentationState 저장 및 복제 요청
 → 서버 OnPresentationStateChanged

클라이언트 PresentationState OnRep
 → 클라이언트 OnPresentationStateChanged
```

- 스냅샷은 ReplicatedUsing 속성이다. 클라이언트에서 게임 상태를 다시 계산하지 않는다.
- 동일 스냅샷은 서버에서 중복 방송하지 않는다. UI도 같은 화면 상태를 다시 적용해도 안전하게 만든다.
- bReady는 초기 기본값과 구별되어 첫 복제/늦은 UI 생성에 사용할 수 있다.
- 기존 ActivationTrigger 복제는 호환성을 위해 유지한다. 도착 순서가 다를 수 있으므로 UI에서 IsTriggered()와 State.CurrentWeight를 섞어 쓰지 않는다.
- 기존 OnButtonPressed/Released, OnActivated/Deactivated, OnTriggerSignal은 서버 게임 이벤트로 유지한다. **OnRep에서 퍼즐 신호를 재실행하지 않는다.**
- Reset은 기존 Release 이벤트가 없어도 표시 스냅샷으로 반영된다.
- 비활성화는 눌림을 자동 해제하지 않는다. bEnabled와 bTriggered를 별도로 표현한다.
- 이 이벤트는 지속 상태 표현용이다. 빠른 Pulse 전체나 같은 상태로 끝난 중간 변화의 애니메이션 재생을 보장하지 않는다.
- Actor가 클라이언트에 존재하고 복제되는 범위에서 동작한다. 언로드된 룸의 UI 대상은 해제한다.
- UI에서 PressButton/ReleaseButton을 호출해도 서버 RPC로 전송되지 않는다. 입력 요청은 기존 소유 플레이어 서버 입력 경로를 사용한다.

## 6. 레버 연출

레버도 공통 State로 ON/OFF 표시한다. 회전 진행률은 기존 전용 계약을 사용한다.

| 값/이벤트 | 의미 |
|---|---|
| LeverAlpha / OnLeverAlphaChanged(float) | -1~+1 논리 목표. 서버 및 클라이언트 OnRep 경로 |
| OnLeverVisualAlphaChanged(float) | 로컬에서 실제 보간 회전 값이 바뀔 때 통지 |
| RotationTransitionDuration | 끝점 간 시각 이동 시간 |

진행률은 (Alpha+1)/2다. 논리 목표와 화면 회전 값을 구분한다. 레버 전용 이벤트는 자식 BP 구현 이벤트이지 외부 Bind용 delegate가 아니다. 필요하면 레버 BP가 UI 컴포넌트에 전달한다.

LeverPivot 회전은 C++에서 처리하므로 UI에서 같은 Pivot에 SetRotation을 반복하지 않는다. 무게 공통 API와 달리 시각 Alpha getter/공통 스냅샷 편입은 이번 범위가 아니다. 초기 시각 게이지 연결은 별도 요구사항으로 남는다.

그랩 팔의 OnPullTargetResolved 결과는 Unhandled(대상 미처리), HandledNoChange(입력 소비·변화 없음), Applied(적용)다. 공통 표시 이벤트로 이 순간 결과까지 대신 처리하지 않는다.

## 7. 구현 범위와 테스트

이번 구현:
- 공통 조회/BlueprintAssignable 이벤트와 표시용 복제 구조체.
- 초기화/활성 상태 변경/Press/Release/Reset의 서버 갱신.
- 압력판 현재·목표·해제 무게 및 클라이언트 조회.
- 기존 퍼즐 신호와 UI 알림 분리.

인계 후 구현:
- 실제 UI ActorComponent/Widget, 버튼 메시 위치/Emissive/사운드.
- 대상 연결·해제, bReady 처리, 최초 조회.
- 네트워크 PIE에서 방장/클라이언트, 지연 접속, 룸 재로드 시 실제 화면 검증.

자동 테스트 Chimera.Trigger.Presentation은 서버 초기 상태, One Shot, 비활성/눌림 분리, Reset, 압력판 100→95→90→89 및 중복 Collider 합산을 검사한다. 실제 네트워크 전송/위젯 연출 검증을 대체하지 않는다.

## 코드 위치

- Source/Chimera/Stage/Trigger/CMTriggerPresentationState.h
- Source/Chimera/Stage/Trigger/CMStageTriggerBase.h 및 .cpp
- Source/Chimera/Stage/Trigger/Component/CMActivationTriggerComponent.h 및 .cpp
- Source/Chimera/Stage/Trigger/CMPressurePlateBase.h 및 .cpp
- Source/Chimera/Tests/CMTriggerPresentationTests.cpp
