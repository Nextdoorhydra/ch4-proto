# Listen Server Network

`ListenServerNetwork`는 Unreal Engine 5.7.4의 OSSv1과 `OnlineSubsystemSteam`을 사용해 Steam Lobby 기반 리슨 서버의 공통 수명 주기를 제공하는 단일 Runtime 플러그인입니다. 커스텀 `GameInstance`, 전용 `GameMode`, Manager Actor 또는 프로젝트 측 OSS delegate 관리가 필요하지 않습니다.

## 1. 지원 범위

- Steam 초기화·LocalUserNum 0 로그인 검사
- `NAME_GameSession` Host, Find, Join, Quick Match, Leave
- Create, Update, Start, End, Destroy, CancelFindSessions
- 기존 세션 제거 후 Host/Invite 진행
- Listen OpenLevel, ClientTravel, host ServerTravel 및 맵 로드 완료 확인
- ProjectKey, BuildUniqueId, GameMode, Region, 문자열 Extra Attribute 필터
- `FTSTicker` 기반 작업별 timeout과 작업 ID 기반 stale callback 차단
- NetworkFailure, TravelFailure, SessionFailure 복구
- 실행 중 Steam 초대 수신/수락과 Steam Overlay 초대 UI
- 설정 검증, 로그, debug snapshot, 네 개의 진단 console command
- OSS와 분리된 정책·레이아웃 Automation Test

## 2. 지원하지 않는 범위

Cold-start 초대, `+connect_lobby`, 전용 서버, 호스트 마이그레이션, 중앙 매칭, MMR, 파티, 로비 플레이어/Ready/팀/게임 규칙, 채팅, 음성, EOS/OSSv2, Blueprint Async Action, Blueprint RPC, UMG, replicated manager, Steam 업적·Cloud·Depot·안티치트·크로스플레이는 포함하지 않습니다.

## 3. 10분 Quick Start

1. `Plugins/ListenServerNetwork`를 프로젝트에 복사하고 `.uproject`에서 플러그인을 활성화합니다.
2. 아래 Steam/NetDriver 설정을 `DefaultEngine.ini`에 병합합니다.
3. `/Game/Maps/L_MainMenu`, `/Game/Maps/L_Lobby`, `/Game/Maps/L_Game` World asset을 에디터에서 만듭니다.
4. Project Settings > Listen Server Network에서 ProjectKey, BuildUniqueId, 세 맵을 확인합니다.
5. 패키징 설정의 Cook 목록에 세 맵을 포함합니다.
6. 메뉴 코드나 Blueprint에서 Game Instance Subsystem을 얻어 `HostDefaultSession`, `FindDefaultSessions`, `QuickMatchDefault` 등을 호출합니다.
7. 시작 시 `ValidateConfiguration()` 또는 `LSN.Validate`를 실행합니다.
8. 서로 다른 Steam 계정과 서로 다른 프로세스에서 테스트합니다.

현재 `SteamListenLab` 프로젝트에는 맵 asset이 없으므로 기본 Host/Quick Match는 맵을 만들기 전까지 의도적으로 요청을 거부합니다. Codex는 `.umap` 바이너리를 생성하지 않았습니다.

## 4. 설치와 모듈 의존성

플러그인을 다른 프로젝트로 복사한 뒤 프로젝트 descriptor에 추가합니다.

```json
{
  "Name": "ListenServerNetwork",
  "Enabled": true
}
```

플러그인의 descriptor가 `OnlineSubsystem`, `OnlineSubsystemUtils`, `OnlineSubsystemSteam`을 활성화합니다. C++ 프로젝트 코드에서 공개 헤더를 사용할 모듈은 자신의 `Build.cs`에 다음 의존성만 추가합니다.

```csharp
PrivateDependencyModuleNames.Add("ListenServerNetwork");
```

프로젝트 코드는 `IOnlineSession`이나 OSS delegate 헤더를 include할 필요가 없습니다.

## 5. UE 5.7.4 Steam 설정

새 프로젝트에서 기존 `GameNetDriver` 정의가 없다면 다음을 사용합니다.

```ini
[/Script/Engine.GameEngine]
!NetDriverDefinitions=ClearArray
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="/Script/SteamSockets.SteamSocketsNetDriver",DriverClassNameFallback="/Script/OnlineSubsystemUtils.IpNetDriver")

[OnlineSubsystem]
DefaultPlatformService=Steam

[OnlineSubsystemSteam]
bEnabled=true
SteamDevAppId=480
bUseSteamNetworking=true

[/Script/SteamSockets.SteamSocketsNetDriver]
NetConnectionClassName="/Script/SteamSockets.SteamSocketsNetConnection"
```

