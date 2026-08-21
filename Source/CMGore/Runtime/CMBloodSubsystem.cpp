#include "CMBloodSubsystem.h"

#include "CMBloodEvent.h"
#include "Components/CMBloodPoolSourceComponent.h"
#include "Tags/CMGoreGameplayTags.h"
#include "Messaging/CMGoreMessages.h"
#include "Data/CMBloodDefinition.h"
#include "Data/CMBloodDefinitionRegistry.h"
#include "Settings/CMBloodSettings.h"
#include "VFX/CMBloodVFXExecutor.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include "GameplayMessageRuntime/Public/GameFramework/GameplayMessageSubsystem.h"


DEFINE_LOG_CATEGORY_STATIC(LogCMBloodSubsystem, Log, All);


namespace CMBloodSubsystemPrivate
{
	FVector NormalizeOrFallback(
		const FVector& Value,
		const FVector& Fallback
	)
	{
		if (Value.IsNearlyZero())
		{
			return Fallback;
		}

		return Value.GetSafeNormal();
	}

	float SanitizeMagnitude(const float Value)
	{
		return FMath::Max(0.0f, Value);
	}

	const TCHAR* LexToString(const ECMBloodEventType Type)
	{
		switch (Type)
		{
		case ECMBloodEventType::Impact:
			return TEXT("Impact");

		case ECMBloodEventType::Burst:
			return TEXT("Burst");

		case ECMBloodEventType::BleedStart:
			return TEXT("BleedStart");

		case ECMBloodEventType::BleedStop:
			return TEXT("BleedStop");

		case ECMBloodEventType::PoolStart:
			return TEXT("PoolStart");

		case ECMBloodEventType::PoolStop:
			return TEXT("PoolStop");

		default:
			return TEXT("Unknown");
		}
	}

	UCMBloodPoolSourceComponent* ResolvePoolSource(UObject* Source)
	{
		if (UCMBloodPoolSourceComponent* PoolSource =
			Cast<UCMBloodPoolSourceComponent>(Source))
		{
			return PoolSource;
		}

		AActor* SourceActor = Cast<AActor>(Source);

		if (!SourceActor)
		{
			if (const UActorComponent* SourceComponent =
				Cast<UActorComponent>(Source))
			{
				SourceActor = SourceComponent->GetOwner();
			}
		}

		return SourceActor
			? SourceActor->FindComponentByClass<UCMBloodPoolSourceComponent>()
			: nullptr;
	}
}


bool UCMBloodSubsystem::ShouldCreateSubsystem(
	UObject* Outer
) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	const UWorld* World = Cast<UWorld>(Outer);

	if (!World)
	{
		return false;
	}

	// CMGore는 Presentation 시스템이다.
	// Dedicated Server에는 생성할 필요가 없다.
	return World->GetNetMode() != NM_DedicatedServer;
}


bool UCMBloodSubsystem::DoesSupportWorldType(
	const EWorldType::Type WorldType
) const
{
	switch (WorldType)
	{
	case EWorldType::Game:
	case EWorldType::PIE:
	case EWorldType::GamePreview:
		return true;

	default:
		return false;
	}
}


void UCMBloodSubsystem::Initialize(
	FSubsystemCollectionBase& Collection
)
{
	Super::Initialize(Collection);

	LoadDefinitionRegistry();

	UE_LOG(
		LogCMBloodSubsystem,
		Log,
		TEXT("UCMBloodSubsystem initialized.")
	);
}

void UCMBloodSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	RegisterMessageListeners();
}

void UCMBloodSubsystem::Deinitialize()
{
	UnregisterMessageListeners();

	BloodDefinitions.Reset();
	LoadedDefinitionRegistry = nullptr;
	DefaultDefinitionId = NAME_None;

	UE_LOG(
		LogCMBloodSubsystem,
		Log,
		TEXT("UCMBloodSubsystem deinitialized.")
	);

	Super::Deinitialize();
}


