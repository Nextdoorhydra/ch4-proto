#include "Stage/Test/CMTestAreaDestination.h"

#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "Player/CMChimera.h"
#include "Stage/Test/CMTestAreaManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraTestDestination, Log, All);

ACMTestAreaDestination::ACMTestAreaDestination()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;

    DestinationVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("DestinationVolume"));
    SetRootComponent(DestinationVolume);
    DestinationVolume->SetBoxExtent(FVector(250.0f, 250.0f, 150.0f));
    DestinationVolume->SetCollisionProfileName(TEXT("Trigger"));
    DestinationVolume->SetGenerateOverlapEvents(true);
    DestinationVolume->ShapeColor = FColor::Cyan;
    DestinationVolume->SetHiddenInGame(true);
}

// 서버에서 TestAreaManager를 찾고 목적지 Overlap 판정 구독
void ACMTestAreaDestination::BeginPlay()
{
    Super::BeginPlay();
    if (!HasAuthority())
    {
        return;
    }

    TestAreaManager = FindTestAreaManager();
    if (!TestAreaManager || AreaId.IsNone())
    {
        UE_LOG(LogChimeraTestDestination, Error,
            TEXT("TestAreaDestination 설정이 유효하지 않습니다. Destination=%s AreaId=%s Manager=%s"),
            *GetName(), *AreaId.ToString(), *GetNameSafe(TestAreaManager));
        return;
    }

    DestinationVolume->OnComponentBeginOverlap.AddUniqueDynamic(
        this, &ThisClass::HandleBeginOverlap);
    DestinationVolume->OnComponentEndOverlap.AddUniqueDynamic(
        this, &ThisClass::HandleEndOverlap);
}

// 모든 활성 몸통 마디가 들어온 경우에만 다음 Area 이동 요청
void ACMTestAreaDestination::HandleBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    ACMChimera* Chimera = Cast<ACMChimera>(OtherActor);
    if (!HasAuthority() || bAdvanceRequested || !IsValid(Chimera)
        || !IsValid(TestAreaManager)
        || !Chimera->AreAllActiveBodySegmentsOverlapping(DestinationVolume))
    {
        return;
    }

    bAdvanceRequested = true;
    TestAreaManager->AdvanceToNextArea(AreaId, Chimera);

    // 물리 텔레포트에서 EndOverlap 이벤트가 생략되어도 다음 순환 진입을 허용
    bAdvanceRequested = false;
}

// 키메라가 목적지를 벗어나면 다음 방문을 위해 완료 잠금 해제
void ACMTestAreaDestination::HandleEndOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex)
{
    if (HasAuthority() && Cast<ACMChimera>(OtherActor))
    {
        bAdvanceRequested = false;
    }
}

// Persistent Test Level에 배치된 유일한 Manager 검색
ACMTestAreaManager* ACMTestAreaDestination::FindTestAreaManager() const
{
    ACMTestAreaManager* Found = nullptr;
    for (TActorIterator<ACMTestAreaManager> It(GetWorld()); It; ++It)
    {
        if (Found)
        {
            UE_LOG(LogChimeraTestDestination, Error,
                TEXT("현재 월드에 TestAreaManager가 둘 이상 있습니다. First=%s Other=%s"),
                *GetNameSafe(Found), *GetNameSafe(*It));
            return nullptr;
        }
        Found = *It;
    }
    return Found;
}
