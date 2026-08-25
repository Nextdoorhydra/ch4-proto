#include "Gore/CMDismembermentTestHarness.h"

#include "Engine/World.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/SkeletalMeshActor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gore/CMDismembermentComponent.h"
#include "EngineUtils.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCMDismembermentTest, Log, All);

ACMDismembermentTestHarness::ACMDismembermentTestHarness()
{
    PrimaryActorTick.bCanEverTick = false;
    SubjectClass = TSoftClassPtr<ACharacter>(FSoftObjectPath(
        TEXT("/Game/Chimera/BP_Human.BP_Human_C")));
}

void ACMDismembermentTestHarness::BeginPlay()
{
    Super::BeginPlay();

    if (!HasAuthority())
    {
        return;
    }

    UWorld* World = GetWorld();
    UClass* LoadedSubjectClass = SubjectClass.LoadSynchronous();
    if (!World || !LoadedSubjectClass)
    {
        UE_LOG(LogCMDismembermentTest, Error,
            TEXT("[DismembermentTest] Subject class could not be loaded."));
        return;
    }

    if (bUseExistingSubject)
    {
        for (TActorIterator<ACharacter> It(World); It; ++It)
        {
            if (It->IsA(LoadedSubjectClass))
            {
                SpawnedSubject = *It;
                break;
            }
        }
    }

    if (!SpawnedSubject)
    {
        const FTransform SpawnTransform(
            GetActorRotation(),
            GetActorLocation() + SubjectSpawnOffset);
        SpawnedSubject = World->SpawnActorDeferred<ACharacter>(
            LoadedSubjectClass,
            SpawnTransform,
            this,
            nullptr,
            ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

        if (!SpawnedSubject)
        {
            UE_LOG(LogCMDismembermentTest, Error,
                TEXT("[DismembermentTest] Failed to spawn the subject."));
            return;
        }

        if (!EnsureDismembermentComponent())
        {
            SpawnedSubject->Destroy();
            SpawnedSubject = nullptr;
            return;
        }

        SpawnedSubject->FinishSpawning(SpawnTransform);
    }
    else if (!EnsureDismembermentComponent())
    {
        return;
    }

    World->GetTimerManager().SetTimer(
        SeverStepTimerHandle,
        this,
        &ACMDismembermentTestHarness::HandleSeverStepElapsed,
        InitialSeverDelaySeconds,
        false);

    UE_LOG(LogCMDismembermentTest, Log,
        TEXT("[DismembermentTest] Prepared '%s'; first living sever scheduled in %.2f seconds."),
        *GetNameSafe(SpawnedSubject),
        InitialSeverDelaySeconds);
}

void ACMDismembermentTestHarness::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DeathTimerHandle);
        World->GetTimerManager().ClearTimer(SeverStepTimerHandle);
        World->GetTimerManager().ClearTimer(VerificationTimerHandle);
    }
    Super::EndPlay(EndPlayReason);
}

void ACMDismembermentTestHarness::HandleDeathTimerElapsed()
{
    TriggerTestDeath();
}

