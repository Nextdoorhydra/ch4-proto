# Chimera 전력 시스템 사용 설명서

## 1. 개요

Chimera 전력 시스템은 다음 구조로 전력을 전달합니다.

```text
Power Source → Power Cable → Power Socket → Power Cable → Power Socket
```

전력은 서버에서 계산됩니다. Socket은 단순히 케이블이 연결된 것과 실제로 전력을 공급받는 것을 구분합니다.

- 물리적 연결: 케이블이 연결되어 있음
- 전력 공급: 연결된 경로를 통해 활성화된 Source의 전력이 도달함

따라서 발전기에 연결되지 않은 케이블이나 제품끼리만 연결된 케이블은 연결 상태일 수 있지만 전력을 공급하지 않습니다.

## 2. 기본 사용 방법

### 2.1 발전기 만들기

아무 Actor나 선택한 뒤 `CMPowerSourceComponent`를 추가합니다.

```text
Actor
└─ CMPowerSourceComponent
```

이 Actor는 전력 공급원이 됩니다.

기본적으로 `bPowerEnabled = true`이므로 연결된 케이블에 전력을 공급합니다. 런타임에서 전원을 제어하려면 다음 Blueprint 함수를 사용할 수 있습니다.

```text
SetPowerEnabled(true)   // 전원 공급
SetPowerEnabled(false)  // 전원 차단
```

### 2.2 제품 만들기

아무 Actor나 선택한 뒤 `CMPowerSocketComponent`를 추가합니다.

```text
Actor
└─ CMPowerSocketComponent
```

Socket이 전력을 공급받으면 `IsPowered()`가 true가 됩니다.

제품에 여러 Socket을 추가할 수도 있습니다. 예를 들어 입력용 Socket과 출력용 Socket을 각각 추가하면 제품 간 전력 전달이 가능합니다.

### 2.3 케이블 배치

레벨에 `CMPowerCableActor`를 배치합니다.

플레이어가 케이블을 잡고 Source 또는 Socket 근처에서 놓으면 자동으로 연결됩니다. 케이블의 양쪽 끝은 다음 대상에 연결할 수 있습니다.

- `CMPowerSourceComponent`
- `CMPowerSocketComponent`

케이블을 연결할 때 `PowerChannel`이 양쪽에서 같아야 합니다. 기본값은 `DefaultPower`입니다.

### 2.4 제품끼리 미리 연결하기

두 제품의 Socket을 케이블로 연결할 수 있습니다. 이때 두 Socket 모두 전원이 없어도 연결 자체는 가능합니다.

```text
제품 A Socket → 케이블 → 제품 B Socket
```

이후 발전기를 제품 A에 연결하면 전력이 제품 B까지 전파됩니다.

```text
Source → 제품 A Socket → 제품 B Socket
```

Spawn 시 자동 연결을 사용하면 케이블에서 다음 옵션을 설정합니다.

- `bAutoConnectOnSpawn = true`
- `InputActor`: 입력 측 Actor
- `OutputActor`: 출력 측 Actor

두 Actor에 각각 Source 또는 Socket을 추가하면 BeginPlay 때 자동 연결됩니다. 두 Socket을 연결하는 경우 방향은 `InputActor → OutputActor`입니다. 한쪽 Actor만 지정하면 한쪽 끝만 연결된 케이블로 생성할 수 있습니다.

## 3. 전력 트리거 사용 방법

전력이 연결되었을 때 버튼, 문, 레이저 등의 기능을 활성화하려면 해당 Actor를 `ACMPowerTriggerBase`를 상속한 Blueprint로 만듭니다.

```text
Power Trigger Actor
├─ CMPowerSocketComponent
└─ ActivationTrigger
```

`RequiredSockets`에 전력을 확인할 Socket을 지정합니다.

- `RequiredSockets`가 비어 있으면 같은 Actor에 있는 모든 `CMPowerSocketComponent`를 자동으로 사용합니다.
- `bRequireAllSockets = true`이면 모든 Socket이 전력을 받아야 활성화됩니다.
- `bRequireAllSockets = false`이면 하나 이상의 Socket만 전력을 받아도 활성화됩니다.

예시:

```text
Generator → Cable → Button Socket
```

