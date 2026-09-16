# Chimera

Chimera는 여러 플레이어가 하나의 키메라를 함께 조종하는 Unreal Engine 기반 협동 게임 프로젝트입니다. 각 플레이어는 공용 신체의 일부 마디와 파츠를 맡아 이동하고, 전투하고, 스테이지의 장애물과 퍼즐을 해결합니다.

현재 프로젝트는 개발 중입니다. 로비와 Steam 리슨 서버, 파츠 기반 조작, 스테이지 진행, 적대 AI, UI, 사운드, 비동기 에셋 로드, 데이터 제작 도구를 포함합니다.

## 주요 기능

- 하나의 키메라를 여러 플레이어가 나누어 조작하는 서버 권한 멀티플레이
- 다리, 팔, 스프링 암, 머리 등 교체 가능한 파츠와 Gameplay Ability System 연동
- 로비부터 스테이지 로드, 플레이, 결과, 다음 맵 이동까지 이어지는 게임 흐름
- 버튼, 레버, 압력판, 전력 케이블, 컨베이어, 레이저 등의 퍼즐·장애물 시스템
- Tetra, Ripper, Centipede 및 Sacrifice 계열 AI와 Learning Agents 연동
- 시야, 핑, 카메라 가림 처리, HUD, 사운드, 절단·혈흔 표현
- Google Sheet 데이터를 DataTable과 Primary Data Asset으로 변환하는 DataForge 파이프라인
- 주요 런타임 규칙과 에디터 도구를 검증하는 Unreal Automation Test

## 개발 환경

| 항목 | 요구 사항 |
|---|---|
| 엔진 | Unreal Engine 5.7 |
| 플랫폼 | Windows 64-bit |
| 그래픽 API | DirectX 12 / Shader Model 6 |
| IDE | Visual Studio 또는 JetBrains Rider |
| 소스 관리 | Git, Git LFS |

Visual Studio를 사용하는 경우 루트의 `.vsconfig`를 불러오면 필요한 C++ 게임 개발 구성 요소를 설치할 수 있습니다.

## 시작하기

### 1. 저장소 받기

Git LFS를 설치한 뒤 저장소를 복제합니다. LFS를 설치하지 않으면 `.uasset`, `.umap` 등의 바이너리 에셋이 정상적으로 내려오지 않습니다.

```powershell
git lfs install
git clone <repository-url>
cd Chimera
git lfs pull
```

### 2. 프로젝트 파일 생성 및 빌드

1. `Chimera.uproject`를 우클릭하고 **Generate Visual Studio project files**를 선택합니다.
2. 생성된 `Chimera.sln`을 Visual Studio 또는 Rider에서 엽니다.
3. `ChimeraEditor / Win64 / Development Editor` 구성을 빌드합니다.

명령줄 빌드가 필요한 경우 다음 명령을 사용할 수 있습니다.

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' `
  ChimeraEditor Win64 Development `
  -Project="$PWD\Chimera.uproject" `
  -WaitMutex -NoHotReloadFromIDE
```

### 3. 에디터 실행

`Chimera.uproject`를 열거나 다음 스크립트를 실행합니다.

```powershell
.\Scripts\LaunchChimeraEditor.ps1
```

실행 스크립트는 임시 파일과 Derived Data Cache를 개발용 쓰기 가능 경로로 분리합니다. Unreal Engine이나 프로젝트를 현재 스크립트가 가정한 경로와 다른 곳에 설치했다면 `$editor`와 캐시 경로를 먼저 수정해야 합니다.

기본 시작 맵은 다음과 같습니다.

```text
/Game/Chimera/Environment/Level/MainMenu/L_MainMenu
```

## 테스트

Unreal Editor의 **Tools > Test Automation**에서 `Chimera` 테스트를 선택해 실행할 수 있습니다. 명령줄에서는 다음과 같이 전체 프로젝트 테스트를 실행합니다.

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  "$PWD\Chimera.uproject" `
  -unattended -nop4 -nosplash -NullRHI `
  -ExecCmds='Automation RunTests Chimera' `
  -TestExit='Automation Test Queue Empty'
