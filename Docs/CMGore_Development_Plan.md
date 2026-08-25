# CMGore 개발 현황 및 구현 계획

마지막 갱신: 2026-08-25  
기준 브랜치: `make-CMGore` → `develop` Pull Request

## 1. 문서 목적

이 문서는 CMGore의 실제 저장소 상태를 기준으로 다음 내용을 한곳에 정리한다.

- 현재까지 구현되고 검증된 기능
- Blood Pool에서 이동 물체로 피가 묻고 Stroke가 생성되는 Phase 5 확장
- 이후 Phase 6~8의 구현 순서와 완료 조건
- CMGore와 Chimera 게임 코드 사이의 책임 경계

초기 인수인계 문서보다 저장소의 코드와 검증 결과를 우선한다. 각 Phase는 빌드 성공만으로 완료 처리하지 않고 Runtime 또는 Editor 검증 결과를 함께 기록한다.

## 2. 현재 진행 상태

| Phase | 상태 | 구현 범위 | 검증 상태 |
|---|---|---|---|
| 0 | 완료 | `CMGore` Runtime 모듈과 의존성 기반 | 프로젝트 빌드 이력 확인 |
| 1 | 완료 | Blood Definition/Registry, Settings, Gameplay Message 계약 | 메시지 테스트 경로 구현 |
| 2 | 완료 | `UCMBloodSubsystem`의 Blood Event 라우팅과 Definition 해석 | 테스트 액터 경로 구현 |
| 3 | 완료 | Impact/Burst Niagara 실행 | CMGore 테스트 에셋 연동 |
| 4A | 완료 | Surface trace/sampling과 semantic `FCMBloodMark` registry | Surface smoke test 구현 |
| 4B | 완료 | Decal presentation과 수명 관리 | Render lifecycle 문제 수정 |
| 4C | 완료 | `ACMBloodDecalActor`, Blueprint material configuration, per-instance MID | Editor 검증 완료 |
| 4D | 완료 | Presentation Actor class별 lazy pool과 재사용 | Pool reuse/MID reset 검증 |
| 4E | 완료 | Surface subsystem을 Actor presentation으로 이관 | 기존 surface 경로 회귀 검증 |
| 5 | 완료 | Growing Blood Pool 생성·성장·정지·수명·페이드 | 사용자의 Editor 풀 검증 완료 |
| 5S | 완료 | Pool 접촉 물체의 제한 거리 Blood Stroke 생성 | 자동 Smoke PASS 및 테스트 에셋 확인 |
| 6 | 예정 | 캐릭터 발 오염, 좌우 Footprint, 오염량 감쇠와 연속 Blood Trail | 미구현 |
| 7 | 예정 | Tracking/query 계층과 generic highlight API | 미구현 |
| 8 | 예정 | Multiplayer 정책, 성능 계측/최적화, production cleanup | 미구현 |

`5S`는 독립적인 대형 Phase가 아니라 Phase 5의 상호작용 확장이다. Stroke용 semantic mark와 presentation 요청은 Phase 6에서도 재사용하지만, 이것만으로 Footprint/Blood Trail Phase 전체가 완료된 것은 아니다.

## 3. 현재 Runtime 구조

```text
Gameplay Message / Direct Request
        ↓
UCMBloodSubsystem
        ↓
UCMBloodDefinitionRegistry → UCMBloodDefinition
        ↓
Instant VFX 또는 Surface Request
        ↓
UCMBloodSurfaceSubsystem
        ├─ semantic FCMBloodMark registry
        ├─ lifetime / fade / eviction
        └─ class별 ACMBloodDecalActor pool
                     ↓
          Blueprint/C++ ConfigureMaterial
                     ↓
             per-instance MID / Decal
```

핵심 원칙은 semantic state와 renderer state의 분리다. `FCMBloodMark`는 Blood Definition, residue type, transform, source/surface reference 같은 조회 가능한 의미 정보만 소유한다. Material parameter 이름, MID 조작, TPBMaterial의 Growth 해석은 Presentation Actor/Blueprint 내부에 둔다.

## 4. 현재까지 구현된 주요 기능

### 4.1 데이터와 이벤트

