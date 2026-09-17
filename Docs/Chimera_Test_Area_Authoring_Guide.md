# Chimera 테스트 구역 제작 가이드

## 1. 목적

Test 맵 하나를 Persistent Level로 유지하고 개발자별 테스트 공간을 Always Loaded Sublevel로 분리한다. 목적지에 도착하면 맵을 다시 열지 않고 Definition에 등록된 다음 구역의 PlayerStart로 공용 키메라를 이동한다. 마지막 구역 다음에는 첫 구역으로 순환한다.

일반 스테이지의 StageRoute와 StageDestination 동작은 변경하지 않는다.

## 2. Persistent Level 구성

다음 Actor를 Persistent Level에 배치한다.

- `CMTestAreaManager` 하나
- 기존 테스트용 `StageDirector` 하나
- 전체 테스트 맵이 공유하는 조명, Sky, Post Process

Persistent Level에는 별도의 PlayerStart를 배치하지 않는다. Definition의 첫 번째 구역 PlayerStart가 최초 키메라 생성과 마지막 구역 순환 복귀를 함께 담당한다. Test 맵 GameMode의 `Shared Chimera Player Start Tag`는 비워 둔다.

## 3. Sublevel 구성

각 Sublevel에는 다음 Actor를 배치한다.

- 구역 텔레포트 도착 위치용 `PlayerStart` 하나
- `CMTestAreaDestination` 하나
- 해당 구역에서 시험할 장애물과 Mechanism
- 해당 구역에만 필요한 로컬 조명과 장식
- 낙사·이탈 복귀가 필요한 위치의 `CMTestKillZone`

Levels 창에서 모든 테스트 Sublevel의 Streaming Method를 `Always Loaded`로 설정한다. 테스트 Actor와 구역 PlayerStart는 반드시 자신의 Sublevel이 Current Level인 상태에서 저장한다.

## 4. TestAreaDefinition 생성

Content Browser에서 `CMTestAreaDefinition` 타입의 Data Asset을 하나 생성한다. 예시는 `PDA_TestAreaDefinition`이다.

`Areas` 배열이 실제 이동 순서다.

| 배열 순서 | AreaId | AreaLevel | StartTag | DisplayName |
| ---: | --- | --- | --- | --- |
| 0 | `Blade` | `Test_Area_Blade` | `Test.Start.Blade` | 회전 칼날 |
| 1 | `Conveyor` | `Test_Area_Conveyor` | `Test.Start.Conveyor` | 컨베이어 |
| 2 | `Door` | `Test_Area_Door` | `Test.Start.Door` | 버튼과 문 |

- `AreaId`, `AreaLevel`, `StartTag`는 Definition 안에서 중복되면 안 된다.
- 배열 첫 항목의 PlayerStart가 Test 맵 최초 생성 위치다.
- 순서를 바꾸려면 Areas 배열 항목을 이동한다.
- 중간 구역 추가도 배열에 항목을 삽입하면 된다.
- `DisplayName`이 비어 있으면 UI에 AreaId가 표시된다.

Persistent Level의 `CMTestAreaManager.Area Definition`에 생성한 Definition을 연결한다.

## 5. PlayerStart 설정

각 Sublevel의 PlayerStart에서 `Player Start Tag`를 Definition의 `StartTag`와 동일하게 입력한다.

```text
칼날 구역 PlayerStart      Test.Start.Blade
컨베이어 구역 PlayerStart  Test.Start.Conveyor
문 구역 PlayerStart        Test.Start.Door
```

같은 PlayerStartTag를 두 곳에 사용하면 텔레포트를 거부하고 오류 로그를 남긴다. PlayerStart의 화살표 방향은 키메라가 도착 후 바라볼 방향이다.

## 6. Destination 설정

각 Sublevel의 `CMTestAreaDestination.Area Id`에는 자신이 속한 현재 구역의 AreaId를 입력한다.

예를 들어 Blade 구역은 다음과 같이 설정한다.

```text
Definition AreaId       Blade
Definition AreaLevel    Test_Area_Blade
Definition StartTag     Test.Start.Blade
PlayerStart Tag         Test.Start.Blade
Destination AreaId      Blade
```

Destination에는 다음 구역 ID를 적지 않는다. `Blade` 완료 후 어디로 갈지는 Definition 배열이 결정한다.

