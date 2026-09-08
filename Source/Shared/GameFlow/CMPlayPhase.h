#pragma once

#include "CoreMinimal.h"

#include "CMPlayPhase.generated.h"

UENUM(BlueprintType)
// 플레이 진입부터 엔딩까지 상위 흐름 상태 구분
enum class ECMPlayPhase : uint8
{
    Loading,            // 스테이지 필수 에셋 로딩 및 초기화 중
    WaitingForPlayers,  // 참가 플레이어의 스테이지 준비 완료 대기
    Starting,           // 카운트다운과 시작 연출 진행
    Playing,            // 공용 몸통 조작과 부위 획득·장착을 포함한 실제 플레이 단계
    Completed,          // 현재 스테이지 클리어 후 결과 처리 및 전환 대기
    Failed,             // 현재 스테이지 실패 후 재시도 또는 포기 대기
    Victory,            // 마지막 스테이지 클리어로 게임 전체 승리 확정
    Defeat,             // 전체 패배 조건 충족으로 게임 종료 확정
    Ending              // 캐릭터 기록과 엔딩 크레딧 표시
};
