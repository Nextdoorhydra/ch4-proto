# 장애물 Google Sheet 밸런스

Google Sheet는 피해와 스테이지 피해 배율만 관리한다. 상태이상 종류, 지속시간과 강도는 각 장애물 BP 또는 배치 인스턴스에서 직접 설정한다.

## 피해 표

- Row Struct: `FCMObstacleDamageBalanceTableRow`
- Parser: `UObstacleDamageBalanceDataParser`
- 필수 열: `RowName`, `ID`, `ObstacleType`, `DefaultDamage`, `HighDamage`

`ObstacleType`은 `Laser`, `Blade`, `Fan`, `FloorSurface`, `AirborneZone`, `Turret` 중 하나다. `HighDamage`는 `DefaultDamage` 이상이어야 한다.

```tsv
RowName	ID	ObstacleType	DefaultDamage	HighDamage
Blade	Blade	Blade	10	25
Laser	Laser	Laser	15	35
Fan	Fan	Fan	0	0
Floor	Floor	FloorSurface	3	8
Air	Air	AirborneZone	2	5
Turret	Turret	Turret	10	25
```

## 스테이지 배율 표

- Row Struct: `FCMStageObstacleBalanceTableRow`
- Parser: `UStageObstacleBalanceDataParser`
- 필수 열: `RowName`, `ID`, `DamageMultiplier`

```tsv
RowName	ID	DamageMultiplier
TestStage	TestStage	1
Stage01	Stage01	1
Stage02	Stage02	1.1
Stage03	Stage03	1.2
```

## 배치 인스턴스

`CMStageObstacleBase > Obstacle > Balance`에서는 피해만 설정한다.

- 피해 전용: `Damage Balance Table/Row`만 지정
- 피해 없음: Damage Table/Row를 비움
- 커스텀 피해만 사용: Damage Mode=Custom, Custom Damage 입력. Damage Table/Row는 비워도 됨

`CMStageDirector > Stage > Balance`에서 `Obstacle Stage Balance Table/Row`를 스테이지당 한 번만 설정한다. 같은 월드에 등록된 모든 장애물이 이 배율을 자동으로 사용한다. Director 설정이 비어 있거나 유효하지 않으면 안전 기본값 `1.0`을 사용한다.

피해 계산:

- Default: `DefaultDamage × StageDamageMultiplier`
- High: `HighDamage × StageDamageMultiplier`
- Custom: `CustomDamage` 그대로

`Application Policy`와 `Period Seconds`는 장애물 BP의 Part Application 또는 Head Vision Application에서 설정한다. Periodic은 진입 즉시 한 번 적용한 뒤 주기마다 반복한다.

Part Application의 상태는 `None`, `Slowed`, `Electrified`만 지원한다. Slowed는 Status Duration과 Movement Multiplier를 설정하고, Electrified는 Status Duration 동안 해당 파츠 능력을 차단한다. 내부 GameplayTag와 차단 플래그는 코드가 자동 결정한다.

시야 감소는 공용 GE나 별도 Zone이 아니라 Hazard가 접촉한 `CMHeadPartActor`의 VisionComponent에 직접 적용한다. 장애물의 Head Vision Application을 켜고 Status Duration과 Vision Angle/Distance Multiplier를 설정한다. 오버랩 이벤트에서는 Hazard의 `NotifyTargetEntered/Exited`에 OtherActor와 OtherComp를 모두 전달해야 한다. 여러 Hazard가 겹치면 배율을 곱하지 않고 각 항목에서 가장 작은 배율 하나를 사용하며, 한 Hazard가 끝나면 남은 발생원의 배율로 복구한다.

실명은 영역 상태가 아니다. 섬광 Actor에 `CMFlashComponent`를 추가하고 폭발·발광 시점에 서버에서 `TriggerFlash()`를 호출한다. Flash Range 안이면서 각 머리의 시야 원뿔에 섬광 위치가 들어오고, 설정한 Trace Channel에서 벽에 가려지지 않은 머리만 Blind Duration 동안 실명된다. 모든 머리가 노출되면 서버가 한 머리를 무작위로 남긴다.

피해는 Hazard가 파츠와 해당 몸통 마디에 직접 적용한다. 실명과 시야 감소는 실제 머리 시야에 반영되며 시야각·거리는 각 VisionComponent에서 `Status Interpolation Speed`로 부드럽게 보간한다.