UE 5.7에서는 `SteamSockets` 플러그인도 활성화해야 합니다. `OnlineSubsystemSteam.SteamNetDriver`는 UE 5.7 런타임 클래스가 아니므로 사용하면 `IpNetDriver`로 fallback되어 Steam P2P 주소로 ClientTravel할 수 없습니다. 이미 프로젝트가 `NetDriverDefinitions`를 정의한다면 위 블록을 그대로 추가하지 말고 기존 배열을 병합하십시오. 같은 `DefName="GameNetDriver"`를 여러 번 추가하면 선택이 모호해집니다. 이 저장소에는 기존 프로젝트 정의가 없었기 때문에 상속된 기본 배열을 한 번 비우고 SteamSockets 정의 하나만 추가했습니다.

UE 5.7.4의 Steam 소스에서 `bInitServerOnClient`는 클라이언트 프로세스에서 Steam Game Server API 초기화를 시도하게 하는 옵션입니다. 이 플러그인이 사용하는 `bUsesPresence=true`, `bUseLobbiesIfAvailable=true` 경로는 Steam Client의 Lobby API를 사용하므로 설정하지 않았습니다. 전용/인터넷 game-server 경로로 설계를 바꿀 때만 다시 검토하십시오.

Steam 구현은 `bUsesPresence`와 `bUseLobbiesIfAvailable` 값이 같아야 한다고 직접 검사합니다. 플러그인은 둘 다 항상 `true`로 설정하고 검색에는 `SEARCH_LOBBIES=true`를 사용합니다.

## 6. Project Settings

설정 section은 `DefaultGame.ini`의 다음 계층입니다.

```ini
[/Script/ListenServerNetwork.ListenServerNetworkSettings]
ProjectKey=MyGame
BuildUniqueId=1
DefaultMaxPlayers=4
DefaultMaxSearchResults=50
DefaultGameModeId=Default
DefaultRegion=None
DefaultSessionDisplayName=My Steam Lobby
MainMenuMap=/Game/Maps/L_MainMenu.L_MainMenu
LobbyMap=/Game/Maps/L_Lobby.L_Lobby
DefaultGameMap=/Game/Maps/L_Game.L_Game
CreateTimeoutSeconds=30.0
SearchTimeoutSeconds=20.0
JoinTimeoutSeconds=30.0
UpdateTimeoutSeconds=15.0
DestroyTimeoutSeconds=15.0
TravelTimeoutSeconds=45.0
bAllowJoinInProgress=true
bEnableVerboseLogging=false
```

`BuildUniqueId`와 광고되는 `BUILD_VERSION`은 이 한 값에서 생성됩니다. 네트워크 호환성이 깨지는 릴리스마다 증가시키십시오. `ProjectKey`는 App ID 480을 공유하는 다른 Spacewar Lobby를 제외할 수 있도록 프로젝트마다 고유하게 정합니다.

## 7. 맵 생성과 Cook 확인

에디터에서 다음 World asset을 만듭니다.

```text
/Game/Maps/L_MainMenu
/Game/Maps/L_Lobby
/Game/Maps/L_Game
```

Project Settings > Packaging의 Cook 대상에 세 맵을 명시적으로 포함하거나 asset 참조 체인을 통해 포함되는지 패키징 로그로 확인하십시오. 런타임 `ValidateConfiguration`은 로컬 package 존재 여부는 검사하지만 최종 Cook 포함 여부까지 보장할 수 없습니다.

원격 `MAP_ID`는 표시와 상태 판단에만 사용합니다. 원격 metadata를 로컬 map 경로, travel URL 또는 console command로 사용하지 않습니다.

## 8. Subsystem 획득

```cpp
#include "ListenServerSessionSubsystem.h"

UListenServerSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UListenServerSessionSubsystem>();
```

`UListenServerSessionSubsystem`은 replicated object가 아니며 서버·클라이언트가 공유해야 하는 게임 상태를 저장하지 않습니다.

## 9. 기본 Host

```cpp
if (Sessions != nullptr)
{
    Sessions->HostDefaultSession();
}
```

성공은 Steam `CreateSession` 완료가 아니라 Lobby map의 `PostLoadMapWithWorld`, 예상 package, `NM_ListenServer`까지 확인한 뒤 전달됩니다.

## 10. 기본 Find와 결과 개별 조회

```cpp
Sessions->FindDefaultSessions();

for (int32 Index = 0; Index < Sessions->GetSearchResultCount(); ++Index)
{
    FListenServerSearchResult Result;
    if (Sessions->GetSearchResultByIndex(Index, Result))
    {
        // Result.Handle, SessionDisplayName, players, mode, region 등을 표시합니다.
    }
}
```

