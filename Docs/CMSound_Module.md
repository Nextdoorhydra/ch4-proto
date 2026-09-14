# CMSound 모듈과 플레이 상태 이벤트

## 의존성

- `Chimera → Shared`
- `CMSound → Shared, NKMSoundRuntime, AsyncPDALoader, GameplayMessageRuntime`
- Chimera와 CMSound는 서로의 클래스나 모듈을 참조하지 않는다.
- UI의 기존 볼륨 설정은 NKMSoundRuntime을 계속 사용한다.

`Source/Chimera/Sound`의 파일을 `Source/CMSound/Sound`로 이동했다. 공개 심벌은 CMSOUND_API로 변경했고, 기존 Sound/... include 경로와 콘텐츠 에셋 경로는 유지했다.

## 이벤트 흐름

1. CMSound의 CMStageAudioDirectorSubsystem이 월드 시작 시 `Chimera.Message.PlayState.Changed`를 구독한다.
2. 구독 직후 `Chimera.Message.PlayState.RequestCurrent`를 보낸다.
3. 이미 BeginPlay를 마친 CMPlayGameState는 요청에 응답해 현재 상태를 Changed 이벤트로 보낸다.
4. 아직 GameState가 시작 전이라 요청을 못 받았으면, GameState의 BeginPlay에서 요청 구독을 등록하고 현재 상태를 보낸다.
5. 이후 Phase나 스테이지 인덱스가 바뀌면 기존 OnRep_PlayState에서 Changed 이벤트를 보낸다.

구독 후 요청하기 때문에 응답을 놓치지 않는다. 양쪽 시작 순서를 고정할 필요가 없고, Tick이나 재시도 타이머도 필요 없다. 현재 상태 메시지라 초기화 중 중복으로 받아도 기존 PlayBGM의 같은 태그 중복 방지 정책을 사용할 수 있다.

메시지는 현재 World, 플레이 Phase, 스테이지 인덱스, 해당 스테이지의 BGM 태그를 담는다. GameState는 데이터를 제공하며 재생/정지 판단은 하지 않는다. CMSound가 Starting/Playing에서 재생, 완료/실패/엔딩/로딩에서 정지, WaitingForPlayers에서 유지를 결정한다.

## 수명과 네트워크

GameplayMessageRouter는 로컬 GameInstance에서 동작한다. 메시지 자체가 서버에서 클라이언트로 복제되는 것은 아니다. 기존 GameState 프로퍼티 복제가 각 클라이언트의 OnRep를 호출하고, 각 머신에서 로컬 이벤트를 발생시킨다.

요청과 상태에 World를 포함해 맵 전환 중 다른 월드의 메시지를 무시한다. GameState는 EndPlay에서, 사운드 디렉터는 Deinitialize에서 구독을 해제한다. 전용 서버는 오디오용 상태 요청 구독과 디렉터 구독을 생략한다.

## 코드를 읽는 순서

1. `Source/Shared/GameFlow/CMPlayPhase.h`: 공유하는 플레이 단계 enum.
2. `Source/Shared/GameFlow/CMPlayStateMessages.h`: 상태와 현재 상태 요청 메시지 계약.
3. `Source/Chimera/GameMode/Play/CMPlayGameState.cpp`: BeginPlay, HandlePlayStateRequest, BroadcastPlayState, OnRep_PlayState.
4. `Source/CMSound/Sound/CMStageAudioDirectorSubsystem.cpp`: 구독 후 요청, 이벤트별 BGM 재생 정책.
5. `Source/CMSound/Sound/CMGameSoundBridgeSubsystem.cpp`: 로드된 사운드 카탈로그를 재생 플러그인에 등록.

GetStageAudioState와 ICMStageAudioSource 인터페이스는 제거했다.

## 에셋 호환성

DefaultEngine.ini에 이동한 세 사운드 클래스의 Core Redirect와 Shared로 옮긴 ECMPlayPhase의 Enum Redirect를 등록했다. ECMLobbyPhase는 기존 위치에 남아 있다. DefaultGame.ini의 사운드 카탈로그 스캔 클래스는 /Script/CMSound.CMGameSoundDataAsset이다.

기존 CMGameFlowTypes.h는 공유 enum 헤더를 포함하므로 기존 include 사용처를 바꿀 필요가 없다. 콘텐츠 경로, PrimaryAssetType(NKMSoundDataAsset), 기존 사운드 태그는 유지했다.

## 검증

자동화 테스트 `Chimera.Sound.PlayStateEvents`는 GameState보다 먼저 구독하는 경우, 늦게 구독하고 현재 상태를 요청하는 경우, 이후 상태 변경, 다른 월드 요청 무시, EndPlay 후 구독 해제를 검증한다. 실제 BGM 출력과 호스트/클라이언트 청취는 PIE에서 확인한다.

## 스테이지 BGM 복제

서버 GameMode의 InitializeStageProgress가 경로에서 선택한 BGM 태그를 GameState의 CurrentStageBGMTag에 저장한다. 이 값은 OnRep_PlayState로 복제되며, BroadcastPlayState는 로컬 RouteSubsystem 대신 이 값을 보낸다. 따라서 클라이언트나 늦게 합류한 플레이어도 서버와 같은 음악 태그를 받는다.

DA_CMStageRouteDefinition의 두 스테이지에는 기존 Stage01 BGM 태그를 연결했다. Stage1의 로딩 스케줄에도 Stage.Entry.Sound를 추가했으며 Gameplay 번들로 시작 전에 준비한다.

모든 효과음을 GameState에 추가하지 않는다. 전역 BGM은 GameState 상태, 문/장애물의 일회성 효과음은 해당 액터의 동작 이벤트, 켜짐/꺼짐이 유지되는 환경 루프는 환경 액터의 복제 상태에서 로컬 재생한다. 정적으로 항상 켜진 환경음은 각 머신의 레벨에서 재생할 수 있다. 새 음원마다 복제 프로퍼티를 추가하는 대신 기존 태그/상태 필드에 데이터를 지정한다.