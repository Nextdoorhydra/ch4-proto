#include "Gore/CMDismembermentTestHarness.h"

#include "Engine/World.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CMBloodTransferComponent.h"
#include "Animation/SkeletalMeshActor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gore/CMDismembermentComponent.h"
#include "EngineUtils.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Runtime/Surface/CMBloodSurfaceSubsystem.h"
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
        World->GetTimerManager().ClearTimer(BloodStrokeVerificationTimerHandle);
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

    bBloodStrokeSmokeTestRequested = FParse::Param(
        FCommandLine::Get(),
        TEXT("CMGorePhase6SmokeTest"));
    if (bRunBloodPoolStrokeVisualTest || bBloodStrokeSmokeTestRequested)
    {
        if (!bPassed)
        {
            if (bBloodStrokeSmokeTestRequested)
            {
                FPlatformMisc::RequestExitWithStatus(
                    false,
                    1,
                    TEXT("CMGorePhase6SmokeTest"));
            }
            return;
        }
        GetWorld()->GetTimerManager().SetTimer(
            BloodStrokeVerificationTimerHandle,
            this,
            &ACMDismembermentTestHarness::VerifyBloodPoolStroke,
            BloodStrokeVisualStartDelaySeconds,
            false);
    }
}

void ACMDismembermentTestHarness::VerifyBloodPoolStroke()
{
    UCMDismembermentComponent* Dismemberment = SpawnedSubject
        ? SpawnedSubject->FindComponentByClass<UCMDismembermentComponent>()
        : nullptr;
    AActor* DetachedActor = Dismemberment
        ? Dismemberment->GetDetachedPartActor(ECMBodyPart::ArmLeft)
        : nullptr;
    UCMBloodTransferComponent* Transfer = DetachedActor
        ? DetachedActor->FindComponentByClass<UCMBloodTransferComponent>()
        : nullptr;
    UCMBloodSurfaceSubsystem* SurfaceSubsystem = GetWorld()
        ? GetWorld()->GetSubsystem<UCMBloodSurfaceSubsystem>()
        : nullptr;

    TArray<FCMBloodMark> Marks;
    if (SurfaceSubsystem && SpawnedSubject)
    {
        SurfaceSubsystem->GetBloodMarksInRadius(
            SpawnedSubject->GetActorLocation(),
            500.0f,
            Marks);
    }
    const FCMBloodMark* PoolMark = Marks.FindByPredicate(
        [this](const FCMBloodMark& Mark)
        {
            return Mark.ResidueType == ECMBloodResidueType::Pool &&
                Mark.SourceActor == SpawnedSubject;
        });
    if (Transfer && PoolMark)
    {
        BloodStrokeVisualPart = DetachedActor;
        BloodStrokeVisualTransfer = Transfer;
        BloodStrokePoolLocation = PoolMark->WorldTransform.GetLocation();
        BloodStrokeSurfaceNormal = PoolMark->SurfaceNormal.GetSafeNormal(
            SMALL_NUMBER,
            FVector::UpVector);
        BloodStrokeSurfaceTangent =
            PoolMark->WorldTransform.GetUnitAxis(EAxis::Y);
        BloodStrokeTravelDistance =
            PoolMark->DecalSize.Y * PoolMark->PresentationProgress;
        BloodStrokeVisualStep = 0;

        if (USkeletalMeshComponent* DetachedMesh =
            DetachedActor->FindComponentByClass<USkeletalMeshComponent>())
        {
            DetachedMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
            DetachedMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
            DetachedMesh->SetAllBodiesSimulatePhysics(false);
            DetachedMesh->SetSimulatePhysics(false);
        }
        DetachedActor->SetActorLocation(
            BloodStrokePoolLocation + BloodStrokeSurfaceNormal * 20.0f,
            false,
            nullptr,
            ETeleportType::TeleportPhysics);

        const bool bLoaded = Transfer->ProcessContactSample(
            BloodStrokePoolLocation,
            PoolMark->SurfaceNormal,
            100.0f);
        BloodStrokeLoadedDistance = Transfer->GetRemainingStrokeDistance();
        if (bLoaded && BloodStrokeLoadedDistance >= 100.0f &&
            BloodStrokeLoadedDistance <= 200.0f)
        {
            UE_LOG(LogCMDismembermentTest, Display,
                TEXT("[DismembermentTest] Blood Pool Stroke visual test started: part=%s budget=%.1fcm."),
                *GetNameSafe(DetachedActor),
                BloodStrokeLoadedDistance);
            GetWorld()->GetTimerManager().SetTimer(
                BloodStrokeVerificationTimerHandle,
                this,
                &ACMDismembermentTestHarness::AdvanceBloodPoolStrokeVisualTest,
                BloodStrokeVisualStepIntervalSeconds,
                false);
            return;
        }
    }

    FinishBloodPoolStrokeVisualTest(false);
}

