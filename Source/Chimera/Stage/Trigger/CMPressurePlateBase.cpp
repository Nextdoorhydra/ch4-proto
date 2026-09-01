#include "Stage/Trigger/CMPressurePlateBase.h"

#include "Components/BoxComponent.h"
#include "Stage/Trigger/Component/CMActivationTriggerComponent.h"
#include "Stage/Trigger/Component/CMMechanismWeightComponent.h"

ACMPressurePlateBase::ACMPressurePlateBase()
{
    PressureVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("PressureVolume"));
    PressureVolume->SetupAttachment(SceneRoot);
    PressureVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    PressureVolume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    PressureVolume->SetGenerateOverlapEvents(true);
    ActivationTrigger->bOneShot = false;
}

// 서버에서 감압판 진입과 이탈 이벤트 구독
void ACMPressurePlateBase::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority())
    {
        PressureVolume->OnComponentBeginOverlap.AddUniqueDynamic(
            this,
            &ThisClass::HandlePressureBeginOverlap);
        PressureVolume->OnComponentEndOverlap.AddUniqueDynamic(
            this,
            &ThisClass::HandlePressureEndOverlap);
    }
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
    if (IsValid(OtherActor) && ResolveMechanismWeight(OtherActor) > 0.0f)
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
