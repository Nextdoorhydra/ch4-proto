# Vision 시스템 사용 설명서

이 문서는 Chimera의 시야 마스크, 시야 차단 충돌, 카메라 가림 페이드 기능을
설정하고 테스트하는 방법을 설명한다.

## 1. CameraOcclusionEditor 사용법

`CameraOcclusionEditor`는 Content Browser에서 머티리얼에 카메라 가림 페이드용
그래프를 추가하는 에디터 모듈이다. Unreal Engine 소스 수정 없이 프로젝트
모듈로 동작한다.

### 선택한 머티리얼 변환

1. Content Browser에서 변환할 머티리얼을 선택한다.
2. 우클릭하고 `Chimera Camera Occlusion` → `Convert Selected Materials`를
   선택한다.
3. 여러 머티리얼을 선택하면 선택된 머티리얼만 처리된다.

### 폴더 아래 머티리얼 일괄 변환

1. Content Browser에서 폴더를 우클릭한다.
2. `Chimera Camera Occlusion` → `Convert Materials Under Folder`를 선택한다.
3. 선택한 폴더와 하위 폴더의 머티리얼이 처리된다.

### 변환 후 반드시 할 작업

현재 변환기는 머티리얼 그래프와 설정을 변경하지만, 머티리얼 에디터의
`Apply`와 저장까지 자동으로 수행하지 않는다. 따라서 변환 직후 각 머티리얼을
열고 다음 작업을 해야 실제 배치된 액터에 반영된다.

1. 머티리얼 에디터에서 변환된 머티리얼을 연다.
2. 아무 설정이나 임시로 변경한 뒤 원래 값으로 되돌린다.
3. `Apply`를 누른다.
4. 머티리얼을 저장한다.

변환 그래프는 `Masked` 블렌드 모드, `Dither Opacity Mask = false`,
`Used With Static Lighting = false`를 전제로 한다. 기존 Opacity Mask가 있으면
변환 그래프에 유지된다.

## 2. 시야 차단 대상 설정

### 기본 동작

시야 차단은 기본적으로 `WorldStatic` 오브젝트 타입을 대상으로 한다.
따라서 새로 배치한 정적 벽이나 구조물은 별도 설정 없이 시야를 차단한다.

시야 차단으로 인식되려면 다음 조건도 충족해야 한다.

- 해당 컴포넌트의 Collision Object Type이 시야 설정의 `WorldStatic`에 포함되어야 한다.
- 시야 레이가 해당 컴포넌트를 Block해야 한다.
- 얇은 단면 메시라면 적절한 Simple Collision 또는 Complex Collision이 필요하다.

얇은 벽에서 가장자리 누수나 부분적으로 보이는 현상이 발생하면 메시의
Collision Geometry를 확인한다. 필요한 경우 Box Collision을 추가하거나
`Use Complex as Simple`을 사용하고, `Minimum Occluder Thickness`를 조정한다.

### 차단 대상에서 제외하기

액터 또는 컴포넌트에 `NoVisionOccluder` 태그를 추가하면 해당 대상은 시야
차단에서 제외된다. 현재 구현은 멀티 레이를 사용하므로 제외된 물체 뒤에
다른 유효한 벽이 있으면 그 벽을 계속 검사한다.

- 개별 액터: Actor Tags에 `NoVisionOccluder` 추가
- 블루프린트 전체: BP의 Class Defaults → Actor Tags에 추가
- 특정 컴포넌트: Component Tags에 추가

Static Mesh 에셋 자체에는 현재 이 태그를 직접 적용하지 않는다. 같은 메시의
모든 사용처를 제외하려면 해당 메시를 사용하는 블루프린트의 기본 Actor Tag나
컴포넌트 태그를 설정한다.

태그 이름은 `DA_CMVisionRenderConfig`의 `Vision Occluder Ignore Tag`에서
변경할 수 있다. 기본값은 `NoVisionOccluder`다.

## 3. DA_CMVisionRenderConfig

기본 데이터 에셋 경로:

`/Game/Chimera/Character/Part/Head/Vision/DA_CMVisionRenderConfig`

프로젝트 기본 설정은 `DefaultRenderConfig`에서 이 에셋을 사용한다. 값을
변경한 뒤 저장하면 시야 매니저가 사용하는 마스크와 충돌 판정에 반영된다.

### Rendering

| 옵션 | 역할 | 값을 높이면 |
| --- | --- | --- |
| `PostProcessMaterial` | 시야 마스크를 화면에 적용하는 포스트 프로세스 머티리얼 | 사용 머티리얼에 따라 결과가 달라짐 |
| `MaskDrawTexture` | 런타임 마스크 캔버스에 그리는 기본 텍스처 | 일반적으로 변경하지 않음 |
| `VisionTintColor` | 보이는 영역에 곱하는 색상 | 시야 영역의 색이 해당 색상에 가까워짐 |
| `VisionTintStrength` | 시야 색상 효과의 강도 | 시야 틴트가 강해짐 |
| `PostProcessBlendWeight` | 포스트 프로세스 적용 강도 | 시야 효과가 강해짐 |

