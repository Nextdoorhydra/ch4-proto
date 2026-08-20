#include "Runtime/Surface/Presentation/CMBloodDecalActor.h"

#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"


ACMBloodDecalActor::ACMBloodDecalActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);
	SetReplicates(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	DecalComponent = CreateDefaultSubobject<UDecalComponent>(TEXT("Decal"));
	DecalComponent->SetupAttachment(SceneRoot);
	DecalComponent->SetAutoActivate(false);
	DecalComponent->SetHiddenInGame(true);
	DecalComponent->SetVisibility(false);

	SetActorHiddenInGame(true);
}


void ACMBloodDecalActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Spawn 직후의 inactive presentation도 render state를 보유하지 않는다.
	if (DecalComponent && DecalComponent->IsRegistered())
	{
		DecalComponent->UnregisterComponent();
	}
}


void ACMBloodDecalActor::ActivatePresentation(
	const FCMBloodDecalSpawnContext& Context,
	UMaterialInterface* MaterialOverride)
{
	if (!DecalComponent || !GetWorld())
	{
		return;
	}

	if (bPresentationActive)
	{
		DeactivatePresentation();
	}
	else if (DecalComponent->IsRegistered())
	{
		DecalComponent->UnregisterComponent();
	}

	UMaterialInterface* Material =
		IsValid(MaterialOverride)
			? MaterialOverride
			: DefaultMaterial.Get();

	if (!IsValid(Material))
	{
		return;
	}

	SetActorTransform(Context.WorldTransform);

	DecalComponent->DecalSize = Context.DecalSize;
	DecalComponent->SetDecalMaterial(Material);

	DynamicMaterial = DecalComponent->CreateDynamicMaterialInstance();

	if (DynamicMaterial)
	{
		ConfigureMaterial(DynamicMaterial, Context);
	}

	const float Lifetime = FMath::Max(0.0f, Context.LifetimeSeconds);
	const float FadeDuration =
		Lifetime > 0.0f
			? FMath::Clamp(Context.FadeDurationSeconds, 0.0f, Lifetime)
			: 0.0f;

	const float FadeStartDelay =
		FMath::Max(0.0f, Lifetime - FadeDuration);

	/*
	 * SetFadeOut은 DestroyOwnerAfterFade=false여도 UDecalComponent 자체의
	 * destroy timer를 설정한다. Fade 값만 구성한 뒤 timer를 취소하여
	 * pooled actor의 default subobject가 파괴되지 않게 한다.
	 */
	DecalComponent->SetFadeOut(FadeStartDelay, FadeDuration, false);
	DecalComponent->SetLifeSpan(0.0f);

	DecalComponent->SetHiddenInGame(false);
	DecalComponent->SetVisibility(true);
	SetActorHiddenInGame(false);

	/*
	 * Phase 4B에서 Material/Transform보다 먼저 register한 decal이
	 * render되지 않았으므로 완전한 상태를 구성한 뒤 마지막에 등록한다.
	 */
	DecalComponent->RegisterComponentWithWorld(GetWorld());

	if (!DecalComponent->IsRegistered())
	{
		DeactivatePresentation();
		return;
	}

	bPresentationActive = true;
	OnPresentationActivated(Context);
}


void ACMBloodDecalActor::DeactivatePresentation()
{
	const bool bWasActive = bPresentationActive;
	bPresentationActive = false;

	if (DecalComponent)
	{
		// Render state를 먼저 제거한 뒤 재사용 상태를 초기화한다.
		if (DecalComponent->IsRegistered())
		{
			DecalComponent->UnregisterComponent();
		}

		DecalComponent->SetFadeOut(0.0f, 0.0f, false);
		DecalComponent->SetHiddenInGame(true);
		DecalComponent->SetVisibility(false);
		DecalComponent->SetDecalMaterial(nullptr);
	}

	DynamicMaterial = nullptr;
	SetActorHiddenInGame(true);

	if (bWasActive)
	{
		OnPresentationDeactivated();
	}
}


void ACMBloodDecalActor::ConfigureMaterial_Implementation(
	UMaterialInstanceDynamic* MID,
	const FCMBloodDecalSpawnContext& Context)
{
}
