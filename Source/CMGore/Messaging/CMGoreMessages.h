#pragma once

#include "CoreMinimal.h"

#include "CMGoreMessages.generated.h"

/**
 * 순간적인 표면 충돌에 의해 발생하는 혈액 이벤트.
 *
 * 예:
 * - 총알 피격
 * - 칼날 타격
 * - 절단 순간 절단면 충격
 * - 벽/바닥에 신체 부위 충돌
 *
 * 이 메시지는 "어떤 시각 효과를 생성할지" 지정하지 않는다.
 * 실제 표현 결정은 CMGore 내부에서 수행한다.
 */
USTRUCT(BlueprintType)
struct CMGORE_API FCMBloodImpactMessage
{
	GENERATED_BODY()

	/** 혈액 이벤트의 원인이 된 객체. */
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UObject> Source = nullptr;

	/** 월드 좌표 기준 충돌 위치. */
	UPROPERTY(BlueprintReadWrite)
	FVector Location = FVector::ZeroVector;

	/** 충돌한 표면의 월드 노멀. */
	UPROPERTY(BlueprintReadWrite)
	FVector SurfaceNormal = FVector::UpVector;

	/** 혈액이 진행하려는 월드 방향. */
	UPROPERTY(BlueprintReadWrite)
	FVector Direction = FVector::ZeroVector;

	/**
	 * 효과 강도.
	 *
	 * 0~1로 제한하지 않는다.
	 * Definition이 최종 해석한다.
	 */
	UPROPERTY(BlueprintReadWrite)
	float Intensity = 1.0f;

	/**
	 * 사용할 Blood Definition 식별자.
	 *
	 * Phase 1에서는 메시지 계층과 DataAsset 계층의 결합을
	 * 피하기 위해 논리 ID만 전달한다.
	 */
	UPROPERTY(BlueprintReadWrite)
	FName BloodDefinitionId = NAME_None;
};


/**
 * 한 지점에서 순간적으로 많은 양의 혈액이 분출되는 이벤트.
 *
 * 예:
 * - 절단
 * - 참수
 * - 신체 파열
 */
USTRUCT(BlueprintType)
struct CMGORE_API FCMBloodBurstMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UObject> Source = nullptr;

	/** 분출 원점. */
	UPROPERTY(BlueprintReadWrite)
	FVector Location = FVector::ZeroVector;

	/** 주 분출 방향. */
	UPROPERTY(BlueprintReadWrite)
	FVector Direction = FVector::UpVector;

	/**
	 * 분출량의 논리적인 크기.
	 *
	 * 리터 등의 실제 단위가 아니라
	 * Definition에서 해석할 gameplay/VFX scalar.
	 */
	UPROPERTY(BlueprintReadWrite)
	float Amount = 1.0f;

	UPROPERTY(BlueprintReadWrite)
	FName BloodDefinitionId = NAME_None;
};


/**
 * 지속 출혈 Source의 상태 변경 메시지.
 *
 * Start / Stop 자체는 이 struct 안의 bool이 아니라
 * GameplayMessage 채널로 구분한다.
 *
 * CM.Message.Gore.Blood.Bleed.Start
 * CM.Message.Gore.Blood.Bleed.Stop
 */
USTRUCT(BlueprintType)
struct CMGORE_API FCMBleedStateMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UObject> Source = nullptr;

	/**
	 * 지속 출혈 기준 위치.
	 *
	 * 부착 대상이 존재하는 시스템에서는 송신자가
	 * 필요할 때 갱신해서 보낼 수 있다.
	 */
	UPROPERTY(BlueprintReadWrite)
	FVector Location = FVector::ZeroVector;

	/** 기본 방출 방향. */
	UPROPERTY(BlueprintReadWrite)
	FVector Direction = FVector::DownVector;

	/** 지속 출혈의 상대적인 세기. */
	UPROPERTY(BlueprintReadWrite)
	float Rate = 1.0f;

	UPROPERTY(BlueprintReadWrite)
	FName BloodDefinitionId = NAME_None;
};


/**
 * Blood Pool 생성원의 시작/정지 요청.
 *
 * Start / Stop은 GameplayMessage 채널로 구분한다.
 */
USTRUCT(BlueprintType)
struct CMGORE_API FCMBloodPoolMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UObject> Source = nullptr;

	UPROPERTY(BlueprintReadWrite)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite)
	FVector SurfaceNormal = FVector::UpVector;

	/** Pool에 공급되는 상대적인 혈액량. */
	UPROPERTY(BlueprintReadWrite)
	float Amount = 1.0f;

	UPROPERTY(BlueprintReadWrite)
	FName BloodDefinitionId = NAME_None;
};