### Mask

| 옵션 | 역할 | 값을 높이면 |
| --- | --- | --- |
| `MaskResolution` | 시야 마스크 텍스처 해상도 | 경계가 선명해지지만 GPU 비용 증가 |
| `ArcSegmentCount` | 원거리 시야 호를 구성하는 레이 수 | 원형 경계가 부드러워지지만 비용 증가 |
| `NearVisionCircleSegmentCount` | 벽에 의해 차단되는 근거리 원형 영역의 레이 수 | 근거리 경계 품질과 비용 증가 |
| `OcclusionEdgeRefinementSteps` | 차단 경계 주변 추가 이분 탐색 횟수 | 경계 정확도와 비용 증가 |
| `OcclusionEdgeRefinementDistance` | 인접 레이 간 거리 차이를 경계로 판단하는 기준 | 경계 보정이 발생하는 범위가 넓어짐 |
| `MaskUpdateInterval` | 마스크 갱신 간격(초) | 갱신 빈도가 낮아져 비용 감소, 반응성 저하 |
| `MinimumWorldHalfExtent` | 마스크가 커버하는 최소 월드 반경 | 기본 마스크 영역이 커짐 |
| `MaskBoundsPadding` | 마스크 가장자리 여백 | 경계 밖 UV 누수 방지 여유 증가 |
| `VisionEdgeSoftness` | 시야 마스크 경계의 월드 공간 부드러움 | 경계가 더 흐려짐 |

### Mask\|Height

| 옵션 | 역할 | 값을 높이면 |
| --- | --- | --- |
| `CeilingSurfaceNormalZThreshold` | 수평에 가까운 표면을 천장으로 판정하는 기준 | 더 많은 표면이 천장으로 판정될 수 있음 |
| `VisionHeightTolerance` | 눈높이 주변의 작은 높이 오차 허용량 | 높이 경계 깜빡임 감소, 판정이 느슨해짐 |
| `VisionBelowHeightAllowance` | 시야 기준점 아래의 저지대/낮은 장애물 처리에 사용하는 허용 높이 | 낮은 장애물의 상단을 더 넓게 고려 |

높이 관련 값은 천장과 낮은 장애물의 처리에 영향을 주므로, 특정 높이의
물체가 잘못 가려지는 경우 먼저 이 세 값을 확인한다.

### Occlusion

| 옵션 | 역할 | 값을 높이면 |
| --- | --- | --- |
| `OccluderObjectTypes` | 시야 차단에 사용할 충돌 오브젝트 타입 | 해당 타입의 물체가 차단 대상이 됨 |
| `OccluderSurfaceRevealDistance` | 벽 표면 뒤를 드러내는 추가 거리 | 벽 표면 주변이 더 많이 보임 |
| `MinimumOccluderThickness` | 얇은 메시를 안정적으로 차단하기 위한 최소 깊이 | 얇은 벽의 가장자리 누수 감소, 너무 높으면 벽 뒤가 과도하게 가려질 수 있음 |
| `VisionOccluderIgnoreTag` | 차단에서 제외할 Actor/Component Tag | 지정 태그를 가진 물체가 차단하지 않음 |
| `OccluderStencilValue` | 차단 대상 Custom Depth 스텐실 값 | 포스트 프로세스 머티리얼과 값이 다르면 효과가 깨질 수 있음 |
| `bTraceOcclusion` | 시야 차단 충돌 레이 활성화 여부 | `false`면 충돌 기반 차단을 하지 않음 |

`MinimumOccluderThickness`는 월드 단위 값이다. 얇은 벽의 충돌이 불안정할 때
값을 높여 안정성을 확보할 수 있지만, 먼저 Collision Geometry를 올바르게
구성하는 것이 좋다.

## 4. 권장 테스트 순서

1. 두꺼운 `WorldStatic` 벽으로 기본 시야 차단을 확인한다.
2. 얇은 벽은 Collision 설정과 `MinimumOccluderThickness`를 확인한다.
3. 차단하면 안 되는 액터/BP에 `NoVisionOccluder`를 추가한다.
4. 시야 마스크 경계가 거칠면 `MaskResolution`, 레이 수, 경계 보정 옵션을
   순서대로 조정한다.
5. 머티리얼 변환 후에는 머티리얼 에디터에서 `Apply`와 저장을 수행한다.

문제가 발생하면 먼저 로그에서 `bTraceOcclusion`, 사용 중인 Object Type,
충돌 컴포넌트, 그리고 적용된 `DA_CMVisionRenderConfig`를 확인한다.
