#include "Sacrifice/CMSacrificeThreatTracker.h"

#include "Aggressive/Common/Core/CMAggressivePawnBase.h"
#include "Common/Core/CMThreatSource.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameMode/CMGameState.h"
#include "HAL/IConsoleManager.h"
#include "Player/CMChimera.h"
#include "Sacrifice/CMSacrificeCharacter.h"
#include "Sacrifice/CMSacrificeRules.h"

DEFINE_LOG_CATEGORY_STATIC(LogCMSacrificeSight, Log, All);

namespace CMSacrificeSightDebug
{
#if !UE_BUILD_SHIPPING
    bool bDrawSight = false;

    void SetSightDebugDraw(const TArray<FString>& Args, UWorld*)
    {
        if (Args.Num() == 1)
        {
            const FString& Value = Args[0];
            if (Value.Equals(TEXT("on"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Value == TEXT("1"))
            {
                bDrawSight = true;
            }
            else if (Value.Equals(TEXT("off"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("false"), ESearchCase::IgnoreCase) || Value == TEXT("0"))
            {
                bDrawSight = false;
            }
            else
            {
                UE_LOG(LogCMSacrificeSight, Warning, TEXT("Usage: CM.AI.SacrificeSightDebug on|off"));

                return;
            }
        }
        else if (Args.Num() != 0)
        {
            UE_LOG(LogCMSacrificeSight, Warning, TEXT("Usage: CM.AI.SacrificeSightDebug on|off"));

            return;
        }

        UE_LOG(LogCMSacrificeSight, Log, TEXT("Sacrifice sight debug: %s"), bDrawSight ? TEXT("on") : TEXT("off"));
    }

    FAutoConsoleCommandWithWorldAndArgs SightDebugCommand(TEXT("CM.AI.SacrificeSightDebug"), TEXT("Draw sacrifice sight cones. Usage: CM.AI.SacrificeSightDebug on|off"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetSightDebugDraw));
#endif

    constexpr float DebugLifetime = 0.15f;
} // namespace CMSacrificeSightDebug

void FCMSacrificeThreatTracker::Initialize(ACMSacrificeCharacter* InSacrifice)
{
    Reset();
    Sacrifice = InSacrifice;
}

void FCMSacrificeThreatTracker::Reset()
{
    Sacrifice.Reset();
    RememberedThreatLocations.Reset();
}

// 플레이어 Chimera와 활성 적대 AI 중 현재 감지 조건을 통과한 위협을 수집한다.
void FCMSacrificeThreatTracker::GatherVisibleThreats(TArray<AActor*>& OutThreats) const
{
    OutThreats.Reset();

    const ACMSacrificeCharacter* SacrificeCharacter = Sacrifice.Get();
    UWorld* World = SacrificeCharacter ? SacrificeCharacter->GetWorld() : nullptr;
    if (!SacrificeCharacter || !World)
    {
        return;
    }

    const ACMGameState* GameState = World->GetGameState<ACMGameState>();
    if (GameState && IsThreatDetected(GameState->SharedChimera))
    {
        OutThreats.Add(GameState->SharedChimera);
    }

    for (TActorIterator<ACMAggressivePawnBase> It(World); It; ++It)
    {
        ACMAggressivePawnBase* Candidate = *It;
        if (IsThreatDetected(Candidate))
        {
            OutThreats.Add(Candidate);
        }
    }
}

// 사라지거나 비활성화된 위협을 제거하고 보이는 위협의 마지막 위치를 기억한다.
void FCMSacrificeThreatTracker::UpdateMemory(const TArray<AActor*>& VisibleThreats)
{
    for (auto It = RememberedThreatLocations.CreateIterator(); It; ++It)
    {
        AActor* Threat = It.Key().Get();
        if (!Threat || !IsThreatActor(Threat))
        {
            It.RemoveCurrent();
        }
    }

    for (AActor* Threat : VisibleThreats)
    {
        if (Threat)
        {
            RememberedThreatLocations.FindOrAdd(Threat) = Threat->GetActorLocation();
        }
    }
}

void FCMSacrificeThreatTracker::GatherRememberedLocations(TArray<FVector>& OutLocations) const
{
    OutLocations.Reset();
    OutLocations.Reserve(RememberedThreatLocations.Num());

    for (const TPair<TWeakObjectPtr<AActor>, FVector>& Pair : RememberedThreatLocations)
    {
        if (Pair.Key.IsValid())
        {
            OutLocations.Add(Pair.Value);
        }
    }
}

void FCMSacrificeThreatTracker::ClearMemory()
{
    RememberedThreatLocations.Reset();
}

// 살아 있는 플레이어나 활성 ThreatSource만 Sacrifice의 위협 대상으로 인정한다.
bool FCMSacrificeThreatTracker::IsThreatActor(const AActor* Candidate) const
{
    if (!Candidate || Candidate == Sacrifice.Get())
    {
        return false;
    }
    if (const ACMChimera* Chimera = Cast<ACMChimera>(Candidate))
    {
        return !Chimera->AreAllSegmentsDead();
    }
    if (!Candidate->GetClass()->ImplementsInterface(UCMThreatSource::StaticClass()))
    {
        return false;
    }

    const ICMThreatSource* ThreatSource = Cast<ICMThreatSource>(Candidate);

    return ThreatSource && ThreatSource->IsThreatActive();
}

bool FCMSacrificeThreatTracker::IsInAnimatedVisionCone(const AActor* Candidate) const
{
    const ACMSacrificeCharacter* SacrificeCharacter = Sacrifice.Get();
    if (!SacrificeCharacter || !Candidate)
    {
        return false;
    }
    return FCMSacrificeRules::IsPointInsideVisionCone(SacrificeCharacter->GetSacrificeVisionOrigin(), SacrificeCharacter->GetSacrificeVisionForward(), VisionDistanceCm, VisionHalfYawDegrees, VisionUpDegrees, VisionDownDegrees, Candidate->GetActorLocation());
}

bool FCMSacrificeThreatTracker::HasLineOfSightToThreat(const AActor* Candidate) const
{
    const ACMSacrificeCharacter* SacrificeCharacter = Sacrifice.Get();
    UWorld* World = SacrificeCharacter ? SacrificeCharacter->GetWorld() : nullptr;
    if (!SacrificeCharacter || !Candidate || !World)
    {
        return false;
    }

    FCollisionQueryParams Params(SCENE_QUERY_STAT(SacrificeSight), false);
    Params.AddIgnoredActor(SacrificeCharacter);
    Params.AddIgnoredActor(Candidate);

    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);

    return !World->LineTraceTestByObjectType(SacrificeCharacter->GetSacrificeVisionOrigin(), Candidate->GetActorLocation(), ObjectParams, Params);
}

// 근접 감지 또는 애니메이션 시야각과 정적 장애물 가시성을 함께 검사한다.
bool FCMSacrificeThreatTracker::IsThreatDetected(const AActor* Candidate) const
{
    const ACMSacrificeCharacter* SacrificeCharacter = Sacrifice.Get();
    if (!SacrificeCharacter || !IsThreatActor(Candidate))
    {
        return false;
    }

    const bool bInsideProximity = FVector::DistSquared(SacrificeCharacter->GetActorLocation(), Candidate->GetActorLocation()) <= FMath::Square(ProximityDetectionDistanceCm);

    return (bInsideProximity || IsInAnimatedVisionCone(Candidate)) && HasLineOfSightToThreat(Candidate);
}

void FCMSacrificeThreatTracker::DrawDebug() const
{
#if ENABLE_DRAW_DEBUG && !UE_BUILD_SHIPPING
    const ACMSacrificeCharacter* SacrificeCharacter = Sacrifice.Get();
    UWorld* World = SacrificeCharacter ? SacrificeCharacter->GetWorld() : nullptr;
    if (!CMSacrificeSightDebug::bDrawSight || !SacrificeCharacter || !World || World->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    const FVector Origin = SacrificeCharacter->GetSacrificeVisionOrigin();
    const FVector Forward = SacrificeCharacter->GetSacrificeVisionForward().GetSafeNormal();
    const FRotator ViewRotation = Forward.Rotation();
    auto DirectionAt = [&ViewRotation](const float Yaw, const float Pitch)
    {
        return ViewRotation.RotateVector(FRotator(Pitch, Yaw, 0.0f).Vector());
    };

    DrawDebugSphere(World, Origin, 8.0f, 8, FColor::Yellow, false, CMSacrificeSightDebug::DebugLifetime);
    DrawDebugCircle(World, SacrificeCharacter->GetActorLocation(), ProximityDetectionDistanceCm, 32, FColor::Green, false, CMSacrificeSightDebug::DebugLifetime, 0, 1.0f, FVector::ForwardVector, FVector::RightVector, false);
    DrawDebugDirectionalArrow(World, Origin, Origin + Forward * VisionDistanceCm, 20.0f, FColor::White, false, CMSacrificeSightDebug::DebugLifetime, 0, 1.5f);

    constexpr int32 ArcSegments = 12;
    FVector Previous = Origin + DirectionAt(-VisionHalfYawDegrees, 0.0f) * VisionDistanceCm;
    DrawDebugLine(World, Origin, Previous, FColor::Cyan, false, CMSacrificeSightDebug::DebugLifetime, 0, 1.0f);

    for (int32 Index = 1; Index <= ArcSegments; ++Index)
    {
        const float Alpha = static_cast<float>(Index) / ArcSegments;
        const float Yaw = FMath::Lerp(-VisionHalfYawDegrees, VisionHalfYawDegrees, Alpha);
        const FVector Current = Origin + DirectionAt(Yaw, 0.0f) * VisionDistanceCm;
        DrawDebugLine(World, Previous, Current, FColor::Cyan, false, CMSacrificeSightDebug::DebugLifetime, 0, 1.0f);
        Previous = Current;
    }

    DrawDebugLine(World, Origin, Previous, FColor::Cyan, false, CMSacrificeSightDebug::DebugLifetime, 0, 1.0f);

    for (const float Yaw : {-VisionHalfYawDegrees, VisionHalfYawDegrees})
    {
        DrawDebugLine(World, Origin, Origin + DirectionAt(Yaw, VisionUpDegrees) * VisionDistanceCm, FColor::Cyan, false, CMSacrificeSightDebug::DebugLifetime, 0, 1.0f);
        DrawDebugLine(World, Origin, Origin + DirectionAt(Yaw, -VisionDownDegrees) * VisionDistanceCm, FColor::Cyan, false, CMSacrificeSightDebug::DebugLifetime, 0, 1.0f);
    }
#endif
}
