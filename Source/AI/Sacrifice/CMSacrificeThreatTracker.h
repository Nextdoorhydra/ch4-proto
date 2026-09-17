#pragma once

#include "CoreMinimal.h"

class ACMSacrificeCharacter;

/** 희생 AI의 위협 감지와 마지막 확인 위치를 컨트롤러 상태 전이와 분리해 관리한다. */
class AI_API FCMSacrificeThreatTracker
{
public:
    void Initialize(ACMSacrificeCharacter* InSacrifice);
    void Reset();
    void GatherVisibleThreats(TArray<AActor*>& OutThreats) const;
    void UpdateMemory(const TArray<AActor*>& VisibleThreats);
    void GatherRememberedLocations(TArray<FVector>& OutLocations) const;
    void ClearMemory();
    void DrawDebug() const;

private:
    bool IsThreatActor(const AActor* Candidate) const;
    bool IsInAnimatedVisionCone(const AActor* Candidate) const;
    bool HasLineOfSightToThreat(const AActor* Candidate) const;
    bool IsThreatDetected(const AActor* Candidate) const;

    TWeakObjectPtr<ACMSacrificeCharacter> Sacrifice;
    TMap<TWeakObjectPtr<AActor>, FVector> RememberedThreatLocations;

    static constexpr float VisionDistanceCm = 1500.0f;
    static constexpr float VisionHalfYawDegrees = 50.0f;
    static constexpr float VisionUpDegrees = 50.0f;
    static constexpr float VisionDownDegrees = 70.0f;
    static constexpr float ProximityDetectionDistanceCm = 200.0f;
};
