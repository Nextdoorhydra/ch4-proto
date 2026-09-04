# 트리거 ON/OFF와 퍼즐 명령

## 상태 계약

- 기본 버튼: 눌린/점등 상태가 ON. Toggle On Hit이면 다음 타격이 OFF, 아니면 Press Duration + Pulse Hold Duration 후 자동 OFF.
- 레버: Local Pull Axis의 양의 방향 임계값에서 ON, 반대 임계값에서 OFF. 중간 구간에서는 직전 상태 유지.
- 압력판: Required Weight 이상이면 ON. Release Weight 미만이면 OFF. 두 임계값 사이에서는 직전 상태 유지.
- 시야석: Vision Stone Mode의 인원/시야 조건 충족이 ON, 불충족이 OFF. Require No Watching Players 모드에서는 아무도 안 보는 것이 ON.
- 모든 기본 스위치의 ON은 Activated, OFF는 Deactivated. Pulse는 상태 없는 외부/호환 입력으로만 남긴다.
- Start Active/장치 Enabled는 입력 허용 상태이지 스위치의 ON 상태가 아니다. Reset은 입력 신호를 재생하지 않고 초기화한다.

## ON/OFF 전환마다 장애물 반전

하나의 채널에서 다음과 같이 설정한다.

- Trigger Condition: Any
- Accepted Signal: ON / OFF
- Steps: 1개, Command: Toggle, Targets: 제어할 장애물
- End Behavior: Loop

켜진 레이저라면 ON 때 꺼지고 OFF 때 켜진다. Toggle은 현재 대상 상태의 반전이지, 스위치 ON을 대상 ON에 맞추는 명령이 아니다.

상태를 명시적으로 맞추려면 채널을 분리한다. 같은 트리거에 ON Only + Activate, OFF Only + Deactivate를 연결한다. 반대로 동작하려면 두 명령을 바꾼다. 두 채널 모두 Loop로 설정한다.

## 옵션 점검

| 옵션 | 동작 |
| --- | --- |
| ON / OFF | Activated와 Deactivated만 허용. 새 채널의 기본값 |
| ON Only | Activated만 허용 |
| OFF Only | Deactivated만 허용 |
| Trigger Condition: Any | 등록된 트리거 하나의 허용 신호마다 Step 실행 |
| All / Latched | 각 트리거의 허용 입력을 누적. 모두 충족되면 실행하고 누적 목록 비움 |
| All / Simultaneous | 현재 상태의 All Active / All Inactive / All Equal을 평가. 해제될 때 자동 역명령을 실행하지 않음 |
| Sequence | Expected Trigger Sequence 순서대로 허용 신호를 소비. OFF까지 입력으로 쓸지 필터로 결정 |
| Stop | 마지막 Step 뒤 채널 완료. 이후 ON/OFF 모두 무시 |
| Loop | 마지막 Step 다음에 이 채널의 Index 0으로 복귀. 다음 허용 신호까지 대기 |
| Repeat Current | 마지막 Step에 머물러 다음 허용 신호마다 재실행 |
| Reset Targets With Puzzle | 퍼즐 리셋 시 연결 대상도 초기 상태로 복원. 완료 시 자동 리셋 옵션 아님 |
| Toggle On Hit | 기본 버튼 자체의 상태 전환 방식. 대상의 Toggle 명령과 별개 |
| One Shot | 최초 ON 이후 추가 ON 차단. OFF는 허용. Reset 후 재사용. Toggle On Hit은 반복 입력을 위해 이를 해제 |

## 기존 콘텐츠 이행

편집 화면에서는 ON Only / OFF Only / ON / OFF 세 가지 신호만 선택한다. 기존 enum 항목의 값과 이름은 로드 호환용으로 숨겨 유지한다. 로드 및 BeginPlay에서 PulseOrActivated는 ON Only로, Any는 ON / OFF로 변환한다. 세 옵션 모두 Pulse는 받지 않는다. 기존 ON 전용 필터를 양방향으로 바꾸지는 않으므로 ON/OFF 모두 반응하려면 ON / OFF를 선택하고, 반복이 필요하면 Stop을 Loop로 바꾼다.

기본 버튼의 자동 복귀 모드는 이제 실제 OFF도 전달한다. 예전에는 화면만 돌아오고 내부 상태는 ON으로 남았다. 기존 Any 필터에서 변환된 콘텐츠에서 자동 OFF 반응이 불필요하면 ON Only로 설정한다. 별도의 Blueprint가 직접 Pulse를 보내던 경우에는 ON/OFF 상태 신호로 수정해야 한다.

동일 상태의 중복 알림은 컨트롤러에서 무시하고 동일 명령의 Targets 내 중복 참조도 한 번만 실행한다. 별개의 채널/컨트롤러가 같은 대상을 제어하는 경우는 독립 실행되므로 콘텐츠에서 의도를 확인해야 한다.
