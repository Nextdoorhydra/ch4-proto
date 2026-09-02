# Chimera 컨베이어 및 분배기 사용 가이드

> 레벨 배치와 블루프린트 연동에 필요한 내용만 정리한 문서다.

<details open>
<summary><strong>필수항목</strong></summary>

## 1. 사용하는 BP

| BP | 용도 |
|---|---|
| `BP_CMConveyorStraightSegment` | 직선 컨베이어 |
| `BP_CMConveyorCurve90Segment` | 90도 곡선 컨베이어 |
| `BP_CMConveyorDistributor` | 두 레인의 직진·교차 분배 |
| `BP_CMConveyorRouteController` | 연결된 컨베이어 전체 제어 |

각 컨베이어 BP에 메시와 이동 경로가 포함되어 있다. 별도의 스플라인 BP를 배치하거나 스플라인 포인트를 직접 편집할 필요가 없다.

## 2. 컨베이어 배치

1. 직선 또는 곡선 컨베이어 BP를 레벨에 배치한다.
2. 다음 컨베이어의 `Previous Placement Segment`에 이전 컨베이어를 지정한다.
3. `Snap To Previous Segment`를 실행한다.
4. 필요한 경로가 완성될 때까지 반복한다.

초록색 화살표가 시작점, 빨간색 화살표가 끝점이다. 앞 컨베이어의 빨간색 화살표와 다음 컨베이어의 초록색 화살표가 이어져야 한다.

## 3. 분배기 배치

1. `BP_CMConveyorDistributor`를 배치한다.
2. 양쪽 입력부에 들어오는 컨베이어의 끝점을 맞춘다.
3. 양쪽 출력부에 나가는 컨베이어의 시작점을 맞춘다.
4. 분배기 내부 메시 컴포넌트의 위치는 별도로 수정하지 않는다.

분배기 방향이 `Reverse`이면 입력과 출력의 의미가 반대가 된다.

## 4. 컨트롤러 배치

1. 하나의 연결된 네트워크에 `BP_CMConveyorRouteController`를 하나만 배치한다.
2. 컨트롤러의 `First Segment`에 시작 컨베이어를 지정한다.
3. 플레이한다.

컨트롤러는 시작 컨베이어에서 이어진 직선, 곡선, 분배기와 분배기 이후의 분기 경로를 자동으로 찾는다. 분배기의 출력마다 컨트롤러를 추가 배치하지 않는다.

서로 물리적으로 끊어진 네트워크에는 각각 컨트롤러가 필요하다. 컨트롤러 액터의 월드 위치는 실제 이동 경로에 영향을 주지 않는다.

## 5. 파츠 배치

1. `BP_CMLegPart` 등 플레이어가 획득할 수 있는 원본 파츠 BP를 벨트 위에 놓는다.
2. 원하는 위치와 회전으로 배치한다.
3. 파츠가 다른 액터에 Attach되어 있지 않은지 확인한다.
4. 플레이한다.

기본 설정에서는 레벨에 배치한 파츠의 위치 오프셋과 회전이 유지된 상태로 이동한다. 플레이어가 파츠를 획득하여 Attach하면 컨베이어 이동 대상에서 제외된다.

## 6. 방향 변경

전체 네트워크의 방향은 루트 `BP_CMConveyorRouteController`에서 변경한다.

| 함수 | 동작 |
|---|---|
| `SetConveyorDirection(Forward)` | 정방향으로 설정 |
| `SetConveyorDirection(Reverse)` | 역방향으로 설정 |
| `ReverseConveyorDirection()` | 현재 방향 반전 |

루트 컨트롤러에 함수를 호출하면 연결된 컨베이어 분기와 분배기에도 방향이 적용된다.

```text
외부 트리거 OnActivated
    → BP_CMConveyorRouteController 참조
    → ReverseConveyorDirection
```

## 7. 분배 방식 변경

| 모드 | 동작 |
|---|---|
| `Straight` | 들어온 레인 그대로 출력 |
| `Cross` | 반대편 레인으로 이동하여 출력 |

| 함수 | 동작 |
|---|---|
| `SetRoutingMode(Straight)` | 그대로 통과 |
| `SetRoutingMode(Cross)` | 반대편 레인으로 전환 |
| `ToggleRoutingMode()` | Straight와 Cross 전환 |

원하는 파츠가 분배기 중앙을 지나기 전에 모드를 설정해야 한다. 이미 중앙을 지난 파츠에는 변경 내용이 소급 적용되지 않는다.

## 8. 열린 경로와 순환 경로

