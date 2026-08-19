#pragma once

#include "NativeGameplayTags.h"

namespace CMStageLoadTags
{
	// 캠페인 전체에서 유지할 공통 데이터 로드 시점
	CHIMERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Session);

	// 로딩 화면을 닫기 전에 반드시 끝나야 하는 현재 스테이지 진입 데이터 로드 시점
	CHIMERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stage);

	// 플레이 시작 후 LoadOrder 순서로 미리 준비하는 스테이지 데이터 로드 시점

	// 현재 스테이지의 클리어·실패 결과 연출 데이터 로드 시점

	// 최종 캐릭터 기록과 엔딩 크레딧 데이터 로드 시점
	CHIMERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ending);
}