void UCMBloodSubsystem::RegisterMessageListeners()
{
	if (!MessageListenerHandles.IsEmpty())
	{
		return;
	}

	if (!UGameplayMessageSubsystem::HasInstance(this))
	{
		UE_LOG(
			LogCMBloodSubsystem,
			Warning,
			TEXT("Gameplay Message Subsystem is unavailable at World BeginPlay.")
		);
		return;
	}

	UGameplayMessageSubsystem& MessageSubsystem =
		UGameplayMessageSubsystem::Get(this);

	MessageListenerHandles.Reserve(6);

	MessageListenerHandles.Add(
		MessageSubsystem.RegisterListener<FCMBloodImpactMessage>(
			CMGoreGameplayTags::Message::Blood::Impact,
			this,
			&ThisClass::HandleBloodImpactMessage
		)
	);

	MessageListenerHandles.Add(
		MessageSubsystem.RegisterListener<FCMBloodBurstMessage>(
			CMGoreGameplayTags::Message::Blood::Burst,
			this,
			&ThisClass::HandleBloodBurstMessage
		)
	);

	MessageListenerHandles.Add(
		MessageSubsystem.RegisterListener<FCMBleedStateMessage>(
			CMGoreGameplayTags::Message::Blood::Bleed::Start,
			this,
			&ThisClass::HandleBleedStartMessage
		)
	);

	MessageListenerHandles.Add(
		MessageSubsystem.RegisterListener<FCMBleedStateMessage>(
			CMGoreGameplayTags::Message::Blood::Bleed::Stop,
			this,
			&ThisClass::HandleBleedStopMessage
		)
	);

	MessageListenerHandles.Add(
		MessageSubsystem.RegisterListener<FCMBloodPoolMessage>(
			CMGoreGameplayTags::Message::Blood::Pool::Start,
			this,
			&ThisClass::HandlePoolStartMessage
		)
	);

	MessageListenerHandles.Add(
		MessageSubsystem.RegisterListener<FCMBloodPoolMessage>(
			CMGoreGameplayTags::Message::Blood::Pool::Stop,
			this,
			&ThisClass::HandlePoolStopMessage
		)
	);

	UE_LOG(
		LogCMBloodSubsystem,
		Log,
		TEXT("Registered %d CMGore message listeners."),
		MessageListenerHandles.Num()
	);
}


void UCMBloodSubsystem::UnregisterMessageListeners()
{
	for (FGameplayMessageListenerHandle& Handle : MessageListenerHandles)
	{
		Handle.Unregister();
	}

	MessageListenerHandles.Reset();
}

void UCMBloodSubsystem::HandleBloodImpactMessage(
	FGameplayTag Channel,
	const FCMBloodImpactMessage& Message
)
{
	FCMBloodEvent Event;

	Event.Type = ECMBloodEventType::Impact;

	Event.Source = Message.Source.Get();

	Event.Location = Message.Location;

	Event.SurfaceNormal =
		CMBloodSubsystemPrivate::NormalizeOrFallback(
			Message.SurfaceNormal,
			FVector::UpVector
		);

	Event.Direction =
		CMBloodSubsystemPrivate::NormalizeOrFallback(
			Message.Direction,
			FVector::ZeroVector
		);

	Event.Magnitude =
		CMBloodSubsystemPrivate::SanitizeMagnitude(
			Message.Intensity
		);

	Event.BloodDefinitionId =
		Message.BloodDefinitionId;

	if (const UWorld* World = GetWorld())
	{
		Event.WorldTimeSeconds = World->GetTimeSeconds();
	}

	ProcessBloodEvent(Event);
}

void UCMBloodSubsystem::HandleBloodBurstMessage(
	FGameplayTag Channel,
	const FCMBloodBurstMessage& Message
)
{
	FCMBloodEvent Event;

	Event.Type = ECMBloodEventType::Burst;

	Event.Source = Message.Source.Get();

	Event.Location = Message.Location;

	Event.Direction =
		CMBloodSubsystemPrivate::NormalizeOrFallback(
			Message.Direction,
			FVector::UpVector
		);

	Event.Magnitude =
		CMBloodSubsystemPrivate::SanitizeMagnitude(
			Message.Amount
		);

	Event.BloodDefinitionId =
		Message.BloodDefinitionId;

	if (const UWorld* World = GetWorld())
	{
		Event.WorldTimeSeconds = World->GetTimeSeconds();
	}

	ProcessBloodEvent(Event);
}

void UCMBloodSubsystem::HandleBleedStartMessage(
	FGameplayTag Channel,
	const FCMBleedStateMessage& Message
)
{
	if (!IsValid(Message.Source.Get()))
	{
		UE_LOG(
			LogCMBloodSubsystem,
			Warning,
			TEXT(
				"Ignored BleedStart message: "
				"stateful blood source requires a valid Source."
			)
		);

		return;
	}

	FCMBloodEvent Event;

	Event.Type = ECMBloodEventType::BleedStart;

	Event.Source = Message.Source.Get();

	Event.Location = Message.Location;

	Event.Direction =
		CMBloodSubsystemPrivate::NormalizeOrFallback(
			Message.Direction,
			FVector::DownVector
		);

	Event.Magnitude =
		CMBloodSubsystemPrivate::SanitizeMagnitude(
			Message.Rate
		);

	Event.BloodDefinitionId =
		Message.BloodDefinitionId;

	if (const UWorld* World = GetWorld())
	{
		Event.WorldTimeSeconds = World->GetTimeSeconds();
	}

	ProcessBloodEvent(Event);
}

