#include "Stage/Test/CMTestKillZone.h"

#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "Player/CMChimera.h"
#include "Stage/Test/CMTestAreaManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraTestKillZone, Log, All);

ACMTestKillZone::ACMTestKillZone()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;

    KillVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("KillVolume"));
    SetRootComponent(KillVolume);
    KillVolume->SetBoxExtent(FVector(500.0f, 500.0f, 100.0f));
    KillVolume->SetCollisionProfileName(TEXT("Trigger"));
    KillVolume->SetGenerateOverlapEvents(true);
    KillVolume->ShapeColor = FColor::Red;
    KillVolume->SetHiddenInGame(true);
}

// 서버에서 TestAreaManager를 찾고 Kill Volume Overlap 판정 구독
void ACMTestKillZone::BeginPlay()
{
    Super::BeginPlay();
    if (!HasAuthority())
    {
        return;
    }

    TestAreaManager = FindTestAreaManager();
    if (!TestAreaManager)
    {
        UE_LOG(LogChimeraTestKillZone, Error,
            TEXT("TestKillZone이 TestAreaManager를 찾지 못했습니다. KillZone=%s"),
            *GetName());
        return;
    }
    KillVolume->OnComponentBeginOverlap.AddUniqueDynamic(
        this, &ThisClass::HandleBeginOverlap);
}

// 키메라 마디가 닿으면 KillZone이 저장된 Sublevel 시작점으로 전체 몸통 복귀
void ACMTestKillZone::HandleBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    ACMChimera* Chimera = Cast<ACMChimera>(OtherActor);
    if (!HasAuthority() || bReturnRequested || !IsValid(Chimera)
        || !IsValid(TestAreaManager))
    {
        return;
    }

    bReturnRequested = true;
    TestAreaManager->RestartAreaForLevel(GetLevel(), Chimera);
    bReturnRequested = false;
}

// Persistent Test Level에 배치된 유일한 Manager 검색
ACMTestAreaManager* ACMTestKillZone::FindTestAreaManager() const
{
    ACMTestAreaManager* Found = nullptr;
    for (TActorIterator<ACMTestAreaManager> It(GetWorld()); It; ++It)
    {
        if (Found)
        {
            UE_LOG(LogChimeraTestKillZone, Error,
                TEXT("현재 월드에 TestAreaManager가 둘 이상 있습니다. First=%s Other=%s"),
                *GetNameSafe(Found), *GetNameSafe(*It));
            return nullptr;
        }
        Found = *It;
    }
    return Found;
}
