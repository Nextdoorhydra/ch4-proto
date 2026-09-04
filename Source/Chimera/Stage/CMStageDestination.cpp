#include "Stage/CMStageDestination.h"

#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "Player/CMChimera.h"
#include "Stage/CMStageDirector.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraDestination, Log, All);

// 서버 Overlap 판정과 에디터 크기 조정을 위한 Box Volume 구성
ACMStageDestination::ACMStageDestination()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;

    DestinationVolume = CreateDefaultSubobject<UBoxComponent>(
        TEXT("DestinationVolume"));
    SetRootComponent(DestinationVolume);
    DestinationVolume->SetBoxExtent(FVector(250.0f, 250.0f, 150.0f));
    DestinationVolume->SetCollisionProfileName(TEXT("Trigger"));
    DestinationVolume->SetGenerateOverlapEvents(true);
    DestinationVolume->ShapeColor = FColor::Green;
    DestinationVolume->SetHiddenInGame(true);
}

// 서버에서 StageDirector를 확인하고 몸통 Overlap 이벤트 구독
void ACMStageDestination::BeginPlay()
{
    Super::BeginPlay();

    if (!HasAuthority())
    {
        return;
    }

    StageDirector = FindStageDirector();
    if (!StageDirector)
    {
        UE_LOG(LogChimeraDestination, Error,
            TEXT("목적지가 현재 월드의 StageDirector를 찾지 못했습니다. Destination=%s"),
            *GetName());
    }

    DestinationVolume->OnComponentBeginOverlap.AddUniqueDynamic(
        this, &ThisClass::HandleDestinationBeginOverlap);
}

// 키메라 소유 컴포넌트의 진입에만 반응해 활성 몸통 겹침 확인
void ACMStageDestination::HandleDestinationBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (HasAuthority() && !bCompletionReported)
    {
        TryCompleteStage(Cast<ACMChimera>(OtherActor));
    }
}

// 활성 BodySegment 하나 이상이 Box와 겹칠 때 StageDirector에 완료 보고
void ACMStageDestination::TryCompleteStage(ACMChimera* Chimera)
{
    if (!Chimera
        || !StageDirector
        || !Chimera->IsAnyActiveBodySegmentOverlapping(
            DestinationVolume))
    {
        return;
    }

    bCompletionReported = true;
    UE_LOG(LogChimeraDestination, Display,
        TEXT("활성 몸통 마디가 목적지에 도착했습니다. Destination=%s Segments=%d"),
        *GetName(), Chimera->GetActiveSegmentCount());
    StageDirector->CompleteStage();
}

// 중복 배치를 오류로 알리고 첫 번째 StageDirector를 완료 보고 대상으로 선택
ACMStageDirector* ACMStageDestination::FindStageDirector() const
{
    ACMStageDirector* FoundDirector = nullptr;
    for (TActorIterator<ACMStageDirector> It(GetWorld()); It; ++It)
    {
        if (FoundDirector)
        {
            UE_LOG(LogChimeraDestination, Error,
                TEXT("현재 월드에 StageDirector가 둘 이상 있습니다. First=%s Other=%s"),
                *GetNameSafe(FoundDirector), *GetNameSafe(*It));
            break;
        }
        FoundDirector = *It;
    }
    return FoundDirector;
}