- `UCMBloodSettings`가 기본 Definition Registry, surface/stroke 예산을 제공한다.
- `UCMBloodDefinitionRegistry`가 Blood Definition ID를 DataAsset으로 해석한다.
- `UCMBloodDefinition`이 Instant VFX, Surface, Pool, Stroke 표현을 한 종류의 혈액 정의로 묶는다.
- `UCMBloodSubsystem`이 Gameplay Message를 받아 Definition을 해석하고 VFX/Surface 실행기로 전달한다.

이 구조를 선택한 이유는 gameplay 발신자가 Niagara, Decal Material 또는 Presentation Class를 직접 알지 않게 하기 위해서다.

### 4.2 Surface Presentation

- `UCMBloodSurfaceSubsystem`이 trace 결과를 semantic `FCMBloodMark`로 등록한다.
- `ACMBloodDecalActor`가 Decal Component, Dynamic Material, Blueprint material configuration을 소유한다.
- Actor class별 pool이 Presentation 재사용을 담당하며, 재활성화할 때 MID를 새로 만들어 이전 인스턴스 파라미터가 남지 않게 한다.
- Splash, Pool, Stroke가 같은 Presentation foundation과 조회 API를 공유한다.

이 구조를 선택한 이유는 TPBMaterial 같은 복잡한 Blueprint 초기화 로직을 Subsystem에 하드코딩하지 않고, 향후 Footprint와 Highlight도 같은 수명/풀링 기반을 재사용하기 위해서다.

### 4.3 Growing Blood Pool

- `UCMBloodPoolSourceComponent`가 Source별 Pool 시작, 성장 진행률, 성장 정지, 제거를 관리한다.
- Pool 요청은 `ECMBloodResidueType::Pool`, Blood Definition ID, Source Actor를 semantic mark에 기록한다.
- `BP_CMBloodPool`은 정규화된 presentation progress를 TPBMaterial의 Growth 의미에 맞게 변환한다.
- Pool의 Material, 성장 시간, 수명, 페이드는 Blood Definition과 Presentation Blueprint에서 설정한다.

Source Component가 Material parameter를 직접 다루지 않는 이유는 같은 Pool gameplay 상태를 다른 shader/presentation에도 적용할 수 있게 하기 위해서다.

## 5. Phase 5 확장: Blood Pool Stroke

### 5.1 동작 흐름

```text
Physics Primitive가 Blood Pool에 접촉
        ↓
UCMBloodTransferComponent가 Pool mark 조회
        ↓
Blood Definition과 100~200cm 페인트 거리 예산 적재
        ↓
물체가 Pool 밖의 표면을 일정 속도 이상으로 이동
        ↓
이동 구간을 spacing 단위로 surface trace
        ↓
FCMBloodStrokeStampRequest 생성
        ↓
UCMBloodSurfaceSubsystem::SpawnStrokeStamp
        ↓
ECMBloodResidueType::Stroke + pooled decal presentation
```

### 5.2 추가된 코드와 이유

#### `Source/CMGore/Components/CMBloodTransferComponent.h/.cpp`

추가한 내용:

- `Dry → Loaded → Painting` 상태
- Pool의 실제 성장 크기를 고려한 접촉 판정
- Chaos `OnComponentHit`와 테스트용 `ProcessContactSample`의 공통 상태 머신
- 속도, 간격, 남은 페인트 거리, 오브젝트별 stamp 수 제한
- 이동 방향과 표면 normal을 이용한 Stroke stamp 요청

왜 추가했는가:

- “어떤 물체가 피를 묻혀 운반하는가”는 Pool Source나 Dismemberment의 책임이 아니기 때문이다.
- CMGore에 범용 Component로 두면 절단 파츠뿐 아니라 무기, 바퀴, 캐릭터 신발에도 같은 전달 규칙을 재사용할 수 있다.
- 직접 Decal을 생성하지 않고 Surface Subsystem에 요청해 기존 pool/lifetime/query 정책을 유지한다.

#### `Source/CMGore/Data/CMBloodDefinition.h`

추가한 내용:

- `FCMBloodStrokeDefinition`
- Stroke Material/Actor class, 전달 거리, 간격, 속도, 폭, 길이, 수명과 페이드 설정

왜 추가했는가:

- 혈액 종류마다 점도와 표현이 다를 수 있으므로 데이터에서 조정해야 한다.
- Transfer Component가 특정 Material asset이나 parameter 이름에 결합되지 않게 한다.