`CMTestAreaDestination`은 모든 활성 몸통 마디가 Trigger 안에 들어왔을 때만 완료된다. Test Sublevel에는 일반 게임용 `CMStageDestination`을 사용하지 않는다.

## 7. 구역 이동과 초기화

목적지 완료 또는 호스트 UI 이동 시 서버가 다음 순서로 처리한다.

1. Definition에서 대상 AreaId, AreaLevel, StartTag를 찾는다.
2. StartTag가 일치하고 AreaLevel에 속한 PlayerStart를 찾는다.
3. 등록된 Sublevel의 장애물과 Mechanism을 초기화한다.
4. 공용 키메라의 마디 배치를 유지한 채 PlayerStart 위치로 이동한다.
5. 물리 속도를 제거하고 위치를 클라이언트에 복제한다.

초기화 대상은 다음 함수가 있는 기반 Actor다.

- `CMStageObstacleBase.ResetObstacle()`
- `CMStageMechanismBase.ResetMechanism()`

`CMTestKillZone`은 별도 AreaId를 입력하지 않는다. 자신이 저장된 Sublevel을 Definition의 AreaLevel과 비교해 해당 구역 PlayerStart로 키메라 전체를 복귀시킨다. KillZone은 반드시 복귀할 구역의 Sublevel에 저장해야 하며, Trigger 크기는 `Kill Volume.Box Extent`로 조정한다.

## 8. 구역 선택 UI

Test Persistent Level의 GameMode는 `CMTestHUD` 기반 Blueprint를 HUD Class로 사용한다. TestHUD는 로컬 플레이어마다 최상위 Test Widget을 생성하고 `Game And UI` 입력과 마우스 커서를 설정한다.

행 Widget Blueprint:

1. 부모를 `CMTestAreaRowWidget`으로 지정한다.
2. Button 이름을 `Btn_Select`로 지정한다.
3. TextBlock 이름을 `Txt_AreaName`으로 지정한다.
4. 두 위젯의 `Is Variable`을 활성화한다.

목록 Widget Blueprint:

1. 부모를 `CMTestAreaSelectorWidget`으로 지정한다.
2. ScrollBox 이름을 `SB_TestAreas`로 지정한다.
3. `Area Row Class`에 행 Widget Blueprint를 지정한다.
4. Test HUD에서 목록 위젯을 생성해 Viewport에 추가한다.

최상위 Test Widget과 HUD Blueprint:

1. `UserWidget` 기반 `WBP_TestHUD`를 생성한다.
2. Designer에 위에서 만든 Test Area Selector를 배치한다.
3. `CMTestHUD` 기반 `BP_CMTestHUD`를 생성한다.
4. `BP_CMTestHUD.Test Overlay Class`에 `WBP_TestHUD`를 지정한다.
5. Test 맵 GameMode의 `HUD Class`에 `BP_CMTestHUD`를 지정한다.

목록은 Definition의 Areas 배열에서 자동 생성된다. Listen Server 호스트와 Standalone 실행자만 이동 버튼을 사용할 수 있다. 클라이언트도 목록은 볼 수 있지만 버튼은 비활성화된다. Shipping 빌드에서는 테스트 구역 조작을 허용하지 않는다.

## 9. 개발자 체크리스트

1. 본인 Sublevel을 Test Persistent Level에 추가한다.
2. Streaming Method를 Always Loaded로 설정한다.
3. 본인 Sublevel을 Current Level로 선택한다.
4. PlayerStart와 CMTestAreaDestination을 배치한다.
5. Definition에 AreaId, AreaLevel, StartTag, DisplayName을 추가한다.
6. PlayerStart Tag와 Definition StartTag를 일치시킨다.
7. Destination AreaId와 Definition AreaId를 일치시킨다.
8. 2인 PIE Listen Server에서 전체 몸통 판정과 호스트 UI 이동을 확인한다.
9. Output Log에서 Definition 누락, 중복 태그, PlayerStart 누락 오류가 없는지 확인한다.

## 10. 현재 범위 밖

- Sublevel 런타임 스트리밍과 언로드
- 클라이언트의 임의 구역 이동 권한
- 테스트 중 사망 시 같은 구역으로 즉시 복귀
- 구역별 별도 GameMode 또는 StageRoute

테스트 맵이 실제로 무거워진 뒤에만 동적 Sublevel 스트리밍을 검토한다.
