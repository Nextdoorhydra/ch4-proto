#include "Stage/Test/CMTestAreaManager.h"

#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Net/UnrealNetwork.h"
#include "Player/CMChimera.h"
#include "Stage/Mechanism/CMStageMechanismBase.h"
#include "Stage/Obstacle/CMStageObstacleBase.h"
#include "Stage/Test/CMTestAreaDefinition.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraTestArea, Log, All);

ACMTestAreaManager::ACMTestAreaManager()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bAlwaysRelevant = true;
}

// 현재 Test Area ID를 모든 클라이언트 UI에 복제
void ACMTestAreaManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ThisClass, CurrentAreaId);
}

// Definition과 레벨 PlayerStart 배치 오류를 시작 시 로그로 확인
void ACMTestAreaManager::BeginPlay()
{
    Super::BeginPlay();
    if (!AreaDefinition)
    {
        UE_LOG(LogChimeraTestArea, Error, TEXT("TestAreaManager에 AreaDefinition이 없습니다."));
        return;
    }

    for (const FCMTestAreaEntry& Area : AreaDefinition->Areas)
    {
        const APlayerStart* PlayerStart = FindPlayerStart(Area.StartTag);
        if (!PlayerStart)
        {
            UE_LOG(LogChimeraTestArea, Error,
                TEXT("Test Area PlayerStart를 찾지 못했습니다. AreaId=%s StartTag=%s"),
                *Area.AreaId.ToString(), *Area.StartTag.ToString());
        }
        else if (!IsPlayerStartInAreaLevel(PlayerStart, Area))
        {
            UE_LOG(LogChimeraTestArea, Error,
                TEXT("PlayerStart가 Definition의 AreaLevel에 속하지 않습니다. AreaId=%s Start=%s AreaLevel=%s"),
                *Area.AreaId.ToString(), *GetNameSafe(PlayerStart),
                *Area.AreaLevel.ToSoftObjectPath().GetLongPackageName());
        }
    }
}

// Definition 배열 순서로 UI 생성용 구역 정보 반환
TArray<FCMTestAreaInfo> ACMTestAreaManager::GetAvailableAreas() const
{
    TArray<FCMTestAreaInfo> Result;
    if (!AreaDefinition)
    {
        return Result;
    }

    Result.Reserve(AreaDefinition->Areas.Num());
    for (const FCMTestAreaEntry& Area : AreaDefinition->Areas)
    {
        FCMTestAreaInfo& Info = Result.AddDefaulted_GetRef();
        Info.AreaId = Area.AreaId;
        Info.DisplayName = Area.DisplayName.IsEmpty()
            ? FText::FromName(Area.AreaId) : Area.DisplayName;
    }
    return Result;
}

// Definition 첫 구역의 StartTag와 일치하는 PlayerStart 반환
APlayerStart* ACMTestAreaManager::GetInitialPlayerStart() const
{
    return AreaDefinition && !AreaDefinition->Areas.IsEmpty()
        ? FindPlayerStart(AreaDefinition->Areas[0].StartTag)
        : nullptr;
}

// UI 요청 Area를 찾아 초기화 후 공용 키메라 이동
bool ACMTestAreaManager::TeleportToArea(FName AreaId)
{
    return HasAuthority() && TeleportChimeraToArea(FindSharedChimera(), AreaId);
}

// 완료 Area 다음 Definition 항목 또는 첫 번째 Area로 순환 이동
bool ACMTestAreaManager::AdvanceToNextArea(FName CompletedAreaId, ACMChimera* Chimera)
{
    if (!HasAuthority() || !IsValid(Chimera) || !AreaDefinition)
    {
        return false;
    }

    const FCMTestAreaEntry* NextArea = AreaDefinition->FindNextArea(CompletedAreaId);
    return NextArea && TeleportChimeraToArea(Chimera, NextArea->AreaId);
}

// 요청 Actor의 Sublevel과 일치하는 Definition 구역을 찾아 시작점으로 복귀
bool ACMTestAreaManager::RestartAreaForLevel(
    const ULevel* SourceLevel,
    ACMChimera* Chimera)
{
    if (!HasAuthority() || !SourceLevel || !IsValid(Chimera) || !AreaDefinition)
    {
        return false;
    }

    const FString SourceLevelPackage = UWorld::RemovePIEPrefix(
        SourceLevel->GetOutermost()->GetName());
    const FCMTestAreaEntry* Area = AreaDefinition->Areas.FindByPredicate(
        [&SourceLevelPackage](const FCMTestAreaEntry& Entry)
        {
            return Entry.AreaLevel.ToSoftObjectPath().GetLongPackageName()
                == SourceLevelPackage;
        });
    if (!Area)
    {
        UE_LOG(LogChimeraTestArea, Error,
            TEXT("KillZone이 속한 Sublevel이 TestAreaDefinition에 없습니다. Level=%s"),
            *SourceLevelPackage);
        return false;
    }
    return TeleportChimeraToArea(Chimera, Area->AreaId);
}

