# Chimera 콘텐츠 폴더 구조 가이드

## 1. 목적

이 문서는 Unreal Engine 프로젝트의 게임 콘텐츠를 `Content/Chimera` 아래에 일관되게 배치하기 위한 기준을 정의한다.

구조의 핵심은 **기능 응집성(feature cohesion)** 이다. Blueprint, Mesh, Material처럼 자산 종류가 같다는 이유만으로 프로젝트 전역에 모으지 않고, 함께 변경되고 함께 사용되는 자산을 동일한 기능 폴더에 둔다.

```text
권장: Content/Chimera/Character/Player/{Blueprint, Mesh, Material, Texture}
지양: Content/{Blueprints, Meshes, Materials, Textures}/Player
```

이 구조는 다음을 목표로 한다.

- 한 기능에 필요한 자산을 한 위치에서 탐색한다.
- 기능의 소유 범위와 의존 관계를 명확히 한다.
- 기능 추가, 이동, 삭제 시 영향 범위를 작게 유지한다.
- 프로젝트 고유 자산과 외부 콘텐츠를 구분한다.

## 2. 기본 원칙

### 2.1 프로젝트 자산은 `Content/Chimera`에 둔다

직접 제작하고 유지보수하는 게임 자산의 루트는 `/Game/Chimera`로 통일한다.

엔진 콘텐츠, 플러그인 콘텐츠, Marketplace 원본 패키지는 각자의 루트에 유지한다. 외부 자산을 수정해 프로젝트 전용으로 사용할 경우에는 필요한 자산만 `Chimera` 아래의 해당 기능 폴더로 옮기거나 복제한다.

### 2.2 첫 번째 분류 기준은 자산 종류가 아니라 기능이다

최상위 폴더는 게임의 시스템 또는 도메인을 나타낸다.

- `Character`: 플레이 가능한 캐릭터와 게임 내 인물
- `Environment`: 레벨, 스폰, 장애물 등 월드 구성 요소
- `UI`: 공용 UI 기반, HUD, 프런트엔드 화면
- `GameMode`: 플레이 규칙과 게임 흐름
- `Input`: 입력 액션과 매핑 컨텍스트
- `Localization`: 현지화 관련 자산
- `Ability`: Gameplay Ability System 관련 자산
  - `GA`: Gameplay Ability
  - `GE`: Gameplay Effect

### 2.3 자산 종류 폴더는 기능 폴더 안에서 사용한다

기능 폴더의 말단에서 자산이 많아질 때 다음과 같이 종류별 하위 폴더를 둘 수 있다.

- `Blueprint`
- `Mesh`
- `Material`
- `Texture`
- `Niagara`

모든 기능에 동일한 빈 폴더 세트를 미리 만들 필요는 없다. 실제 자산이 생길 때 필요한 폴더만 생성한다. 자산 수가 적으면 기능 폴더 바로 아래에 두고, 탐색성이 떨어지는 시점에 종류별 폴더로 분리한다.

### 2.4 공유 범위는 가능한 한 좁게 유지한다

한 기능에서만 쓰는 자산은 해당 기능 안에 둔다. 여러 하위 기능이 함께 쓰는 자산만 가장 가까운 공통 상위 폴더의 `Common` 또는 `Foundation`으로 올린다.

```text
UI/HUD에서만 사용       -> UI/HUD/...
UI 전반에서 공용        -> UI/Foundation/...
Character 전반에서 공용 -> Character/Common/...
프로젝트 전반에서 공용  -> 제한적으로 Chimera/Common/...
```

`Common`, `Shared`, `Misc`는 책임이 불명확한 자산의 임시 보관소가 되기 쉬우므로 명확한 재사용 근거가 있을 때만 사용한다.

## 3. 목표 폴더 구조

아래 구조는 현재 설계안의 기준 구조다. 중괄호는 실제 폴더명이 아니라, 해당 기능 안에 필요에 따라 둘 수 있는 자산 종류 폴더를 뜻한다.

```text
Content/
└─ Chimera/
   ├─ GameMode/
   ├─ Input/
   ├─ Localization/
   ├─ Environment/
   │  ├─ Spawn/
   │  ├─ Level/
   │  └─ Obstacle/
   │     └─ {Blueprint, Mesh, Material, Texture, Niagara}
   ├─ UI/
   │  ├─ Foundation/
   │  ├─ HUD/
   │  └─ Frontend/
   ├─ Character/
   │  ├─ Part/
   │  ├─ Player/
   │  └─ <CharacterName>/
   │     └─ {Blueprint, Mesh, Material, Texture, Niagara}
   └─ Ability/
      ├─ GA/
      └─ GE/
```