Button Socket이 Powered 상태가 되면 Trigger가 활성화됩니다. 전원 공급이 끊기면 Trigger가 비활성화됩니다.

레이저, 문 등 다른 기능을 작동시키려면 Power Trigger의 기존 `TargetActor`, `TargetPlacementId`, `TargetGroup`, `TargetCommandTag` 설정을 함께 지정합니다.

## 4. 주요 옵션

### 4.1 CMPowerSourceComponent

| 옵션 | 기본값 | 설명 |
|---|---:|---|
| `PowerChannel` | `DefaultPower` | 연결 가능한 전력 채널입니다. 케이블과 값이 같아야 합니다. |
| `MaxConnectedCables` | `1` | Source에 동시에 연결할 수 있는 케이블 수입니다. |
| `bAllowCableDisconnect` | `true` | 연결된 케이블을 다시 분리할 수 있는지 결정합니다. |
| `ConnectionRadius` | `150` | 플레이어가 케이블을 놓았을 때 연결 가능한 거리입니다. |
| `bPowerEnabled` | `true` | Source의 전력 공급 여부입니다. |

`bPowerEnabled`를 false로 설정하면 물리적 연결은 유지되지만 전력은 전달되지 않습니다.

주요 함수:

- `IsPhysicallyConnected()`: 케이블이 연결되어 있는지 확인합니다.
- `IsProvidingPower()`: 현재 전력을 공급 중인지 확인합니다.
- `SetPowerEnabled(bool)`: 전원을 On/Off합니다.
- `GetConnectedCableCount()`: 연결된 케이블 수를 반환합니다.

### 4.2 CMPowerSocketComponent

| 옵션 | 기본값 | 설명 |
|---|---:|---|
| `PowerChannel` | `DefaultPower` | 연결 가능한 전력 채널입니다. |
| `ConnectionRadius` | `150` | 케이블을 연결할 수 있는 거리입니다. |
| `MaxConnectedCables` | `1` | Socket에 동시에 연결할 수 있는 케이블 수입니다. |
| `bAllowCableDisconnect` | `true` | 연결된 케이블의 분리 가능 여부입니다. |

주요 함수:

- `IsPhysicallyConnected()`: 케이블 연결 여부만 확인합니다.
- `IsPowered()`: 실제 전력 공급 여부를 확인합니다.
- `CanProvidePower()`: Socket이 현재 다른 Socket으로 전력을 전달할 수 있는지 확인합니다.
- `GetConnectedCableCount()`: 연결된 케이블 수를 반환합니다.

`IsPhysicallyConnected()`가 true여도 발전기와 연결된 전력 경로가 없으면 `IsPowered()`는 false입니다.

### 4.3 ACMPowerCableActor

#### 일반 옵션

| 옵션 | 기본값 | 설명 |
|---|---:|---|
| `CableDefinition` | - | 케이블 메시지와 로프 설정을 가진 DA입니다. |
| `LoadGroupId` | `Stage.Entry.Power` | 케이블 시각 리소스를 로드할 그룹입니다. |
| `PowerChannel` | `DefaultPower` | 연결 대상과 일치해야 하는 전력 채널입니다. |

#### Spawn 자동 연결 옵션

| 옵션 | 기본값 | 설명 |
|---|---:|---|
| `bAutoConnectOnSpawn` | `false` | BeginPlay 때 자동 연결을 사용할지 결정합니다. |
| `InputActor` | 없음 | 전력 입력 측 Actor입니다. Source 또는 upstream Socket을 찾습니다. |
| `OutputActor` | 없음 | 전력 출력 측 Actor입니다. 대상 Socket을 찾습니다. |

자동 연결은 연결 반경을 무시하고 실행되지만, PowerChannel, 최대 연결 수 등의 유효성 검사는 수행합니다.

#### 개별 케이블 시각/로프 Override

`bOverrideDefinitionSettings = true`일 때만 아래 값이 Cable Definition 대신 사용됩니다.