#### `Source/CMGore/Runtime/Surface/CMBloodSurfaceTypes.h`

추가한 내용:

- `ECMBloodResidueType::Stroke`
- Surface/Pool/Stroke 구분과 Blood Definition/Source reference
- `FCMBloodStrokeStampRequest`

왜 추가했는가:

- Tracking 단계에서 Pool과 Stroke를 구분해 조회할 수 있어야 한다.
- renderer 세부사항 없이 semantic 관계를 보존해야 한다.

#### `Source/CMGore/Runtime/Surface/CMBloodSurfaceSubsystem.h/.cpp`

추가한 내용:

- `SpawnStrokeStamp`
- 이동 tangent와 surface normal 기반 decal orientation
- Stroke별 수명/페이드와 presentation 생성
- 전체 Stroke 수 및 frame당 생성 수 예산
- mark 제거 시 Stroke 카운터 복구

왜 추가했는가:

- 모든 표면 혈흔이 동일한 등록, 제거, pooling, query 경로를 사용하게 하기 위해서다.
- 빠른 물체가 한 frame에 많은 stamp를 만들더라도 렌더링 비용이 폭증하지 않게 한다.

#### `Source/CMGore/Settings/CMBloodSettings.h`

추가한 내용:

- 전체 Stroke mark, 물체별 stamp, frame별 stamp, 최소 spacing, camera cull distance 예산

왜 추가했는가:

- 하드코딩된 성능 제한 대신 Project Settings에서 플랫폼별로 조정할 수 있게 한다.

#### `Source/Chimera/Gore/CMDismembermentComponent.h/.cpp`

변경한 내용:

- 절단 파츠에 `UCMBloodTransferComponent`를 런타임으로 부착한다.
- Dismemberment 내부의 단순 hit-decal trail 상태와 handler를 제거한다.

왜 변경했는가:

- Dismemberment는 “절단 파츠를 만든다”까지만 담당하고, 피 전달 규칙은 CMGore가 담당하도록 책임을 분리한다.
- 새 Component가 기존의 제한 없는 단발 decal 방식 대신 Pool 접촉 여부와 전달 거리 예산을 적용한다.

#### 테스트와 에셋

- `ACMDismembermentTestHarness`가 절단된 왼팔을 Pool에서 Stroke 방향으로 이동시키고 mark 수와 도색 거리를 검증한다.
- `MI_CMBloodStroke_HumanRed`와 `DA_CMBlood_HumanRed`의 Stroke 설정을 테스트 경로에 연결했다.
- `CMGoreLevel`에서 시각 검증할 수 있다.

테스트 코드는 production actor가 아니라 명시적인 Harness에만 둔다. 자동 테스트 플래그는 현재 구현 당시의 이름인 `-CMGorePhase6SmokeTest`를 유지하고 있으나, 이 문서에서는 Phase 5 확장 검증으로 분류한다. 이후 테스트 명칭 정리 시 기존 자동화 호환을 위해 alias 기간을 둔다.

## 6. 검증 현황

### Phase 5 Pool

- Editor에서 생성, 성장, 유지, 소멸 표현 검증 완료.
- Presentation progress와 TPBMaterial Growth 방향 변환 검증 완료.

### Phase 5 Stroke 확장

- 자동 실행 명령:

```text
UnrealEditor-Cmd.exe C:\Chimera\Chimera.uproject /Game/CMGore/CMGoreLevel -game -CMGorePhase6SmokeTest -unattended -nop4 -nosplash -nullrhi -nosound -stdout -FullStdOutLogOutput
```

- 확인된 성공 로그 예:

```text
[DismembermentTest] Blood Pool Stroke visual test: PASS | Budget=136.7cm Stamps=17 Painted=136.0cm StrokeMarks=17
```

- 검증 항목: Pool mark 탐색, 전달 거리 적재, Stroke stamp 생성, 도색 거리 제한, semantic Stroke mark 등록.
- 남은 수동 검증: 실제 Chaos 물리 이동에서의 접촉 안정성, 경사/벽 경계, Shipping 성능 예산.

## 7. 수정된 이후 구현 계획

### Phase 6 — Footprints / Character Blood Trail

Stroke foundation은 이미 있으므로 새 renderer나 별도 decal pool을 만들지 않는다.

