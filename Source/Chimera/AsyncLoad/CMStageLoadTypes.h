#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "CMStageLoadTypes.generated.h"

UENUM(BlueprintType)
// 로드 그룹의 에셋을 언제 해제할 수 있는지 구분
enum class ECMLoadRetentionPolicy : uint8
{
	KeepUntilStageEnd,          // 재방문 가능하거나 공용인 그룹을 스테이지 종료까지 유지
	ReleaseWhenBranchRejected, // 되돌릴 수 없는 분기에서 선택되지 않았을 때 해제
	ReleaseAfterExit           // 재진입할 수 없는 구역을 완전히 빠져나간 뒤 해제
};

UENUM(BlueprintType)
// 로컬 머신에서 처리 중인 로드 그룹 상태 구분
enum class ECMStageLoadGroupState : uint8
{
	NotRequested,  // 아직 요청하지 않음
	Loading,       // AsyncPDALoader가 로드 중
	Ready,         // 요청한 PDA가 모두 준비됨
	Failed,        // 일부 또는 전체 로드 실패
	PendingRelease,// 로드 완료 직후 해제하도록 예약됨
	Released       // 로드하지 않았거나 이미 해제됨
};

// 하나의 스테이지에서 함께 로드할 콘텐츠 묶음과 실행 순서 정의
UENUM(BlueprintType)
// 스테이지 스케줄에서 그룹이 언제 요청되는지 구분
enum class ECMStageLoadPolicy : uint8
{
	BeforeStageStart, // 플레이 시작 전에 반드시 준비
	Sequential,      // 시작 후 LoadOrder 순서로 백그라운드 준비
	OnDemand         // 분기나 기믹이 명시적으로 요청할 때만 준비
};

USTRUCT(BlueprintType)
struct CHIMERA_API FCMStageLoadGroupDefinition
{
	GENERATED_BODY()

	// CSV와 레벨 기믹이 참조하는 사람이 읽을 수 있는 안정적인 식별자
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Load")
	FName LoadGroupId;

	// LoadGroupId 정렬 결과로 자동 생성되는 AsyncPDALoader 내부 조회 키
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Load")
	int32 Scope = INDEX_NONE;

	// 같은 TimingTag 안에서 작은 값부터 순차 요청. 중간 삽입을 위해 10 단위 사용 권장
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Load")
	int32 LoadOrder = 0;

	// Session, Stage.Entry, Stage.Background, Stage.Result, Ending 중 준비 마감 시점
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Load")
	ECMStageLoadPolicy LoadPolicy = ECMStageLoadPolicy::Sequential;

	// AssetManager 요청 우선순위. Entry는 높게, Background는 낮게 설정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Load")
	int32 LoadPriority = 0;

	// PDA 내부 soft reference 중 이번 요청에서 함께 준비할 Asset Bundle
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Load")
	TArray<FName> AssetBundles = { TEXT("Gameplay") };

	// 분기 탈락이나 구역 이탈 이후의 해제 정책
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Load")
	ECMLoadRetentionPolicy RetentionPolicy = ECMLoadRetentionPolicy::KeepUntilStageEnd;
};
