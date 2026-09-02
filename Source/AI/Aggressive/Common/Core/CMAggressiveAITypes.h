#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

#include "CMAggressiveAITypes.generated.h"

/** 상위 행동은 코드가 제어하고 이동만 머신러닝 정책이 담당한다. */
UENUM(BlueprintType)
enum class ECMAggressiveAIState : uint8
{
    Searching,
    Chasing,
    Attacking,
    Waiting
};

/**
 * 몸통 로컬 공간의 평면 8방향이다.
 *
 * Unreal 로컬 +X는 전방이고 로컬 +Y는 오른쪽이다. 값은 시계 방향의
 * 45도 구역 순서이므로 머신러닝의 이산 관측값으로 바로 사용할 수 있다.
 */
UENUM(BlueprintType)
enum class ECMAggressiveMoveDirection : uint8
{
    Forward = 0,
    ForwardRight,
    Right,
    BackwardRight,
    Backward,
    BackwardLeft,
    Left,
    ForwardLeft,
    None = 255
};

/** 일정속도 전방위 Pawn의 NavMesh 경로 이동 종료 결과다. */
UENUM(BlueprintType)
enum class ECMAggressivePathMoveResult : uint8
{
    ReachedGoal,
    Failed,
    Cancelled
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCMAggressivePathMoveCompletedSignature, ECMAggressivePathMoveResult, Result);

/**
 * Pawn에 작성된 다리 정의 배열을 가리키는 안정적인 런타임 인덱스다.
 *
 * 고정된 다리 이름이나 개수를 핸들에 포함하지 않는다. 몸통 구성이 바뀌면
 * 다리 정의와 정책만 교체하고 공통 이동 API는 그대로 유지할 수 있다.
 */
USTRUCT(BlueprintType)
struct AI_API FCMAggressiveLegHandle
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aggressive AI")
    int32 Index = INDEX_NONE;

    /** 다리 인덱스의 유효 여부를 반환한다. */
    bool IsValid() const
    {
        return Index >= 0;
    }
};

/** 코드가 소유하고 머신러닝 이동 계층이 사용하는 내비게이션 목표다. */
USTRUCT(BlueprintType)
struct AI_API FCMAggressiveMovementGoal
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aggressive AI")
    FVector WorldLocation = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI")
    ECMAggressiveMoveDirection LocalDirection = ECMAggressiveMoveDirection::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aggressive AI", meta = (ClampMin = "0.0"))
    float AcceptanceRadius = 0.0f;
};

/** 고정 다리 구동기가 한 번의 다리 입력에 사용하는 물리 설정이다. */
USTRUCT(BlueprintType)
struct AI_API FCMAIFixedLegActuationSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aggressive AI", meta = (ClampMin = "0.0"))
    float ImpulseMagnitude = 5000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aggressive AI", meta = (ClampMin = "0.0"))
    float CooldownSeconds = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aggressive AI", meta = (ClampMin = "0.0"))
    float GroundCheckRadius = 12.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aggressive AI", meta = (ClampMin = "0.0"))
    float GroundContactDistance = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aggressive AI", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float MinimumGroundNormalZ = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aggressive AI")
    TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_Visibility;
};

/** 적용된 다리 임펄스를 학습 보상과 디버깅에서 재사용하기 위한 결과다. */
USTRUCT(BlueprintType)
struct AI_API FCMAIFixedLegActuationResult
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI")
    int32 LegIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI")
    FVector WorldImpulse = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI")
    FVector ApplicationLocation = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI")
    float YawAngularImpulse = 0.0f;
};

namespace CMAggressiveDirection
{
    /** 몸통 로컬 평면 벡터를 45도 간격의 8방향 중 하나로 변환한다. */
    AI_API ECMAggressiveMoveDirection QuantizeLocalDirection8(const FVector& LocalDirection, float MinimumPlanarMagnitude = KINDA_SMALL_NUMBER);

    /**
     * 월드 방향을 Yaw만 사용해 몸통 로컬 방향으로 바꾼 뒤 8방향으로 변환한다.
     * Pitch와 Roll은 평면 내비게이션 목표에 영향을 주지 않는다.
     */
    AI_API ECMAggressiveMoveDirection QuantizeWorldDirection8(const FVector& WorldDirection, const FQuat& BodyRotation, float MinimumPlanarMagnitude = KINDA_SMALL_NUMBER);

    /** 지정한 방향이 나타내는 정규화된 로컬 벡터를 반환한다. */
    AI_API FVector ToLocalUnitVector(ECMAggressiveMoveDirection Direction);

    /** 지정한 방향의 로그용 한글 이름을 반환한다. */
    AI_API const TCHAR* GetKoreanDisplayName(ECMAggressiveMoveDirection Direction);
} // namespace CMAggressiveDirection