// PlayerStartTag가 일치하는 유일한 시작점 검색
APlayerStart* ACMTestAreaManager::FindPlayerStart(FName StartTag) const
{
    APlayerStart* Found = nullptr;
    for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
    {
        if (It->PlayerStartTag != StartTag)
        {
            continue;
        }
        if (Found)
        {
            UE_LOG(LogChimeraTestArea, Error,
                TEXT("중복 PlayerStartTag로 텔레포트할 수 없습니다. StartTag=%s"),
                *StartTag.ToString());
            return nullptr;
        }
        Found = *It;
    }
    return Found;
}

// PIE 패키지 접두사를 제외하고 PlayerStart 소속 Sublevel과 Definition의 AreaLevel 비교
bool ACMTestAreaManager::IsPlayerStartInAreaLevel(
    const APlayerStart* PlayerStart,
    const FCMTestAreaEntry& Area) const
{
    if (!PlayerStart || Area.AreaLevel.IsNull())
    {
        return false;
    }

    const FString ActualLevelPackage = UWorld::RemovePIEPrefix(
        PlayerStart->GetLevel()->GetOutermost()->GetName());
    const FString ExpectedLevelPackage =
        Area.AreaLevel.ToSoftObjectPath().GetLongPackageName();
    return ActualLevelPackage == ExpectedLevelPackage;
}

// 현재 서버 월드의 공용 키메라 검색
ACMChimera* ACMTestAreaManager::FindSharedChimera() const
{
    for (TActorIterator<ACMChimera> It(GetWorld()); It; ++It)
    {
        return *It;
    }
    return nullptr;
}

// 대상 PlayerStart와 같은 서브레벨의 장애물·Mechanism을 초기 상태로 복원
void ACMTestAreaManager::ResetArea(const APlayerStart* PlayerStart) const
{
    if (!PlayerStart)
    {
        return;
    }

    ULevel* TargetLevel = PlayerStart->GetLevel();
    for (TActorIterator<ACMStageObstacleBase> It(GetWorld()); It; ++It)
    {
        if (It->GetLevel() == TargetLevel)
        {
            It->ResetObstacle();
        }
    }
    for (TActorIterator<ACMStageMechanismBase> It(GetWorld()); It; ++It)
    {
        if (It->GetLevel() == TargetLevel)
        {
            It->ResetMechanism();
        }
    }
}

// Definition의 AreaId와 StartTag를 해석해 공용 키메라 전체 마디 이동
bool ACMTestAreaManager::TeleportChimeraToArea(ACMChimera* Chimera, FName AreaId)
{
    const FCMTestAreaEntry* Area = AreaDefinition ? AreaDefinition->FindArea(AreaId) : nullptr;
    APlayerStart* PlayerStart = Area ? FindPlayerStart(Area->StartTag) : nullptr;
    if (!IsValid(Chimera) || !Area || !IsValid(PlayerStart)
        || !IsPlayerStartInAreaLevel(PlayerStart, *Area))
    {
        UE_LOG(LogChimeraTestArea, Error,
            TEXT("Test Area 텔레포트 설정이 유효하지 않습니다. Chimera=%s AreaId=%s Start=%s"),
            *GetNameSafe(Chimera), *AreaId.ToString(), *GetNameSafe(PlayerStart));
        return false;
    }

    ResetArea(PlayerStart);
    if (!Chimera->TeleportAssembly(PlayerStart->GetActorTransform()))
    {
        return false;
    }

    CurrentAreaId = AreaId;
    OnTestAreaChanged.Broadcast(CurrentAreaId);
    ForceNetUpdate();
    UE_LOG(LogChimeraTestArea, Display,
        TEXT("Test Area로 이동했습니다. AreaId=%s StartTag=%s"),
        *AreaId.ToString(), *Area->StartTag.ToString());
    return true;
}

// 복제된 현재 Area를 클라이언트 UI에 전달
void ACMTestAreaManager::OnRep_CurrentAreaId()
{
    OnTestAreaChanged.Broadcast(CurrentAreaId);
}