void UCMBloodSubsystem::HandleBleedStopMessage(
	FGameplayTag Channel,
	const FCMBleedStateMessage& Message
)
{
	if (!IsValid(Message.Source.Get()))
	{
		UE_LOG(
			LogCMBloodSubsystem,
			Warning,
			TEXT(
				"Ignored BleedStop message: "
				"stateful blood source requires a valid Source."
			)
		);

		return;
	}

	FCMBloodEvent Event;

	Event.Type = ECMBloodEventType::BleedStop;

	Event.Source = Message.Source.Get();

	Event.Location = Message.Location;

	Event.Direction =
		CMBloodSubsystemPrivate::NormalizeOrFallback(
			Message.Direction,
			FVector::DownVector
		);

	Event.Magnitude =
		CMBloodSubsystemPrivate::SanitizeMagnitude(
			Message.Rate
		);

	Event.BloodDefinitionId =
		Message.BloodDefinitionId;

	if (const UWorld* World = GetWorld())
	{
		Event.WorldTimeSeconds = World->GetTimeSeconds();
	}

	ProcessBloodEvent(Event);
}

void UCMBloodSubsystem::HandlePoolStartMessage(
	FGameplayTag Channel,
	const FCMBloodPoolMessage& Message
)
{
	if (!IsValid(Message.Source.Get()))
	{
		UE_LOG(
			LogCMBloodSubsystem,
			Warning,
			TEXT(
				"Ignored PoolStart message: "
				"stateful blood source requires a valid Source."
			)
		);

		return;
	}

	FCMBloodEvent Event;

	Event.Type = ECMBloodEventType::PoolStart;

	Event.Source = Message.Source.Get();

	Event.Location = Message.Location;

	Event.SurfaceNormal =
		CMBloodSubsystemPrivate::NormalizeOrFallback(
			Message.SurfaceNormal,
			FVector::UpVector
		);

	Event.Magnitude =
		CMBloodSubsystemPrivate::SanitizeMagnitude(
			Message.Amount
		);

	Event.BloodDefinitionId =
		Message.BloodDefinitionId;

	if (const UWorld* World = GetWorld())
	{
		Event.WorldTimeSeconds = World->GetTimeSeconds();
	}

	ProcessBloodEvent(Event);
}

void UCMBloodSubsystem::HandlePoolStopMessage(
	FGameplayTag Channel,
	const FCMBloodPoolMessage& Message
)
{
	if (!IsValid(Message.Source.Get()))
	{
		UE_LOG(
			LogCMBloodSubsystem,
			Warning,
			TEXT(
				"Ignored PoolStop message: "
				"stateful blood source requires a valid Source."
			)
		);

		return;
	}

	FCMBloodEvent Event;

	Event.Type = ECMBloodEventType::PoolStop;

	Event.Source = Message.Source.Get();

	Event.Location = Message.Location;

	Event.SurfaceNormal =
		CMBloodSubsystemPrivate::NormalizeOrFallback(
			Message.SurfaceNormal,
			FVector::UpVector
		);

	Event.Magnitude =
		CMBloodSubsystemPrivate::SanitizeMagnitude(
			Message.Amount
		);

	Event.BloodDefinitionId =
		Message.BloodDefinitionId;

	if (const UWorld* World = GetWorld())
	{
		Event.WorldTimeSeconds = World->GetTimeSeconds();
	}

	ProcessBloodEvent(Event);
}

void UCMBloodSubsystem::ProcessBloodEvent(
	const FCMBloodEvent& Event
)
{
	UE_LOG(
		LogCMBloodSubsystem,
		Verbose,
		TEXT(
			"Blood Event | "
			"Type=%s | "
			"Location=%s | "
			"Magnitude=%.2f | "
			"Definition=%s | "
			"Source=%s"
		),
		CMBloodSubsystemPrivate::LexToString(Event.Type),
		*Event.Location.ToString(),
		Event.Magnitude,
		*Event.BloodDefinitionId.ToString(),
		Event.Source.IsValid()
			? *Event.Source->GetName()
			: TEXT("None")
	);

	switch (Event.Type)
	{
	case ECMBloodEventType::Impact:
	case ECMBloodEventType::Burst:
		break;

	case ECMBloodEventType::PoolStart:
	case ECMBloodEventType::PoolStop:
	{
		UCMBloodPoolSourceComponent* PoolSource =
			CMBloodSubsystemPrivate::ResolvePoolSource(Event.Source.Get());

		if (!PoolSource)
		{
			UE_LOG(
				LogCMBloodSubsystem,
				Warning,
				TEXT("Ignored %s: Source has no CMBloodPoolSourceComponent."),
				CMBloodSubsystemPrivate::LexToString(Event.Type));
			return;
		}

		if (Event.Type == ECMBloodEventType::PoolStart)
		{
			if (!PoolSource->StartBloodPool(
				Event.Location,
				Event.SurfaceNormal,
				Event.Magnitude,
				Event.BloodDefinitionId))
			{
				UE_LOG(
					LogCMBloodSubsystem,
					Warning,
					TEXT("PoolStart failed for Source '%s'."),
					*GetNameSafe(Event.Source.Get()));
			}
		}
		else
		{
			PoolSource->StopBloodPool();
		}

		return;
	}

	default:
		// Bleed state는 이후 Phase에서 처리.
		return;
	}

	const UCMBloodDefinition* Definition =
		ResolveBloodDefinition(
			Event.BloodDefinitionId
		);

	if (!IsValid(Definition))
	{
		UE_LOG(
			LogCMBloodSubsystem,
			Warning,
			TEXT(
				"Unable to resolve Blood Definition '%s'."
			),
			*Event.BloodDefinitionId.ToString()
		);

		return;
	}

	if (!FCMBloodVFXExecutor::ExecuteInstant(
		this,
		Event,
		*Definition
	))
	{
		UE_LOG(
			LogCMBloodSubsystem,
			Verbose,
			TEXT(
				"No instant Blood VFX was spawned "
				"for Definition '%s'."
			),
			*Definition->DefinitionId.ToString()
		);
	}
}