void ACMDismembermentTestHarness::HandleSeverStepElapsed()
{
    static const ECMBodyPart BodyParts[] = {
        ECMBodyPart::ArmLeft,
        ECMBodyPart::ArmRight,
        ECMBodyPart::Head,
        ECMBodyPart::LegLeft,
        ECMBodyPart::LegRight
    };
    static const FName ComponentNames[] = {
        TEXT("Arn_L"),
        TEXT("Arn_R"),
        TEXT("Head"),
        TEXT("Leg_L"),
        TEXT("Leg_R")
    };
    static const TCHAR* PartLabels[] = {
        TEXT("ArmLeft"),
        TEXT("ArmRight"),
        TEXT("Head"),
        TEXT("LegLeft"),
        TEXT("LegRight")
    };

    if (!SpawnedSubject || CurrentSeverStep >= UE_ARRAY_COUNT(BodyParts))
    {
        return;
    }

    UCMDismembermentComponent* Component =
        SpawnedSubject->FindComponentByClass<UCMDismembermentComponent>();
    USkeletalMeshComponent* PartMesh =
        FindSubjectPartMesh(ComponentNames[CurrentSeverStep]);
    const int32 PreviousBloodBurstCount = Component
        ? Component->GetBroadcastBloodBurstCount()
        : 0;
    const int32 PreviousBloodDecalCount = Component
        ? Component->GetSpawnedBloodDecalCount()
        : 0;
    const FVector SeverLocation = PartMesh
        ? PartMesh->Bounds.Origin
        : SpawnedSubject->GetActorLocation();
    const bool bSevered = Component && Component->SeverBodyPart(
        BodyParts[CurrentSeverStep],
        SeverLocation,
        TestSeverImpulse);
    ASkeletalMeshActor* DetachedActor = Component
        ? Cast<ASkeletalMeshActor>(
            Component->GetDetachedPartActor(BodyParts[CurrentSeverStep]))
        : nullptr;
    USkeletalMeshComponent* DetachedMesh = DetachedActor
        ? DetachedActor->GetSkeletalMeshComponent()
        : nullptr;
    const bool bStepPassed = bSevered && PartMesh && !PartMesh->IsVisible() &&
        DetachedMesh && DetachedMesh->IsSimulatingPhysics() &&
        Component->GetBroadcastBloodBurstCount() ==
            PreviousBloodBurstCount + 1 &&
        Component->GetSpawnedBloodDecalCount() >=
            PreviousBloodDecalCount + 1;

    UE_LOG(LogCMDismembermentTest, Display,
        TEXT("[DismembermentTest] Living sever %d/5 %s: %s | BloodBurst=%d BloodDecals=%d Location=%s"),
        CurrentSeverStep + 1,
        PartLabels[CurrentSeverStep],
        bStepPassed ? TEXT("PASS") : TEXT("FAIL"),
        Component ? Component->GetBroadcastBloodBurstCount() : 0,
        Component ? Component->GetSpawnedBloodDecalCount() : 0,
        *SeverLocation.ToCompactString());

    if (!bStepPassed)
    {
        return;
    }

    ++CurrentSeverStep;
    if (CurrentSeverStep < UE_ARRAY_COUNT(BodyParts))
    {
        GetWorld()->GetTimerManager().SetTimer(
            SeverStepTimerHandle,
            this,
            &ACMDismembermentTestHarness::HandleSeverStepElapsed,
            SeverStepIntervalSeconds,
            false);
    }
    else
    {
        GetWorld()->GetTimerManager().SetTimer(
            DeathTimerHandle,
            this,
            &ACMDismembermentTestHarness::HandleDeathTimerElapsed,
            PostSeverDeathDelaySeconds,
            false);
        UE_LOG(LogCMDismembermentTest, Display,
            TEXT("[DismembermentTest] Living sever sequence complete; death scheduled in %.2f seconds."),
            PostSeverDeathDelaySeconds);
    }
}

bool ACMDismembermentTestHarness::TriggerTestDeath()
{
    if (!HasAuthority() || !SpawnedSubject)
    {
        return false;
    }

    UCMDismembermentComponent* Component =
        SpawnedSubject->FindComponentByClass<UCMDismembermentComponent>();
    const bool bStarted = Component && Component->EnterCorpseRagdoll();

    UE_LOG(LogCMDismembermentTest, Log,
        TEXT("[DismembermentTest] Death trigger for '%s': %s."),
        *GetNameSafe(SpawnedSubject),
        bStarted ? TEXT("PASS") : TEXT("FAIL"));

    if (bStarted)
    {
        GetWorld()->GetTimerManager().SetTimer(
            VerificationTimerHandle,
            this,
            &ACMDismembermentTestHarness::VerifyTestDeath,
            VerificationDelaySeconds,
            false);
    }
    return bStarted;
}

bool ACMDismembermentTestHarness::EnsureDismembermentComponent()
{
    if (!SpawnedSubject)
    {
        return false;
    }

    if (SpawnedSubject->FindComponentByClass<UCMDismembermentComponent>())
    {
        return true;
    }

    UCMDismembermentComponent* Component =
        NewObject<UCMDismembermentComponent>(
            SpawnedSubject,
            TEXT("DismembermentComponent"));
    if (!Component)
    {
        return false;
    }

    SpawnedSubject->AddInstanceComponent(Component);
    Component->RegisterComponent();
    return true;
}

