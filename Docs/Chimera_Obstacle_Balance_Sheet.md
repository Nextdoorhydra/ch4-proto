# 장애물 Google Sheet 밸런스

피해, 상태이상, 스테이지 배율을 각각 독립된 표로 관리한다. 배치 장애물은 필요한 표만 선택하므로 피해 전용, 상태 전용, 피해+상태 조합을 모두 지원한다.

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

## 상태이상 표

- Row Struct: `FCMObstacleStatusBalanceTableRow`
- Parser: `UObstacleStatusBalanceDataParser`
- 필수 열: `RowName`, `ID`, `StatusEffect`, `DefaultDuration`, `PrimaryStatusValue`, `SecondaryStatusValue`

상태가 없는 장애물은 상태 행을 선택하지 않는다. 따라서 `None` 행은 만들지 않는다.

```tsv
RowName	ID	StatusEffect	DefaultDuration	PrimaryStatusValue	SecondaryStatusValue
Slow50	Slow50	PartSlowed	3	0.5	0
Electric	Electric	PartElectrified	2	0	0
Confused	Confused	BodyConfused	4	0	0
Delirious	Delirious	BodyDelirious	4	0	0
BlindOneHead	BlindOneHead	BodyBlinded	4	1	0
VisionReduced60	VisionReduced60	BodyVisionReduced	5	0.6	0.6
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

`CMStageObstacleBase > Obstacle > Balance`에서는 피해와 상태 행만 설정한다.

- 피해 전용: `Damage Balance Table/Row`만 지정
- 상태 전용: `Status Balance Table/Row`만 지정
- 피해+상태: 두 종류 모두 지정
- 피해 없음: Damage Table/Row를 비움
- 상태 없음: Status Table/Row를 비움
- 커스텀 피해만 사용: Damage Mode=Custom, Custom Damage 입력. Damage Table/Row는 비워도 됨

`CMStageDirector > Stage > Balance`에서 `Obstacle Stage Balance Table/Row`를 스테이지당 한 번만 설정한다. 같은 월드에 등록된 모든 장애물이 이 배율을 자동으로 사용한다. Director 설정이 비어 있거나 유효하지 않으면 안전 기본값 `1.0`을 사용한다.

피해 계산:

- Default: `DefaultDamage × StageDamageMultiplier`
- High: `HighDamage × StageDamageMultiplier`
- Custom: `CustomDamage` 그대로

지속시간:

- Default: 상태 표의 `DefaultDuration`
- Custom: `CustomDuration` 그대로

`Application Policy`와 `Period Seconds`는 장애물 BP의 Part/Chimera Application에서 설정한다. 같은 상태 행을 장판, 레이저 등 서로 다른 적용 주기의 장애물이 재사용할 수 있도록 시트에서는 관리하지 않는다. Periodic은 진입 즉시 한 번 적용한 뒤 주기마다 반복한다.

피해량, 상태 태그, 지속시간, 이동 배율, 행동 차단 여부와 해석된 상태 Enum은 테이블에서 계산되므로 에디터에 노출하지 않는다. 파츠 상태 GameplayTag는 `Chimera.State.Part.Slowed`, `Chimera.State.Part.Electrified`만 사용한다.

몸통 상태이상 GE에는 다음 SetByCaller 값만 전달한다.

- `Data.Obstacle.Duration`
- `Data.Obstacle.Status.Primary`
- `Data.Obstacle.Status.Secondary`

피해는 Hazard가 파츠와 해당 몸통 마디에 직접 적용한다. GE에서 같은 피해를 다시 적용하지 않는다. 혼란, 착란, 실명, 시야 감소는 현재 데이터 파싱과 값 전달까지만 준비되어 있으며 실제 플레이 동작은 후속 구현 범위다.