```

플러그인 테스트는 `DataForge`, `ListenServerNetwork`처럼 각 테스트의 루트 이름으로 별도 실행할 수 있습니다.

## 프로젝트 구조

```text
Chimera/
├─ Config/                 프로젝트 설정, 입력, Gameplay Tag
├─ Content/Chimera/        게임 전용 Unreal 에셋
├─ Docs/                   시스템 설계 및 제작 가이드
├─ Plugins/                프로젝트 플러그인
├─ Scripts/                에디터 실행 및 Control Rig 보조 도구
└─ Source/                 게임 및 에디터 C++ 모듈
```

### C++ 모듈

| 모듈 | 역할 |
|---|---|
| `Chimera` | 플레이어 신체, 파츠, 전투, 스테이지, 퍼즐, 게임 흐름 |
| `AI` | 적대 AI, 이동·인지·행동, Learning Agents |
| `UI` | 메인 메뉴, 로비, HUD, 옵션, 로딩 및 결과 화면 |
| `CMSound` | 게임 사운드 데이터와 스테이지 오디오 흐름 |
| `CMGore` | 절단 반응, 혈흔 표면 및 VFX |
| `Shared` | 모듈 간 공유 타입과 게임 흐름 메시지 |
| `ChimeraEditor` | Google Sheet 파서, DataForge 연동, 에디터 전용 기능 |
| `CameraOcclusionEditor` | 카메라 가림 머티리얼 변환 도구 |

### 주요 프로젝트 플러그인

| 플러그인 | 역할 |
|---|---|
| `ListenServerNetwork` | 리슨 서버 세션과 로비 네트워크 기반 |
| `AsyncPDALoader` | Primary Data Asset 비동기 로드와 수명 관리 |
| `GameplayMessageRouter` | Gameplay Message 기반 로컬 이벤트 전달 |
| `DataForge` | 외부 데이터의 검증, 바인딩, 에셋 생성 파이프라인 |
| `GoogleSheetLoader` | Google Sheet 데이터 수집 및 파싱 |
| `NKMUI`, `UIExtension` | UI 프레임워크와 확장 지점 |
| `NKMSound`, `NKMLocalization` | 사운드 및 현지화 기반 기능 |
| `McpAutomationBridge` | Unreal Editor 자동화 인터페이스 |

## 런타임 개요

플레이어는 공용 키메라를 직접 Possess하지 않습니다. 각 플레이어가 자신의 `ControlBody`를 Possess하고, 서버 RPC를 통해 자신에게 배정된 신체 마디와 파츠를 조작합니다. 공용 키메라와 Gameplay Ability System은 서버가 권한을 가지며 모든 클라이언트에 상태를 복제합니다.

스테이지 흐름은 `LobbyGameMode`에서 시작해 `StageRouteSubsystem`, `PlayGameMode`, `StageDirector`로 이어집니다. 각 클라이언트의 로드 결과는 서버 배리어에 보고되며, 필요한 에셋이 준비된 뒤에만 플레이 상태로 전환됩니다.

프로젝트 에셋은 `/Game/Chimera` 아래에서 기능 단위로 관리합니다. 새 콘텐츠를 추가할 때는 에셋 종류보다 소유 기능을 먼저 기준으로 삼습니다.

## 데이터 제작

게임 밸런스와 파츠 데이터는 Google Sheet, DataTable, Primary Data Asset을 연결하는 DataForge 파이프라인으로 관리합니다.

- 런타임 코드는 생성된 DataTable과 Primary Data Asset만 사용합니다.
- 외부 데이터의 스키마, 바인딩, 생성 규칙은 DataForge RuleSet에서 관리합니다.
- 전체 데이터 검증은 다음 명령으로 실행할 수 있습니다.

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  "$PWD\Chimera.uproject" -run=DataForge -All -ValidateOnly
```

자세한 제작 절차는 [DataForge 사용자 가이드](Docs/DataForge_UserGuide.md)를 참고하세요.

## 문서

- [게임 흐름과 스테이지 런타임 구조](Docs/Chimera_GameFlow_Stage_Architecture.md)
- [스테이지 제작 가이드](Docs/Chimera_Stage_Authoring_Guide.md)
- [테스트 구역 제작 가이드](Docs/Chimera_Test_Area_Authoring_Guide.md)
- [콘텐츠 폴더 구조](Docs/Content_Folder_Structure.md)
- [비동기 로드 개발 가이드](Docs/Chimera_Async_Load_Developer_Guide.md)
- [장애물과 메커니즘 구조](Docs/Chimera_Obstacle_Mechanism_Architecture.md)
- [트리거와 퍼즐 상태 가이드](Docs/Trigger_Puzzle_State_Guide.md)
- [전력 시스템 가이드](Docs/Chimera_Power_System_Guide.md)
- [시야 시스템 가이드](Docs/Vision_System_Guide.md)
- [Wireframe HUD 구조](Docs/Wireframe_HUD_Architecture.md)
- [DataForge 구조](Docs/DataForge_Architecture.md)
- [DataForge MCP 가이드](Docs/DataForge_MCP_Guide.md)

세부 플러그인 사용법은 각 플러그인 폴더의 README에서 확인할 수 있습니다.

## 작업 시 주의사항

- `.uasset`, `.umap` 등 바이너리 에셋은 Git LFS로 관리합니다.
- 생성물인 `Binaries`, `Intermediate`, `Saved`, `DerivedDataCache`는 커밋하지 않습니다.
- 게임 전용 에셋은 원칙적으로 `Content/Chimera` 아래에 배치합니다.
- 런타임 게임플레이 코드에서 동기 에셋 로드를 추가하지 말고 공용 비동기 로드 흐름을 사용합니다.
- 네트워크 게임 상태는 서버가 결정하고, 클라이언트에는 복제 또는 명시적인 RPC 경계를 통해 전달합니다.