void UCMBloodSubsystem::LoadDefinitionRegistry()
{
	BloodDefinitions.Reset();
	LoadedDefinitionRegistry = nullptr;
	DefaultDefinitionId = NAME_None;

	const UCMBloodSettings* Settings =
		GetDefault<UCMBloodSettings>();

	if (!Settings)
	{
		UE_LOG(
			LogCMBloodSubsystem,
			Error,
			TEXT("Failed to get CM Blood settings.")
		);

		return;
	}

	DefaultDefinitionId =
		Settings->DefaultDefinitionId;

	UCMBloodDefinitionRegistry* Registry =
		Settings->DefinitionRegistry.LoadSynchronous();

	if (!IsValid(Registry))
	{
		UE_LOG(
			LogCMBloodSubsystem,
			Warning,
			TEXT(
				"CMGore has no valid Blood Definition Registry. "
				"Configure it in Project Settings > CM Gore."
			)
		);

		return;
	}

	LoadedDefinitionRegistry = Registry;

	for (UCMBloodDefinition* Definition : Registry->Definitions)
	{
		if (!IsValid(Definition))
		{
			UE_LOG(
				LogCMBloodSubsystem,
				Warning,
				TEXT(
					"Blood Definition Registry contains "
					"a null definition."
				)
			);

			continue;
		}

		if (Definition->DefinitionId.IsNone())
		{
			UE_LOG(
				LogCMBloodSubsystem,
				Error,
				TEXT(
					"Blood Definition '%s' has no DefinitionId."
				),
				*Definition->GetName()
			);

			continue;
		}

		if (BloodDefinitions.Contains(Definition->DefinitionId))
		{
			UE_LOG(
				LogCMBloodSubsystem,
				Error,
				TEXT(
					"Duplicate Blood DefinitionId '%s'. "
					"Definition '%s' was ignored."
				),
				*Definition->DefinitionId.ToString(),
				*Definition->GetName()
			);

			continue;
		}

		BloodDefinitions.Add(
			Definition->DefinitionId,
			Definition
		);
	}

	UE_LOG(
		LogCMBloodSubsystem,
		Log,
		TEXT(
			"Loaded %d Blood Definitions. Default='%s'"
		),
		BloodDefinitions.Num(),
		*DefaultDefinitionId.ToString()
	);
}

const UCMBloodDefinition*
UCMBloodSubsystem::ResolveBloodDefinition(
	FName RequestedDefinitionId
) const
{
	FName EffectiveId = RequestedDefinitionId;

	if (EffectiveId.IsNone())
	{
		EffectiveId = DefaultDefinitionId;
	}

	if (const TObjectPtr<UCMBloodDefinition>* Found =
		BloodDefinitions.Find(EffectiveId))
	{
		return Found->Get();
	}

	// 요청 ID가 잘못됐으면 Default Definition으로 fallback.
	if (
		!DefaultDefinitionId.IsNone() &&
		EffectiveId != DefaultDefinitionId
	)
	{
		if (const TObjectPtr<UCMBloodDefinition>* DefaultDefinition =
			BloodDefinitions.Find(DefaultDefinitionId))
		{
			UE_LOG(
				LogCMBloodSubsystem,
				Warning,
				TEXT(
					"Unknown BloodDefinitionId '%s'. "
					"Using default '%s'."
				),
				*EffectiveId.ToString(),
				*DefaultDefinitionId.ToString()
			);

			return DefaultDefinition->Get();
		}
	}

	return nullptr;
}