전체 배열을 Blueprint 반환값으로 복사하지 않습니다. C++에서 무복사 순회가 필요하면 `GetSearchResultsView()`를 사용합니다. 새 검색이 시작되면 `SearchGeneration`이 증가하고 이전 handle은 즉시 무효입니다.

## 11. Join

```cpp
FListenServerSearchResult Result;
if (Sessions->GetSearchResultByIndex(0, Result))
{
    Sessions->JoinSession(Result.Handle);
}
```

Join 성공 판정은 `JoinSession` callback, `GetResolvedConnectString`, primary PlayerController `ClientTravel`, 새 World load, `NM_Client` 확인까지 포함합니다. connect string 전체는 로그에 남기지 않습니다.

## 12. Quick Match

```cpp
Sessions->QuickMatchDefault();
```

Steam 결과 순서대로 후보를 시도하고, 후보가 모두 실패하면 한 번 재검색한 뒤 Host fallback을 수행합니다. Steam Lobby ping이 유효하더라도 debug 값으로만 보존하고 정렬 기준으로 사용하지 않습니다.

## 13. Host 게임 이동

```cpp
Sessions->HostTravelToDefaultGameMap();
```

호스트 권한을 코드 내부에서 검사합니다. `MAP_ID`/`LOBBY_STATE=InGame` Update, 필요 시 StartSession, seamless `ServerTravel`, 예상 package와 `NM_ListenServer` 확인 순으로 완료됩니다. 후속 ServerTravel에는 `?listen`을 다시 붙이지 않아 기존 SteamSockets listener와 client connection을 유지합니다.

## 14. Leave

```cpp
Sessions->LeaveAndReturnToMenu();
```

필요 시 EndSession, DestroySession, 검색/초대/local state 정리, MainMenu map load 순으로 진행합니다. End가 실패해도 Destroy와 로컬 복구를 계속 시도합니다.

## 15. Project별 Extra Attribute

```cpp
FListenServerHostRequest Host;
Host.MaxPlayers = 4;
Host.GameModeId = TEXT("Coop");
Host.LobbyMap = FSoftObjectPath(TEXT("/Game/Maps/L_Lobby.L_Lobby"));
Host.SessionDisplayName = TEXT("Hard Korean Coop");
FListenServerSessionAttribute& Difficulty = Host.ExtraAdvertisedAttributes.AddDefaulted_GetRef();
Difficulty.Key = TEXT("Difficulty");
Difficulty.Value = TEXT("Hard");
Sessions->HostSession(Host);

FListenServerSearchRequest Search;
Search.MaxSearchResults = 50;
FListenServerSessionAttribute& Required = Search.RequiredAttributes.AddDefaulted_GetRef();
Required.Key = TEXT("Difficulty");
Required.Value = TEXT("Hard");
Sessions->FindSessions(Search);
```

비교는 문자열 동등 비교만 지원합니다. 예약 키 덮어쓰기, 중복 키, 64자를 넘는 키, 256자를 넘는 값은 거부됩니다.

## 16. 상태와 완료 delegate

세 상태 축은 독립적입니다.

- `EListenServerRole`: None, Host, Client
- `EListenServerConnectionState`: Offline, Lobby, InGame
- `EListenServerOperation`: 현재 진행 중인 일시적 비동기 작업

Blueprint/C++ 공통 multicast delegate는 다음 네 개뿐입니다.

- `OnStateChanged(Role, ConnectionState, Operation)`
- `OnOperationCompleted(CompletedOperation, Result)`
- `OnSearchResultsChanged()`
- `OnNetworkFailure(Result)`

`FListenServerOperationResult`의 `bSucceeded`, `bRecoverable`, `Error`, `UserMessage`를 사용자 흐름에 사용하고 `InternalError`는 개발 로그에만 표시하십시오.

## 17. 최소 Blueprint 연결

메뉴 Blueprint에서 `Get Game Instance Subsystem`으로 `ListenServerSessionSubsystem`을 얻고 다음 노드만 연결하면 됩니다.

- Host: `Host Default Session`
- Find: `Find Default Sessions`
- 각 항목: `Get Search Result By Index`, 이후 `Join Session`
- Quick Match: `Quick Match Default`
- 게임 시작: `Host Travel To Default Game Map`
- 나가기: `Leave And Return To Menu`
- 초대: `Show Invite UI`

버튼의 bool 반환값은 요청이 시작되었는지만 의미합니다. 최종 결과는 `OnOperationCompleted`에서 처리하십시오.