### 폴더별 책임

| 경로 | 책임 | 예시 자산 |
|---|---|---|
| `GameMode` | 게임 규칙, 상태 전환, 플레이 흐름 | GameMode Blueprint, GameState 관련 자산 |
| `Input` | Enhanced Input 설정 | Input Action, Input Mapping Context |
| `Localization` | 언어 및 텍스트 현지화 | String Table, Localization Target 관련 자산 |
| `Environment/Spawn` | 플레이어·NPC·오브젝트 생성 지점과 규칙 | Spawn Point, Spawn Manager |
| `Environment/Level` | 레벨 고유 구성과 월드 자산 | Level Blueprint 보조 자산, Level Instance |
| `Environment/Obstacle` | 장애물의 표현과 동작 | 장애물 Blueprint, Mesh, Material |
| `UI/Foundation` | 여러 UI 영역이 공유하는 기반 요소 | 공용 위젯, 스타일, 폰트, UI Material |
| `UI/HUD` | 플레이 중 표시되는 UI | 체력, 조준점, 상태 표시 위젯 |
| `UI/Frontend` | 플레이 전후의 화면 흐름 | 타이틀, 로비, 설정, 결과 화면 |
| `Character/Part` | 여러 캐릭터가 조립·공유하는 파츠 | 신체·장비 Mesh, 파츠 정의 데이터 |
| `Character/Player` | 플레이어 공통 기능 | Player Character, Controller 연계 자산 |
| `Character/<CharacterName>` | 특정 캐릭터 전용 자산 | 전용 Blueprint, Mesh, Material, Texture |
| `Ability/GA` | Gameplay Ability 정의 | `GA_CM*` Blueprint |
| `Ability/GE` | Gameplay Effect 정의 | `GE_CM*` Blueprint |

## 4. 자산 배치 판단 순서

새 자산을 추가할 때 다음 순서로 위치를 결정한다.

1. 이 자산이 담당하는 **기능 또는 도메인**을 찾는다.
2. 하나의 세부 기능에서만 사용된다면 그 기능 폴더에 둔다.
3. 같은 도메인의 여러 기능에서 사용된다면 가장 가까운 `Foundation` 또는 `Common`을 검토한다.
4. 자산 수가 많아 탐색이 어려우면 `Blueprint`, `Mesh`, `Material`, `Texture`, `Niagara`로 나눈다.
5. 어디에 둘지 애매하면 전역 `Misc`에 넣지 말고 자산의 소유 기능을 먼저 결정한다.

예시:

```text
플레이어 전용 애니메이션 BP -> Character/Player/Blueprint
특정 장애물의 Static Mesh   -> Environment/Obstacle/Mesh
모든 HUD가 쓰는 UI Material  -> UI/Foundation/Material
타이틀 화면 전용 Texture     -> UI/Frontend/Texture
```

## 5. 명명 규칙

폴더명은 Unreal Content Browser에서 읽기 쉽도록 영문 PascalCase를 사용한다. 실제 캐릭터명도 가능하면 영문 식별자로 정하고, 표시 이름은 현지화 데이터에서 관리한다.

### 5.1 기본 형식

프로젝트에서 만드는 클래스와 자산은 다음 형식을 사용한다.

```text
<Type>_<ProjectPrefix><Name>_<Numbering>
```

- `Type`: Unreal 자산 타입 접두사. 예: `BP`, `WBP`, `T`, `M`, `SM`, `NS`, `GA`, `GE`
- `ProjectPrefix`: Chimera 프로젝트 접두사인 `CM`. 생략하지 않는다.
- `Name`: 자산의 역할을 나타내는 PascalCase 이름
- `Numbering`: 같은 이름의 변형을 구분할 때 붙이는 번호. 필요하지 않으면 생략할 수 있다.

```text
BP_CMSomeItem
BP_CMSomeItem_1
BP_CMSomeItem_2
T_CMItem_1
T_CMItem_2
M_CMObstacleBarricade
NS_CMObstacleImpact
GA_CMJump
GE_CMInvulnerable
```

`T_Item_1`처럼 `CM`이 없는 프로젝트 자산명은 사용하지 않는다. 프로젝트가 직접 소유하는 클래스와 Blueprint에는 반드시 `CM`을 붙이며, 다른 자산에도 같은 프로젝트 접두사 규칙을 적용한다.

### 5.2 UI 이름