- 열린 일반 컨베이어의 마지막에 도달한 파츠는 제거된다.
- 마지막 컨베이어가 처음으로 이어지는 순환 경로는 컨트롤러의 `Closed Loop`를 활성화한다.
- 분배기 출력에는 파츠를 받을 다음 컨베이어가 연결되어 있어야 한다.

</details>

<details>
<summary><strong>고급기능</strong></summary>

## 1. 분배기 좌우 외형 변경

| 함수 | 동작 |
|---|---|
| `SetSideMeshesSwapped(true/false)` | 좌우 에셋 배치를 지정 |
| `SwapSideMeshes()` | 현재 좌우 배치를 반전 |
| `AreSideMeshesSwapped()` | 현재 교체 상태 조회 |

좌우 교체 시 필요한 메시 회전과 위치 보정은 자동으로 적용된다. 메시 교체는 외형만 바꾸며 `Straight`와 `Cross`의 논리 동작은 유지된다.

## 2. 개별 분배기 방향 제어

독립적으로 사용하는 분배기는 다음 함수를 직접 호출할 수 있다.

| 함수 | 동작 |
|---|---|
| `SetDistributorDirection(Forward/Reverse)` | 방향 직접 지정 |
| `ReverseDistributorDirection()` | 현재 방향 반전 |
| `GetDistributorDirection()` | 현재 방향 조회 |

자동 네트워크에 포함된 분배기는 개별 제어보다 루트 컨트롤러에서 방향을 변경하는 것을 권장한다.

## 3. 선택 설정

| 설정 | 사용하는 경우 |
|---|---|
| `Move Speed` | 이동 속도를 변경할 때 |
| `Closed Loop` | 하나의 경로를 계속 순환시킬 때 |
| `Preserve Initial Part Placement` | 레벨에서 지정한 파츠 위치와 회전을 유지할 때 |
| `Align Parts To Path` | 파츠를 진행 방향에 강제로 정렬할 때 |
| `Part Rotation Offset` | 경로 정렬 후 추가 회전이 필요할 때 |
| `Part Height Offset` | 파츠의 경로 기준 높이를 조정할 때 |

분배기는 자체 `Move Speed`를 가진다. 컨베이어 구간과 분배기 구간의 속도를 같게 하려면 두 값을 동일하게 설정한다.

## 4. 자동 네트워크 검색

- 검색은 플레이 시작 시 수행된다.
- 시작점은 컨트롤러의 `First Segment`다.
- 분배기 이후 갈라진 경로에는 내부 컨트롤러가 자동 생성된다.
- 플레이 도중 새로 생성한 컨베이어는 자동 재검색되지 않는다.
- 같은 네트워크에 루트 컨트롤러를 여러 개 배치하지 않는다.

연결이 매우 가깝거나 의도적으로 간격을 둔 특수 배치에서만 다음 값을 조정한다.

| 설정 | 용도 |
|---|---|
| `Connection Tolerance` | 일반 컨베이어 사이 연결 허용 거리 |
| `Connection Angle Tolerance` | 연결 방향 허용 각도 |
| `Network Connection Tolerance` | 컨베이어와 분배기 연결 허용 거리 |
| `Handoff Acceptance Distance` | 파츠 인계 위치 허용 거리 |

허용 값을 지나치게 크게 설정하면 가까이에 있는 별도 컨베이어를 같은 네트워크로 인식할 수 있다.

## 5. 상태 조회와 이벤트

### 컨트롤러

| 함수·이벤트 | 용도 |
|---|---|
| `GetConveyorDirection()` | 현재 방향 조회 |
| `GetTrackedPartCount()` | 이동 중인 파츠 수 조회 |
| `GetRouteSegmentCount()` | 현재 경로 세그먼트 수 조회 |
| `GetConnectedDistributorCount()` | 연결된 분배기 수 조회 |
| `OnDirectionChanged` | 방향 변경 이벤트 |

### 분배기

| 함수·이벤트 | 용도 |
|---|---|
| `GetDistributorDirection()` | 현재 방향 조회 |
| `GetRoutingMode()` | 현재 분배 모드 조회 |
| `GetTrackedPartCount()` | 분배기 내부 파츠 수 조회 |
| `OnDirectionChanged` | 방향 변경 이벤트 |
| `OnRoutingModeChanged` | 분배 모드 변경 이벤트 |

## 6. 멀티플레이와 갱신 방식

방향, 분배 모드, 좌우 메시 변경 함수는 서버 권한에서 호출한다. 해당 상태는 클라이언트에 복제된다.

컨베이어와 분배기는 Actor Tick을 사용하지 않는다. 이동할 파츠가 있을 때만 내부 타이머가 동작한다.

</details>

---

테스트 절차와 장애 대응은 별도 문서인 `CMConveyor_Test_and_Troubleshooting.md`를 참고한다.