## 18. 설정 검증

```cpp
const FListenServerConfigurationReport Report = Sessions->ValidateConfiguration();
Sessions->LogConfigurationReport();
```

Steam OSS/Session/Identity/login, primary LocalPlayer, 단일 local user, ProjectKey, BuildUniqueId, limits, timeout, map package, Steam 및 SteamSockets plugin, DefaultPlatformService, SteamDevAppId, SteamSocketsNetDriver를 검사합니다. App ID 480은 오류가 아니라 경고입니다.

## 19. 진단 command

Development/DebugGame에서만 등록됩니다.

```text
LSN.Status
LSN.Validate
LSN.DumpSearchResults
LSN.DumpStructLayouts
```

여러 PIE World가 있으면 모든 Game/PIE World를 이름과 index로 나누어 출력합니다. 상태를 바꾸는 console command는 없습니다.

## 20. Steam 초대

호스트나 참가자는 `ShowInviteUI()`로 Steam Overlay 친구 초대 UI를 엽니다. 실행 중 `OnSessionInviteReceived`는 안전하게 기록하고, 사용자가 Steam UI에서 수락해 `OnSessionUserInviteAccepted`가 오면 ProjectKey/Build/open slot을 다시 검증합니다. 다른 작업 중이면 마지막으로 수락된 초대 하나만 보관하고, 안전한 idle 상태에서 기존 Named Session을 정리한 뒤 Join합니다. 동일 Session ID 초대는 무시합니다.

게임이 꺼진 상태의 초대 실행과 `+connect_lobby`는 지원하지 않습니다.

## 21. Timeout, 취소, stale callback

Create, Find, Join, Update, Start, End, Destroy, Travel은 작업 ID를 값으로 캡처한 delegate와 `FTSTicker` one-shot timeout을 사용합니다. 정상 완료·실패·취소·복구·Deinitialize에서 ticker와 delegate handle을 제거합니다. 취소된 Create 성공은 생성 세션을 Destroy하고, 취소된 Join 성공은 travel하지 않고 Named Session을 정리하며, 늦은 Find 결과는 폐기합니다.

`CancelCurrentOperation()`은 Find에서 `CancelFindSessions`를 호출합니다. Steam provider가 전송 취소를 지원하지 않거나 호출이 실패하면 결과를 로컬에서 즉시 무효화합니다. Create/Join/Update/Destroy는 취소 의사를 기록하고 backend callback 또는 timeout에서 정리합니다. 이미 전달된 map travel은 안전하게 되돌릴 수 없어 취소하지 않습니다.

OSSv1 완료 delegate 자체에는 요청 token이 없으므로, timeout 후 delegate가 제거된 뒤 provider가 같은 종류의 매우 늦은 성공을 내부적으로 만들면 이를 완벽히 귀속할 방법이 없습니다. 플러그인은 timeout 시 현재 Named Session을 즉시 정리하고 작업 ID로 관찰 가능한 stale callback을 차단하지만, provider가 callback 없이 뒤늦게 backend 상태만 바꾸는 극단적 상황은 다음 작업 전 `LSN.Status`와 Steam 테스트에서 확인해야 합니다.

## 22. Failure 복구

NetworkFailure, TravelFailure, SessionFailure 중 하나가 발생하면 현재 작업을 무효화하고 delegate/timeout을 정리한 뒤 Named Session Destroy와 MainMenu 복귀를 시도합니다. 복구 중 추가 failure는 같은 장애의 중복 이벤트로 간주해 사용자 broadcast를 억제합니다. 복구 후 Role=None, Connection=Offline, Operation=None이 되어 다시 Host/Find할 수 있습니다.

## 23. App ID 480 제한

480은 모든 개발자가 공유하는 Spacewar App ID입니다. Steam 자체가 검색을 현재 App ID로 제한하므로 C++에 480을 하드코딩하지 않습니다. 플러그인은 Steam 검색 요청에 ProjectKey와 광고된 BUILD_VERSION을 필수 조건으로 넣고, 요청된 GameMode, Region, Extra Attribute도 서버 측 조건으로 추가합니다. 따라서 unrelated lobby를 결과 상한 적용 전에 제외하며, 받은 결과에도 동일한 호환성 검사를 다시 적용합니다. Steam OSS가 관리하는 내부 BuildUniqueId와 플러그인의 BuildUniqueId 설정은 서로 다른 값이므로 직접 비교하지 않습니다.

## 24. 두 Steam 계정 수동 테스트