| 옵션 | 설명 |
|---|---|
| `OverrideVisualSegmentCount` | 케이블 시각 세그먼트의 기본 개수입니다. |
| `OverrideCableThicknessScale` | 케이블 두께 배율입니다. |
| `OverrideInitialCableLength` | 초기 케이블 길이입니다. |
| `OverrideRopeNodeSpacing` | 로프 시뮬레이션 노드 간격입니다. 작을수록 더 세밀하지만 비용이 증가합니다. |
| `OverrideRopeGravityScale` | 로프에 적용되는 중력 배율입니다. |
| `OverrideRopeDamping` | 로프 움직임의 감쇠량입니다. 높을수록 빨리 안정됩니다. |
| `OverrideRopeConstraintIterations` | 길이 제약 보정 반복 횟수입니다. 높을수록 형태가 안정적이지만 비용이 증가합니다. |
| `OverrideRopeCollisionRadius` | 로프 충돌 검사 반경입니다. |
| `OverrideInitialCoilRadius` | 시작 시 말림 형태의 반지름입니다. |
| `bOverrideStartCoiled` | 시작 시 케이블을 말린 상태로 만들지 결정합니다. |

케이블이 안정된 상태가 되면 로프 Tick이 자동으로 비활성화됩니다. 다시 잡거나 연결/해제하면 시뮬레이션이 재활성화됩니다.

### 4.4 ACMPowerTriggerBase

| 옵션 | 기본값 | 설명 |
|---|---:|---|
| `RequiredSockets` | 비어 있음 | 전력 조건을 확인할 Socket 목록입니다. 비어 있으면 같은 Actor의 Socket을 자동 수집합니다. |
| `bRequireAllSockets` | `true` | true면 모든 Socket, false면 하나 이상의 Socket이 Powered여야 합니다. |

Power Trigger는 `OnConnectionChanged`가 아니라 Socket의 `OnPowerStateChanged`를 기준으로 동작합니다. 따라서 케이블이 연결되어 있어도 전력이 없으면 활성화되지 않습니다.

## 5. 연결 해제

연결된 케이블을 다시 잡으면 잡은 쪽 끝점을 분리하려고 시도합니다.

- 해당 Source/Socket의 `bAllowCableDisconnect = true`: 분리 후 잡을 수 있습니다.
- `bAllowCableDisconnect = false`: 해당 끝점을 잡을 수 없습니다.
- 반대쪽 고정 끝점은 잡은 끝점과 무관하게 유지됩니다.

Blueprint에서는 다음 함수를 사용할 수도 있습니다.

- `Disconnect()`: 양쪽 연결을 모두 해제합니다.
- `DisconnectFromSocket()`: Socket 쪽 연결만 해제합니다.
- `DisconnectFromSource()`: Source 쪽 연결만 해제합니다.

## 6. 멀티플레이 주의사항

전력 계산과 연결 변경은 서버 권한으로 처리됩니다.

- 케이블 연결/해제 결과는 서버가 결정합니다.
- Source On/Off도 서버에서 실행해야 합니다.
- Trigger 활성화/비활성화도 서버에서 처리됩니다.
- 케이블 연결 상태, Source 전원 상태, Socket 상태는 클라이언트에 복제됩니다.

공용 카메라를 사용하는 경우 전력 결과는 모든 플레이어에게 동일하게 표시하면 됩니다. 클라이언트 입력으로 전원을 변경해야 한다면 클라이언트에서 직접 `SetPowerEnabled()`를 호출하지 말고, 프로젝트의 상호작용 Server RPC를 통해 서버에서 호출해야 합니다.

## 7. 문제 해결 체크리스트

전력이 전달되지 않는 경우 다음을 확인합니다.

1. Source의 `bPowerEnabled`가 true인지 확인합니다.
2. Source와 모든 Cable, Socket의 `PowerChannel`이 같은지 확인합니다.
3. 케이블이 양쪽 끝에 모두 연결되어 있는지 확인합니다.
4. 중간 Socket의 `MaxConnectedCables`가 초과되지 않았는지 확인합니다.
5. 제품에 `CMPowerSocketComponent`가 실제로 추가되어 있는지 확인합니다.
6. Trigger의 `RequiredSockets`가 올바른 Socket을 가리키는지 확인합니다.
7. `bRequireAllSockets`가 의도한 조건과 맞는지 확인합니다.
8. 멀티플레이에서는 변경 함수가 서버에서 실행되는지 확인합니다.

