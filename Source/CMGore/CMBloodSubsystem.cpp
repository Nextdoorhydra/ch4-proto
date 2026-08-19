#include "CMBloodSubsystem.h"

#include "CMBloodEvent.h"
#include "CMGoreGameplayTags.h"
#include "CMGoreMessages.h"

#include "Engine/World.h"

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

	RegisterMessageListeners();

	UE_LOG(
		LogCMBloodSubsystem,
		Log,
		TEXT("UCMBloodSubsystem initialized.")
	);
}


void UCMBloodSubsystem::Deinitialize()
{
	UnregisterMessageListeners();

	UE_LOG(
		LogCMBloodSubsystem,
		Log,
		TEXT("UCMBloodSubsystem deinitialized.")
	);

	Super::Deinitialize();
}


void UCMBloodSubsystem::RegisterMessageListeners()
{
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
	// Phase2에서는 로그만 띄움
	UE_LOG(
		LogCMBloodSubsystem,
		Log,
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
}