void ACMDismembermentTestHarness::AdvanceBloodPoolStrokeVisualTest()
{
    if (!BloodStrokeVisualPart || !BloodStrokeVisualTransfer)
    {
        FinishBloodPoolStrokeVisualTest(false);
        return;
    }

    BloodStrokeTravelDistance += FMath::Max(1.0f, BloodStrokeVisualStepDistance);
    ++BloodStrokeVisualStep;
    const FVector ContactLocation = BloodStrokePoolLocation +
        BloodStrokeSurfaceTangent * BloodStrokeTravelDistance;
    BloodStrokeVisualPart->SetActorLocation(
        ContactLocation + BloodStrokeSurfaceNormal * 20.0f,
        false,
        nullptr,
        ETeleportType::TeleportPhysics);
    BloodStrokeVisualTransfer->ProcessContactSample(
        ContactLocation,
        BloodStrokeSurfaceNormal,
        100.0f);

    const bool bCompleted =
        BloodStrokeVisualTransfer->GetTransferState() ==
            ECMBloodTransferState::Dry;
    if (bCompleted || BloodStrokeVisualStep >= 20)
    {
        FinishBloodPoolStrokeVisualTest(bCompleted);
        return;
    }

    GetWorld()->GetTimerManager().SetTimer(
        BloodStrokeVerificationTimerHandle,
        this,
        &ACMDismembermentTestHarness::AdvanceBloodPoolStrokeVisualTest,
        BloodStrokeVisualStepIntervalSeconds,
        false);
}

void ACMDismembermentTestHarness::FinishBloodPoolStrokeVisualTest(
    const bool bMovementCompleted)
{
    UCMBloodSurfaceSubsystem* SurfaceSubsystem = GetWorld()
        ? GetWorld()->GetSubsystem<UCMBloodSurfaceSubsystem>()
        : nullptr;
    TArray<FCMBloodMark> Marks;
    if (SurfaceSubsystem)
    {
        SurfaceSubsystem->GetBloodMarksInRadius(
            BloodStrokePoolLocation,
            500.0f,
            Marks);
    }

    const int32 StrokeMarkCount = Marks.FilterByPredicate(
        [this](const FCMBloodMark& Mark)
        {
            return Mark.ResidueType == ECMBloodResidueType::Stroke &&
                Mark.SourceActor == BloodStrokeVisualPart;
        }).Num();
    const int32 StampCount = BloodStrokeVisualTransfer
        ? BloodStrokeVisualTransfer->GetSpawnedStrokeStampCount()
        : 0;
    const float PaintedDistance = BloodStrokeVisualTransfer
        ? BloodStrokeVisualTransfer->GetTotalPaintedDistance()
        : 0.0f;
    const bool bPassed = bMovementCompleted && StampCount > 0 &&
        PaintedDistance >= 90.0f && PaintedDistance <= 204.0f &&
        StrokeMarkCount > 0;

    UE_LOG(LogCMDismembermentTest, Display,
        TEXT("[DismembermentTest] Blood Pool Stroke visual test: %s | Budget=%.1fcm Stamps=%d Painted=%.1fcm StrokeMarks=%d"),
        bPassed ? TEXT("PASS") : TEXT("FAIL"),
        BloodStrokeLoadedDistance,
        StampCount,
        PaintedDistance,
        StrokeMarkCount);

    if (USkeletalMeshComponent* DetachedMesh = BloodStrokeVisualPart
        ? BloodStrokeVisualPart->FindComponentByClass<USkeletalMeshComponent>()
        : nullptr)
    {
        DetachedMesh->SetAllBodiesSimulatePhysics(true);
        DetachedMesh->SetSimulatePhysics(true);
        DetachedMesh->WakeAllRigidBodies();
    }

    if (bBloodStrokeSmokeTestRequested)
    {
        FPlatformMisc::RequestExitWithStatus(
            false,
            bPassed ? 0 : 1,
            TEXT("CMGorePhase6SmokeTest"));
    }
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