1. `UCMBloodFootprintComponent`의 contamination 상태를 구현한다.
   - Blood amount, Definition ID, 좌/우 발, 마지막 step, 최소 간격, 감쇠를 관리한다.
2. 발별 surface contact를 sampling한다.
   - Character movement/animation event의 구체적인 연결점은 게임 코드에서 제공하고 CMGore는 입력 API를 유지한다.
3. Footprint request를 `UCMBloodSurfaceSubsystem`에 추가한다.
   - 기존 Presentation Actor pool, lifetime, query를 재사용한다.
4. contamination 감소를 구현한다.
   - 걸음마다 footprint 크기/opacity가 감소하고 0이 되면 Dry 상태로 전환한다.
5. 기존 `UCMBloodTransferComponent`와 역할을 정리한다.
   - Physics object의 연속 Stroke는 유지한다.
   - 신발/발의 연속 끌림이 필요하면 동일 Component 또는 공통 request만 재사용하고 상태를 억지로 합치지 않는다.
6. Build, automated smoke, Editor visual regression을 순서대로 통과한다.

완료 조건:

- 좌/우 발자국 방향과 간격이 올바르다.
- Pool 접촉 전에는 생성되지 않고, 접촉 후 오염량만큼만 생성된다.
- 오염량이 감소하며 Blood Definition별 표현이 유지된다.
- Splash/Pool/Stroke 기존 경로에 회귀가 없다.

### Phase 7 — Tracking / Highlight Infrastructure

1. 기존 `FindBloodMark`와 `GetBloodMarksInRadius` 위에 목적별 query API를 추가한다.
2. residue type, source/surface actor, Definition ID로 필터링한다.
3. Presentation에 `SetHighlighted(bool)` 같은 generic contract를 추가한다.
4. 실제 Material parameter 처리는 Presentation Blueprint 내부에 둔다.
5. 이 Phase에서는 제6감 gameplay나 UI를 구현하지 않는다.

완료 조건:

- Pool, Stroke, Footprint를 semantic 정보로 조회할 수 있다.
- Highlight on/off가 pooled actor 재사용 후 남지 않는다.
- Tracking code가 MID나 parameter 이름을 직접 알지 않는다.

### Phase 8 — Multiplayer / Performance / Production Cleanup

1. cosmetic presentation과 gameplay에 필요한 semantic state의 replication 정책을 분리한다.
2. Dedicated Server에서 Decal/MID/Niagara 생성을 차단한다.
3. Actor spawn/reuse, MID allocation, active decals, DBuffer, Niagara, surface trace를 계측한다.
4. 계측 결과가 필요성을 보일 때만 prewarm, class별 retained count, soft class loading을 도입한다.
5. 테스트 플래그 명칭, 임시 에셋 위치, test-only 기본값과 문서를 production 기준으로 정리한다.

완료 조건:

- 네트워크 역할별 생성 정책이 문서화되고 검증된다.
- 예산 초과 시 eviction/culling이 안전하게 동작한다.
- test-only 코드와 asset이 production 경로와 명확히 구분된다.

## 8. 변경 시 유지할 책임 경계

- `CMGore`: 혈액 데이터, 이벤트, surface mark, presentation, pool, transfer, footprint, generic tracking 계약.
- `Chimera/Gore`: 캐릭터 절단 시점, 절단 파츠 생성, 게임 고유 이벤트와 CMGore Component 연결.
- Presentation Actor/Blueprint: Material, MID, TPBMaterial parameter와 shader별 변환.
- Source/Transfer/Footprint Component: gameplay/runtime 상태와 generic request 생성.
- Surface Subsystem: semantic registry, trace 결과, lifetime, pooling, query, 전역 예산.

새 기능이 특정 Material parameter 이름을 Surface Subsystem 또는 gameplay code에 추가한다면 이 경계를 위반한 것으로 간주한다.

## 9. Phase 작업 보고 규칙

각 Phase 구현 후 다음을 함께 보고한다.

- 변경 파일과 새 public API
- 파일별로 무엇이 바뀌었는지
- 해당 책임을 그 타입에 둔 이유
- Before/After runtime 흐름
- 의도적으로 변경하지 않은 범위
- Build/Runtime/Regression 테스트 방법과 결과
- 실패 시 확인 항목
- 다음 Phase의 정확한 범위
