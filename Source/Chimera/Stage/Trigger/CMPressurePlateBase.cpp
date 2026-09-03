#include "Stage/Trigger/CMPressurePlateBase.h"

#include "Collision/CMCollisionChannels.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Stage/Trigger/Component/CMActivationTriggerComponent.h"
#include "Stage/Trigger/Component/CMMechanismWeightComponent.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraPressurePlate, Log, All);

ACMPressurePlateBase::ACMPressurePlateBase()
{
    PressureVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("PressureVolume"));
    PressureVolume->SetupAttachment(SceneRoot);
    PressureVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    PressureVolume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    PressureVolume->SetGenerateOverlapEvents(true);

    PlateVisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("PlateVisualMesh"));
    PlateVisualMesh->SetupAttachment(SceneRoot);
    PlateVisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ActivationTrigger->bOneShot = false;
}

// 서버에서 감압판 진입과 이탈 이벤트 구독
void ACMPressurePlateBase::BeginPlay()
{
    Super::BeginPlay();

    PlateMaterial = PlateVisualMesh->CreateDynamicMaterialInstance(
        FMath::Max(MaterialSlotIndex, 0));
    OnPresentationStateChanged.AddUniqueDynamic(
        this, &ThisClass::HandlePresentationStateChanged);
    ApplyPresentationState(GetPresentationState());

    if (HasAuthority())
    {
        // Blueprint component templates can retain older collision overrides, so restore the gameplay contract at runtime.
        PressureVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        PressureVolume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
        PressureVolume->SetCollisionResponseToChannel(
            CMCollision::ChimeraHurtbox,
            ECR_Overlap
        );
        PressureVolume->SetGenerateOverlapEvents(true);
        PressureVolume->OnComponentBeginOverlap.AddUniqueDynamic(
            this,
            &ThisClass::HandlePressureBeginOverlap);
        PressureVolume->OnComponentEndOverlap.AddUniqueDynamic(
            this,
            &ThisClass::HandlePressureEndOverlap);
        GetWorldTimerManager().SetTimer(OverlapRefreshTimerHandle, this, &ThisClass::RefreshOverlaps, 0.1f, true, 0.0f);
    }
}

void ACMPressurePlateBase::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(OverlapRefreshTimerHandle);
    OnPresentationStateChanged.RemoveDynamic(
        this, &ThisClass::HandlePresentationStateChanged);
    Super::EndPlay(EndPlayReason);
}

// 같은 액터의 여러 콜리전이 들어와도 무게는 한 번만 더하도록 횟수 기록
void ACMPressurePlateBase::HandlePressureBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    const float MechanismWeight = ResolveMechanismWeight(OtherActor);
    UE_LOG(LogChimeraPressurePlate, Log, TEXT("[Pressure Overlap Begin] Plate=%s Actor=%s Component=%s Weight=%.1f OtherOverlap=%s"), *GetName(), *GetNameSafe(OtherActor), *GetNameSafe(OtherComponent), MechanismWeight, IsValid(OtherComponent) && OtherComponent->GetGenerateOverlapEvents() ? TEXT("true") : TEXT("false"));
    if (IsValid(OtherActor) && MechanismWeight > 0.0f)
    {
        ++OverlapCounts.FindOrAdd(OtherActor);
        RecalculatePressure(OtherActor);
    }
}

// 마지막 콜리전이 이탈했을 때만 해당 액터의 무게 제거
void ACMPressurePlateBase::HandlePressureEndOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex)
{
    UE_LOG(LogChimeraPressurePlate, Log, TEXT("[Pressure Overlap End] Plate=%s Actor=%s Component=%s"), *GetName(), *GetNameSafe(OtherActor), *GetNameSafe(OtherComponent));
    if (int32* Count = OverlapCounts.Find(OtherActor))
    {
        if (--(*Count) <= 0)
        {
            OverlapCounts.Remove(OtherActor);
        }
        RecalculatePressure(OtherActor);
    }
}

// 유효한 무게 제공 액터를 합산하고 눌림과 해제 임계값을 적용
void ACMPressurePlateBase::RecalculatePressure(AActor* ChangedActor)
{
    CurrentWeight = 0.0f;
    for (auto It = OverlapCounts.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid() || It.Value() <= 0)
        {
            It.RemoveCurrent();
            continue;
        }
        CurrentWeight += ResolveMechanismWeight(It.Key().Get());
    }

    if (!ActivationTrigger->IsTriggered() && CurrentWeight >= RequiredWeight)
    {
        PressButton(ChangedActor);
    }
    else if (ActivationTrigger->IsTriggered() && CurrentWeight < ReleaseWeight)
    {
        ReleaseButton(ChangedActor);
    }
    RefreshPresentationState();
    OnPressureChanged(CurrentWeight, ActivationTrigger->IsTriggered());
    UE_LOG(LogChimeraPressurePlate, Log, TEXT("[Pressure Weight] Plate=%s Changed=%s Current=%.1f Required=%.1f Triggered=%s"), *GetName(), *GetNameSafe(ChangedActor), CurrentWeight, RequiredWeight, ActivationTrigger->IsTriggered() ? TEXT("true") : TEXT("false"));
}

