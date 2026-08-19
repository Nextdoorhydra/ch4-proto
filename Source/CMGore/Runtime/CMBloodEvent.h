#pragma once

#include "CoreMinimal.h"

/**
 * CMGore 내부에서 사용하는 정규화된 Blood Event 타입.
 *
 * 외부 모듈은 이 타입을 직접 생성하지 않는다.
 * 외부 입력은 FCMBlood*Message를 통해 들어오며,
 * UCMBloodSubsystem이 FCMBloodEvent로 변환한다.
 */
enum class ECMBloodEventType : uint8
{
	Impact,
	Burst,

	BleedStart,
	BleedStop,

	PoolStart,
	PoolStop
};


/**
 * CMGore 내부 표준 Blood Event.
 *
 * GameplayMessageRouter의 외부 메시지 형식과
 * 실제 VFX 처리 계층 사이의 중간 표현이다.
 */
struct FCMBloodEvent
{
	ECMBloodEventType Type = ECMBloodEventType::Impact;

	/**
	 * 이벤트를 발생시킨 객체.
	 *
	 * Event 자체가 Source의 수명을 소유하지 않는다.
	 */
	TWeakObjectPtr<UObject> Source;

	FVector Location = FVector::ZeroVector;

	/**
	 * 분사/이동 방향.
	 *
	 * Impact, Burst, Bleed 등에서 사용.
	 */
	FVector Direction = FVector::ZeroVector;

	/**
	 * 표면 Normal.
	 *
	 * Impact / Pool에서 주로 사용.
	 */
	FVector SurfaceNormal = FVector::UpVector;

	/**
	 * 이벤트의 정규화된 세기.
	 *
	 * 외부 Message의
	 * Intensity / Amount / Rate가 이 필드로 통합된다.
	 */
	float Magnitude = 0.0f;

	FName BloodDefinitionId = NAME_None;

	/**
	 * 해당 World에서 이벤트가 정규화된 시간.
	 */
	double WorldTimeSeconds = 0.0;
};