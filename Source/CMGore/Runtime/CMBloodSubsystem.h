#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "GameplayMessageRuntime/Public/GameFramework/GameplayMessageSubsystem.h"

#include "CMBloodSubsystem.generated.h"

class UCMBloodDefinition;
class UCMBloodDefinitionRegistry;

struct FCMBloodImpactMessage;
struct FCMBloodBurstMessage;
struct FCMBleedStateMessage;
struct FCMBloodPoolMessage;
struct FCMBloodEvent;


/**
 * CMGore Blood Runtime의 중앙 수신/정규화 Subsystem.
 *
 * 역할:
 * 1. GameplayMessageRouter의 Gore Message 구독
 * 2. 외부 Message를 FCMBloodEvent로 정규화
 * 3. 이후 Phase에서 Blood VFX 처리 계층으로 전달
 *
 * 하지 않는 역할:
 * - GAS 처리
 * - Dismemberment 판정
 * - Network Replication
 * - Niagara 직접 Spawn (Phase 2 기준)
 */
UCLASS()
class CMGORE_API UCMBloodSubsystem final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(
		FSubsystemCollectionBase& Collection
	) override;

	virtual void Deinitialize() override;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	virtual bool ShouldCreateSubsystem(
		UObject* Outer
	) const override;

	/** Registry/default fallback 정책을 공유하는 Definition lookup. */
	const UCMBloodDefinition* ResolveBloodDefinition(
		FName RequestedDefinitionId
	) const;

protected:
	virtual bool DoesSupportWorldType(
		const EWorldType::Type WorldType
	) const override;

private:
	void RegisterMessageListeners();
	void UnregisterMessageListeners();

	void HandleBloodImpactMessage(
		FGameplayTag Channel,
		const FCMBloodImpactMessage& Message
	);

	void HandleBloodBurstMessage(
		FGameplayTag Channel,
		const FCMBloodBurstMessage& Message
	);

	void HandleBleedStartMessage(
		FGameplayTag Channel,
		const FCMBleedStateMessage& Message
	);

	void HandleBleedStopMessage(
		FGameplayTag Channel,
		const FCMBleedStateMessage& Message
	);

	void HandlePoolStartMessage(
		FGameplayTag Channel,
		const FCMBloodPoolMessage& Message
	);

	void HandlePoolStopMessage(
		FGameplayTag Channel,
		const FCMBloodPoolMessage& Message
	);

	/**
	 * 모든 외부 Message가 최종적으로 도달하는 단일 진입점.
	 *
	 * Phase 2에서는 Log만 출력하며,
	 * Phase 3부터 VFX 계층으로 전달한다.
	 */
	void ProcessBloodEvent(
		const FCMBloodEvent& Event
	);
	
	void LoadDefinitionRegistry();

private:
	TArray<FGameplayMessageListenerHandle> MessageListenerHandles;

	UPROPERTY(Transient)
	TObjectPtr<UCMBloodDefinitionRegistry> LoadedDefinitionRegistry;

	UPROPERTY(Transient)
	TMap<
		FName,
		TObjectPtr<UCMBloodDefinition>
	> BloodDefinitions;

	FName DefaultDefinitionId = NAME_None;
};