UI 자산에는 일반 `CM` 대신 UI 전용 접두사 `CMUI`를 사용한다. `CMUI` 자체가 프로젝트 식별자 `CM`을 포함하므로 `CMCMUI`처럼 중복해서 쓰지 않는다.

```text
WBP_CMUIHealthBar
WBP_CMUIMainMenu
M_CMUIBackground
T_CMUIIcon_1
```

### 5.3 C++ 클래스와 Blueprint

C++ 클래스와 Blueprint 모두 프로젝트 소유 여부를 이름에서 식별할 수 있어야 한다.

```text
ACMPlayerCharacter
UCMInventoryComponent
FCMItemData
ECMCharacterState
BP_CMPlayerCharacter
```

Unreal의 C++ 타입 접두사(`A`, `U`, `F`, `E`, `I`) 다음에 프로젝트 접두사 `CM`을 붙인다. Blueprint는 자산 타입 접두사 다음에 `_CM`을 붙인다.

### 5.4 Gameplay Tag

프로젝트에서 정의하는 모든 Gameplay Tag는 최상위 루트 `Chimera`로 시작한다.

```text
Chimera.Ability.Jump
Chimera.State.Invulnerable
Chimera.Character.Player
Chimera.UI.Frontend
```

기능별 태그를 별도의 최상위 루트로 만들지 않는다. 예를 들어 `Ability.Jump` 대신 `Chimera.Ability.Jump`를 사용한다.

## 6. 의존성 규칙

폴더 구조가 실제 응집성을 유지하려면 참조 방향도 함께 관리해야 한다.

- 세부 기능은 같은 도메인의 `Foundation` 또는 `Common`을 참조할 수 있다.
- 공용 기반 폴더가 특정 세부 기능을 역으로 참조하지 않도록 한다.
- `UI/Foundation`은 `UI/HUD`나 `UI/Frontend`의 구체 자산을 참조하지 않는다.
- 캐릭터 공용 파츠는 특정 캐릭터 전용 Blueprint를 참조하지 않는다.
- Environment, UI, Character 사이의 직접 참조가 늘어나면 인터페이스, 이벤트, 데이터 자산을 통한 결합 완화를 검토한다.
- 순환 참조가 생기지 않도록 Reference Viewer와 Size Map으로 주기적으로 확인한다.

## 7. 기존 콘텐츠 이관 기준

현재 루트의 `Blueprints`, `Data`, `Maps` 자산은 일괄적으로 같은 이름의 폴더에 옮기지 않는다. 각 자산의 실제 책임을 확인한 후 목표 기능 폴더로 분류한다.

1. Content Browser의 Reference Viewer로 참조 관계를 확인한다.
2. 자산의 소유 기능을 정하고 `/Game/Chimera/...`로 이동한다.
3. Unreal Editor의 **Fix Up Redirectors in Folder**를 실행한다.
4. 주요 맵을 열고 Blueprint 컴파일 및 PIE를 수행한다.
5. 소스 코드, Config, DataTable 등에 문자열로 기록된 Soft Object Path를 확인한다.

파일 탐색기에서 `.uasset` 파일을 직접 이동하면 참조가 깨질 수 있으므로 반드시 Unreal Editor의 Content Browser 또는 검증된 에디터 도구를 사용한다.

## 8. 설계 시 추가로 확정할 항목

다음 항목은 팀 합의 후 이 문서에 반영한다.

- `Environment/Level`과 프로젝트의 맵 파일 저장 위치를 동일하게 할지 여부
- `Character/Part`가 외형 파츠만 담당하는지, 장비와 부착물까지 포함하는지 여부

## 9. 완료 기준

폴더 구조 정리는 다음 조건을 만족하면 완료된 것으로 본다.

- 프로젝트 고유 게임 자산이 `/Game/Chimera` 아래에 있다.
- 각 자산의 위치만 보고 소유 기능을 파악할 수 있다.
- 하나의 기능을 수정할 때 필요한 주요 자산이 같은 기능 트리 안에 있다.
- 불필요한 빈 타입 폴더와 책임이 불명확한 `Misc` 폴더가 없다.
- `GA`와 `GE`가 `/Game/Chimera/Ability` 아래에 배치되어 있다.
- 프로젝트 자산과 클래스에 `CM`, UI 자산에 `CMUI` 접두사가 적용되어 있다.
- 프로젝트 Gameplay Tag가 모두 `Chimera` 루트로 시작한다.
- Redirector 정리 후 끊어진 참조가 없다.
- 주요 맵 로드, Blueprint 컴파일, PIE가 정상 동작한다.