void ACMDismembermentTestHarness::VerifyTestDeath()
{
    UCMDismembermentComponent* Component = SpawnedSubject
        ? SpawnedSubject->FindComponentByClass<UCMDismembermentComponent>()
        : nullptr;
    USkeletalMeshComponent* Mesh = SpawnedSubject
        ? SpawnedSubject->GetMesh()
        : nullptr;
    const UCapsuleComponent* Capsule = SpawnedSubject
        ? SpawnedSubject->GetCapsuleComponent()
        : nullptr;
    const UCharacterMovementComponent* Movement = SpawnedSubject
        ? SpawnedSubject->GetCharacterMovement()
        : nullptr;

    const ECMBodyPart SeveredParts[] = {
        ECMBodyPart::ArmLeft,
        ECMBodyPart::ArmRight,
        ECMBodyPart::Head,
        ECMBodyPart::LegLeft,
        ECMBodyPart::LegRight
    };
    bool bSeveredStatesPreserved = Component != nullptr;
    bool bDetachedPhysics = Component != nullptr;
    for (const ECMBodyPart BodyPart : SeveredParts)
    {
        bSeveredStatesPreserved &= Component &&
            Component->GetPartState(BodyPart) == ECMBodyPartState::Severed;
        ASkeletalMeshActor* DetachedActor = Component
            ? Cast<ASkeletalMeshActor>(
                Component->GetDetachedPartActor(BodyPart))
            : nullptr;
        USkeletalMeshComponent* DetachedMesh = DetachedActor
            ? DetachedActor->GetSkeletalMeshComponent()
            : nullptr;
        bDetachedPhysics &=
            DetachedMesh && DetachedMesh->IsSimulatingPhysics();
    }
    const bool bTorsoCorpseAttached = Component &&
        Component->GetPartState(ECMBodyPart::Torso) ==
        ECMBodyPartState::CorpseAttached;
    const bool bBloodBurstsComplete = Component &&
        Component->GetBroadcastBloodBurstCount() ==
        UE_ARRAY_COUNT(SeveredParts);
    const int32 ExpectedChunkCount = Component
        ? UE_ARRAY_COUNT(SeveredParts) * Component->FleshChunkCount
        : 0;
    const bool bChunksComplete = Component &&
        Component->GetSpawnedFleshChunkCount() >= ExpectedChunkCount;
    const bool bInitialBloodDecalsComplete = Component &&
        Component->GetSpawnedBloodDecalCount() >=
        UE_ARRAY_COUNT(SeveredParts);

    const bool bPassed =
        Component &&
        Component->IsCorpseRagdoll() &&
        Mesh &&
        Mesh->IsSimulatingPhysics() &&
        Capsule &&
        Capsule->GetCollisionEnabled() == ECollisionEnabled::NoCollision &&
        Movement &&
        Movement->MovementMode == MOVE_None &&
        bSeveredStatesPreserved &&
        bTorsoCorpseAttached &&
        bDetachedPhysics &&
        bBloodBurstsComplete &&
        bChunksComplete &&
        bInitialBloodDecalsComplete &&
        Component->IsCorpseBloodPoolActive();

    UE_LOG(LogCMDismembermentTest, Display,
        TEXT("[DismembermentTest] Living-sever-then-death verification: %s | Ragdoll=%s SeveredPreserved=%s TorsoCorpse=%s DetachedPhysics=%s BloodBursts=%d/5 BloodDecals=%d BloodPool=%s FleshChunks=%d/%d"),
        bPassed ? TEXT("PASS") : TEXT("FAIL"),
        Component && Component->IsCorpseRagdoll() ? TEXT("true") : TEXT("false"),
        bSeveredStatesPreserved ? TEXT("true") : TEXT("false"),
        bTorsoCorpseAttached ? TEXT("true") : TEXT("false"),
        bDetachedPhysics ? TEXT("true") : TEXT("false"),
        Component ? Component->GetBroadcastBloodBurstCount() : 0,
        Component ? Component->GetSpawnedBloodDecalCount() : 0,
        Component && Component->IsCorpseBloodPoolActive()
            ? TEXT("active")
            : TEXT("inactive"),
        Component ? Component->GetSpawnedFleshChunkCount() : 0,
        ExpectedChunkCount);
}

USkeletalMeshComponent* ACMDismembermentTestHarness::FindSubjectPartMesh(
    const FName ComponentName
) const
{
    if (!SpawnedSubject)
    {
        return nullptr;
    }

    TInlineComponentArray<USkeletalMeshComponent*> MeshComponents;
    SpawnedSubject->GetComponents(MeshComponents);
    for (USkeletalMeshComponent* MeshComponent : MeshComponents)
    {
        if (MeshComponent && MeshComponent->GetFName() == ComponentName)
        {
            return MeshComponent;
        }
    }
    return nullptr;
}