// Overlap 이벤트 누락 여부와 무관하게 현재 발판 영역의 무게 액터를 주기적으로 동기화
void ACMPressurePlateBase::RefreshOverlaps()
{
    if (!HasAuthority() || !PressureVolume)
    {
        return;
    }

    PressureVolume->UpdateOverlaps();
    TArray<UPrimitiveComponent*> OverlappingComponents;
    PressureVolume->GetOverlappingComponents(OverlappingComponents);
    TMap<TWeakObjectPtr<AActor>, int32> RefreshedOverlapCounts;
    for (UPrimitiveComponent* OverlappingComponent : OverlappingComponents)
    {
        AActor* OverlappingActor = IsValid(OverlappingComponent) ? OverlappingComponent->GetOwner() : nullptr;
        if (IsValid(OverlappingActor) && ResolveMechanismWeight(OverlappingActor) > 0.0f)
        {
            ++RefreshedOverlapCounts.FindOrAdd(OverlappingActor);
        }
    }
    const bool bOverlapStateChanged = !bHasRefreshedOverlaps || !OverlapCounts.OrderIndependentCompareEqual(RefreshedOverlapCounts);
    if (bOverlapStateChanged)
    {
        OverlapCounts = MoveTemp(RefreshedOverlapCounts);
        RecalculatePressure(nullptr);
        UE_LOG(LogChimeraPressurePlate, Log, TEXT("[Pressure Overlap Scan] Plate=%s Components=%d Actors=%d Current=%.1f Required=%.1f GenerateOverlap=%s Collision=%d Profile=%s VolumeOrigin=%s VolumeExtent=%s VisualOrigin=%s VisualExtent=%s"), *GetName(), OverlappingComponents.Num(), OverlapCounts.Num(), CurrentWeight, RequiredWeight, PressureVolume->GetGenerateOverlapEvents() ? TEXT("true") : TEXT("false"), static_cast<int32>(PressureVolume->GetCollisionEnabled()), *PressureVolume->GetCollisionProfileName().ToString(), *PressureVolume->Bounds.Origin.ToCompactString(), *PressureVolume->Bounds.BoxExtent.ToCompactString(), PlateVisualMesh ? *PlateVisualMesh->Bounds.Origin.ToCompactString() : TEXT("None"), PlateVisualMesh ? *PlateVisualMesh->Bounds.BoxExtent.ToCompactString() : TEXT("None"));
    }
    bHasRefreshedOverlaps = true;
}

// 대상 액터에 설정된 게임플레이 무게 컴포넌트 값 조회
float ACMPressurePlateBase::ResolveMechanismWeight(const AActor* Actor) const
{
    if (const UCMMechanismWeightComponent* WeightComponent =
            Actor ? Actor->FindComponentByClass<UCMMechanismWeightComponent>() : nullptr)
    {
        return FMath::Max(WeightComponent->GetMechanismWeight(), 0.0f);
    }
    return 0.0f;
}

// 리셋 시 기존 Overlap을 비우고 다음 진입부터 무게를 다시 계산
void ACMPressurePlateBase::HandleElementReset_Implementation()
{
    OverlapCounts.Reset();
    CurrentWeight = 0.0f;
    bHasRefreshedOverlaps = false;
    Super::HandleElementReset_Implementation();
    RefreshPresentationState();
    OnPressureChanged(CurrentWeight, false);
}

void ACMPressurePlateBase::FillPresentationState(FCMTriggerPresentationState& State) const
{
    Super::FillPresentationState(State);
    State.bSupportsWeight = true;
    State.CurrentWeight = CurrentWeight;
    State.RequiredWeight = RequiredWeight;
    State.ReleaseWeight = ReleaseWeight;
}

void ACMPressurePlateBase::HandlePresentationStateChanged(
    const FCMTriggerPresentationState& State)
{
    ApplyPresentationState(State);
}

void ACMPressurePlateBase::ApplyPresentationState(
    const FCMTriggerPresentationState& State)
{
    if (!State.bReady || !PlateMaterial)
    {
        return;
    }

    const FLinearColor Color = State.bEnabled
        ? (State.bTriggered ? OnColor : OffColor)
        : DisabledColor;
    const float EmissiveIntensity = !State.bEnabled
        ? 0.0f
        : State.bTriggered
            ? FMath::Max(OnEmissiveIntensity, 0.0f)
            : FMath::Max(OffEmissiveIntensity, 0.0f);
    if (!ColorParameterName.IsNone())
    {
        PlateMaterial->SetVectorParameterValue(ColorParameterName, Color);
    }
    if (!EmissiveParameterName.IsNone())
    {
        PlateMaterial->SetScalarParameterValue(
            EmissiveParameterName, EmissiveIntensity);
    }
}
