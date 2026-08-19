#pragma once

class UObject;
class UCMBloodDefinition;

struct FCMBloodEvent;


/**
 * CMGore의 순간성 Blood VFX 실행 계층.
 *
 * Stateful Bleed / Pool은 담당하지 않는다.
 */
class FCMBloodVFXExecutor final
{
public:
	static bool ExecuteInstant(
		UObject* WorldContextObject,
		const FCMBloodEvent& Event,
		const UCMBloodDefinition& Definition
	);
};