1. 서로 다른 두 PC 또는 완전히 분리된 두 Steam 환경에 서로 다른 계정으로 로그인합니다.
2. Steam client가 실행 중인지 확인하고 Development packaged build 또는 standalone game을 실행합니다.
3. A에서 `LSN.Validate` 후 Host, B에서 Find/Join을 수행합니다.
4. A의 Lobby→Game ServerTravel이 B에도 적용되는지 확인합니다.
5. B Leave, A Leave를 각각 확인합니다.
6. A가 비정상 종료될 때 B의 `OnNetworkFailure`와 MainMenu 복구를 확인합니다.
7. 다시 Host한 뒤 Overlay invite를 보내고 B가 실행 중 수락해 Join하는지 확인합니다.
8. 서로 다른 BuildUniqueId, ProjectKey, Region, Extra Attribute가 필터되는지 확인합니다.

PIE 한 프로세스의 복수 player와 하나의 Steam 계정으로 두 client를 띄우는 방식은 지원되지 않습니다.

## 25. 일반적인 오류

- `SteamUnavailable`: Steam client 실행, plugin 활성화, `DefaultPlatformService=Steam` 확인
- `NotLoggedIn`: LocalUserNum 0의 Steam 로그인과 standalone 실행 확인
- `InvalidRequest`/`TravelFailed`: 세 map asset 경로와 실제 package 존재 확인
- `NoSessionsFound`: ProjectKey/Build/GameMode/Region/Attribute, App ID 480 결과 상한 확인
- `IncompatibleBuild`: 양쪽 `BuildUniqueId` 일치 확인
- `ConnectStringFailed`: SteamSocketsNetDriver/NetConnection 설정과 방화벽 확인
- `AlreadyBusy`: 현재 Operation 완료 또는 `CancelCurrentOperation` 결과를 기다림
- 패키지에서 map load 실패: Packaging Cook 목록 확인

## 26. 메모리·복사 정책

Public 헤더에는 `FOnlineSessionSearchResult`, `FOnlineSessionSettings`, `IOnlineSessionPtr`, `FUniqueNetId` 또는 Steam SDK 타입이 없습니다. 원본 OSS 결과는 private cache 한 곳에서만 소유하고 외부에는 generation/index handle과 정제된 요약만 제공합니다. 검색 배열은 raw result 수만큼 `Reserve`하고 Steam 순서를 유지합니다. 큰 request는 `const&`, timeout/delegate에는 operation ID와 weak owner만 캡처합니다. UObject tick과 polling은 없습니다.

UE 5.7.4 Win64 Development Editor 측정값:

```text
FOperationContextPod              sizeof=24 alignof=8
FSearchCandidatePod               sizeof=16 alignof=4
FListenServerSearchResultHandle   sizeof=8  alignof=4
FListenServerSessionAttribute     sizeof=32 alignof=8
```

## 27. Automation Test

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Path\Project.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds='Automation RunTests ListenServerNetwork' -TestExit='Automation Test Queue Empty'
```

테스트는 상태 전환, operation ID/stale 판별, search generation, Project/Build/GameMode/Region/Attribute 호환성, 예약/중복 key, open slot, 검색 순서, 오류 mapping, 구조체 layout, 기본 설정을 검사합니다. 실제 Steam 연결은 mock하지 않습니다.

## 28. Build

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' MyProjectEditor Win64 Development -Project='C:\Path\MyProject.uproject' -WaitMutex -NoHotReloadFromIDE
```

## 29. 알려진 제한사항

- 실행 중 accepted invite만 자동 Join하며 cold-start invite는 처리하지 않습니다.
- OSSv1에는 application-level request token이 없어 위의 극단적 timeout/late-provider 상태 한계가 있습니다.
- Cook 포함 여부는 런타임 검증만으로 완전히 보장하지 않습니다.
- App ID 480 검색은 공유 결과 상한 때문에 자신의 Lobby를 놓칠 수 있습니다.
- 호스트 마이그레이션이 없으므로 호스트 종료 시 client는 MainMenu로 복구합니다.
- Client Join의 원격 map 경로는 신뢰하지 않으며 새 World의 client net mode로 travel 완료를 확인합니다.

## 30. 플러그인 없이 프로젝트가 직접 구현해야 했던 것

OSS interface 획득, session delegate 등록/해제, 모든 session CRUD, 검색 cache와 호환성 filter, connect string, ClientTravel/Listen OpenLevel/ServerTravel, map load 확인, timeout, cancel, stale callback 정책, failure 복구, invite 수락, settings 검증, logging과 diagnostics가 이 Runtime 모듈 안에 모여 있습니다. 프로젝트 측 코드는 메뉴 호출과 공통 완료 결과 처리만 담당합니다.
