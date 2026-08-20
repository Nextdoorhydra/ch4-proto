#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CMBodyTableRow.generated.h"

USTRUCT(BlueprintType)
struct CHIMERA_API FCMBodyTableRow : public FTableRowBase
{
	GENERATED_BODY()

	/*
	 * CSV 한 행이 하나의 몸통 밸런스 프리셋이다.
	 * RowName은 FTableRowBase가 제공하는 DataTable의 키로 사용하므로
	 * 구조체에 중복 선언하지 않는다. 예: RowName=LineBody를 FindRow 키로 사용한다.
	 *
	 * 값의 최종 사용처:
	 * - MaxStamina / StaminaRegen: 키메라 공용 ASC
	 * - SegmentMaxHP: 각 몸통 마디의 독립 체력
	 * - SegmentMass ~ MaxVelocity: LineBody 물리 이동 설정
	 */

	// 외부 데이터나 GameplayTag와 연결할 때 사용할 고유 식별자다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ID = NAME_None;

	// 같은 Row Struct 안에서 Line/Ring/Star 등의 배치 방식을 구분한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName BodyType = NAME_None;

	// 각 몸통 마디가 따로 보유하는 체력의 최댓값이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SegmentMaxHP = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxStamina = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StaminaRegen = 0.0f;

	// 아래 값들은 몸통 타입의 게임 규칙이 아니라 Chaos 물리 튜닝 값이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SegmentMass = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GroundFriction = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LinearDamping = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AngularDamping = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxVelocity = 0.0f;

	// 모든 이동 파츠가 공유하는 몸통 기준 Impulse다. 파츠별 최종 힘은
	// 이 값에 MovementImpulseMultiplier를 곱해서 계산한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float BaseMovementImpulse = 0.0f;
};
