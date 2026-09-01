#include "Player/CMControlBody.h"

#include "Components/SceneComponent.h"
#include "GameMode/CMGameState.h"
#include "Player/CMChimera.h"
#include "Player/CMPartSlotComponent.h"
#include "Player/CMPlayerState.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraControlBody, Log, All);

ACMControlBody::ACMControlBody()
{
    // Possession과 RPC 소유권만 담당하므로 프레임 단위 작업이 전혀 필요 없다.
    PrimaryActorTick.bCanEverTick = false;

    // 서버가 플레이어마다 하나를 만들고 Possession 관계를 복제한다.
    // 위치는 게임 판정에 사용하지 않으므로 이동도 복제하지 않는다.
    bReplicates = true;
    SetReplicateMovement(false);
    // 공용 몸통의 슬롯 마커가 모든 플레이어의 할당을 표시하므로
    // ControlSlots는 소유자뿐 아니라 모든 클라이언트가 볼 수 있어야 한다.
    bOnlyRelevantToOwner = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    for (FCMPartSlotAddress& PressedPartSlot : PressedPartSlots)
    {
        PressedPartSlot = FCMPartSlotAddress();
    }
}

void ACMControlBody::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACMControlBody, ControlSlots);
    DOREPLIFETIME(ACMControlBody, OwnedSegmentIndex);
    DOREPLIFETIME(ACMControlBody, bControlInputEnabled);
    DOREPLIFETIME(ACMControlBody, DisabledControlSlotMask);
}

void ACMControlBody::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogChimeraControlBody, Log,
        TEXT("[ControlBody Ready] Pawn=%s Role=%s LocallyControlled=%s"),
        *GetName(),
        HasAuthority() ? TEXT("Server") : TEXT("Client"),
        IsLocallyControlled() ? TEXT("true") : TEXT("false"));
}

void ACMControlBody::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    UE_LOG(LogChimeraControlBody, Log,
        TEXT("[Server Possession] Controller=%s now owns ControlBody=%s"),
        *GetNameSafe(NewController),
        *GetName());
}

void ACMControlBody::UnPossessed()
{
    // 소유자가 사라진 뒤에도 Shared Chimera에 눌림 비트가 남지 않게 한다.
    ClearPressedControlSlots();
    Super::UnPossessed();
}

void ACMControlBody::OnRep_Controller()
{
    Super::OnRep_Controller();

    UE_LOG(LogChimeraControlBody, Log,
        TEXT("[Client Possession] Controller=%s received ControlBody=%s"),
        *GetNameSafe(GetController()),
        *GetName());
}

void ACMControlBody::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    OnPlayerStateChanged.Broadcast();
}

ACMChimera* ACMControlBody::GetSharedChimera() const
{
    const ACMGameState* GameState = GetWorld()
        ? GetWorld()->GetGameState<ACMGameState>()
        : nullptr;
    return GameState ? GameState->SharedChimera : nullptr;
}

void ACMControlBody::SetControlSlotPressed(
    int32 SlotIndex,
    bool bPressed,
    bool bReverseMovement
)
{
    if (!IsLocallyControlled()
        || !IsControlSlotEnabled(SlotIndex)
        || SlotIndex < 0
        || SlotIndex >= CMControl::MaxKeysPerPlayer)
    {
        return;
    }

    const uint8 SlotBit = static_cast<uint8>(1u << SlotIndex);
    const bool bWasPressed = (LocalPressedControlSlotMask & SlotBit) != 0;
    if (bWasPressed != bPressed)
    {
        if (bPressed)
        {
            LocalPressedControlSlotMask |= SlotBit;
        }
        else
        {
            LocalPressedControlSlotMask &= ~SlotBit;
        }
        OnControlInputChanged.Broadcast(SlotIndex, bPressed);
    }

    ServerSetControlSlotPressed(
        SlotIndex,
        bPressed,
        bReverseMovement
    );
}

void ACMControlBody::RequestAttachPartToControlSlot(
    int32 SlotIndex,
    AActor* PartActor
)
{
    if (!IsLocallyControlled()
        || !IsControlSlotEnabled(SlotIndex)
        || SlotIndex < 0
        || SlotIndex >= CMControl::MaxKeysPerPlayer
        || !IsValid(PartActor))
    {
        return;
    }

    ServerRequestAttachPartToControlSlot(SlotIndex, PartActor);
}

void ACMControlBody::RequestDetachPartFromControlSlot(int32 SlotIndex)
{
    if (!IsLocallyControlled()
        || !IsControlSlotEnabled(SlotIndex)
        || SlotIndex < 0
        || SlotIndex >= CMControl::MaxKeysPerPlayer)
    {
        return;
    }

    ServerRequestDetachPartFromControlSlot(SlotIndex);
}

void ACMControlBody::ServerRequestAttachPartToControlSlot_Implementation(
    int32 SlotIndex,
    AActor* PartActor
)
{
    if (!IsControlSlotEnabled(SlotIndex) || !IsValid(PartActor))
    {
        return;
    }

    const FCMPartSlotAddress PartSlotAddress =
        GetPartSlotAddressForControlSlot(SlotIndex);
    ACMChimera* SharedChimera = GetSharedChimera();
    UCMPartSlotComponent* PartSlot = SharedChimera
        ? SharedChimera->GetPartSlotComponent(PartSlotAddress)
        : nullptr;
    if (!PartSlot
        || PartSlot->HasAttachedPart()
        || PartActor->GetAttachParentActor())
    {
        UE_LOG(LogChimeraControlBody, Warning,
            TEXT("[Attach Request Rejected] ControlBody=%s ControlSlot=%d has no available physical PartSlot."),
            *GetName(),
            SlotIndex);
        return;
    }

    const float AttachDistanceSquared = FVector::DistSquared(
        PartSlot->GetComponentLocation(),
        PartActor->GetActorLocation()
    );
    if (AttachDistanceSquared
        > FMath::Square(MaximumPartAttachDistance))
    {
        UE_LOG(LogChimeraControlBody, Warning,
            TEXT("[Attach Request Rejected] Part=%s is %.1fuu from Slot=(%d,%d); maximum is %.1fuu."),
            *GetNameSafe(PartActor),
            FMath::Sqrt(AttachDistanceSquared),
            PartSlotAddress.SegmentIndex,
            PartSlotAddress.PartSlotIndex,
            MaximumPartAttachDistance);
        return;
    }

    const bool bAttached = SharedChimera->AttachPartToSlot(
        PartSlotAddress,
        PartActor
    );
    UE_LOG(LogChimeraControlBody, Log,
        TEXT("[Attach Request] ControlBody=%s ControlSlot=%d Slot=(%d,%d) Part=%s Result=%s"),
        *GetName(),
        SlotIndex,
        PartSlotAddress.SegmentIndex,
        PartSlotAddress.PartSlotIndex,
        *GetNameSafe(PartActor),
        bAttached ? TEXT("Attached") : TEXT("Rejected"));
}

void ACMControlBody::ServerRequestDetachPartFromControlSlot_Implementation(
    int32 SlotIndex
)
{
    if (!IsControlSlotEnabled(SlotIndex))
    {
        return;
    }

    const FCMPartSlotAddress PartSlotAddress =
        GetPartSlotAddressForControlSlot(SlotIndex);
    ACMChimera* SharedChimera = GetSharedChimera();
    if (!SharedChimera
        || !CMControl::IsValidPartSlot(PartSlotAddress))
    {
        return;
    }

    if (SlotIndex >= 0 && SlotIndex < CMControl::MaxKeysPerPlayer)
    {
        FCMPartSlotAddress& PressedPartSlot = PressedPartSlots[SlotIndex];
        if (CMControl::IsValidPartSlot(PressedPartSlot))
        {
            SharedChimera->SetPartSlotPressed(PressedPartSlot, false);
            PressedPartSlot = FCMPartSlotAddress();
        }
    }

    AActor* DetachedPart =
        SharedChimera->DetachPartFromSlot(PartSlotAddress);
    UE_LOG(LogChimeraControlBody, Log,
        TEXT("[Detach Request] ControlBody=%s ControlSlot=%d Slot=(%d,%d) Part=%s Result=%s"),
        *GetName(),
        SlotIndex,
        PartSlotAddress.SegmentIndex,
        PartSlotAddress.PartSlotIndex,
        *GetNameSafe(DetachedPart),
        DetachedPart ? TEXT("Detached") : TEXT("Empty"));
}

void ACMControlBody::ServerSetControlSlotPressed_Implementation(
    int32 SlotIndex,
    bool bPressed,
    bool bReverseMovement
)
{
    if (!IsControlSlotEnabled(SlotIndex)
        || SlotIndex < 0
        || SlotIndex >= CMControl::MaxKeysPerPlayer)
    {
        return;
    }

    ACMChimera* SharedChimera = GetSharedChimera();
    ACMPlayerState* CMPlayerState = GetPlayerState<ACMPlayerState>();
    if (!SharedChimera || !CMPlayerState)
    {
        return;
    }

    FCMPartSlotAddress& PressedPartSlot = PressedPartSlots[SlotIndex];
    if (!bPressed)
    {
        if (CMControl::IsValidPartSlot(PressedPartSlot))
        {
            const FCMPartSlotAddress ReleasedPartSlot = PressedPartSlot;
            const bool bActivateOnRelease =
                SharedChimera->ShouldActivateBasicArmOnRelease(
                    ReleasedPartSlot);
            SharedChimera->SetPartSlotPressed(
                ReleasedPartSlot,
                false
            );
            if (bActivateOnRelease)
            {
                SharedChimera->ActivatePartSlot(
                    ReleasedPartSlot,
                    CMPlayerState,
                    false
                );
            }
        }
        PressedPartSlot = FCMPartSlotAddress();
        return;
    }

    const FCMPartSlotAddress PartSlotAddress =
        GetPartSlotAddressForControlSlot(SlotIndex);
    if (!CMControl::IsValidPartSlot(PartSlotAddress))
    {
        return;
    }

    if (CMControl::IsValidPartSlot(PressedPartSlot)
        && PressedPartSlot != PartSlotAddress)
    {
        SharedChimera->SetPartSlotPressed(PressedPartSlot, false);
    }

    PressedPartSlot = PartSlotAddress;
    SharedChimera->SetPartSlotPressed(PartSlotAddress, true);
    if (!SharedChimera->IsBasicArmPartSlot(PartSlotAddress))
    {
        SharedChimera->ActivatePartSlot(
            PartSlotAddress,
            CMPlayerState,
            bReverseMovement
        );
    }
}

void ACMControlBody::SetControlSlots(
    const TArray<FCMPartSlotAddress>& NewControlSlots
)
{
    if (!HasAuthority() || ControlSlots == NewControlSlots)
    {
        return;
    }

    // 재할당 전에 이전 슬롯의 눌림 상태부터 Shared Chimera에서 제거한다.
    ClearPressedControlSlots();
    ControlSlots = NewControlSlots;

    DisabledControlSlotMask = 0;
    bControlInputEnabled = true;
    if (const ACMChimera* SharedChimera = GetSharedChimera())
    {
        if (OwnedSegmentIndex >= 0
            && !SharedChimera->IsSegmentAlive(OwnedSegmentIndex))
        {
            DisabledControlSlotMask |= 0x03; // Q/W
        }
        if (OwnedSegmentIndex >= 0
            && !SharedChimera->IsSegmentAlive(OwnedSegmentIndex + 1))
        {
            DisabledControlSlotMask |= 0x0C; // E/R
        }
    }
    bControlInputEnabled = GetEnabledControlCount() > 0;
    OnRep_ControlSlots();
    OnRep_ControlState();
    ForceNetUpdate();
}

FCMPartSlotAddress ACMControlBody::GetPartSlotAddressForControlSlot(
    int32 SlotIndex
) const
{
    return ControlSlots.IsValidIndex(SlotIndex)
        ? ControlSlots[SlotIndex]
        : FCMPartSlotAddress();
}

int32 ACMControlBody::GetAssignedControlCount() const
{
    return ControlSlots.Num();
}

int32 ACMControlBody::GetEnabledControlCount() const
{
    int32 EnabledCount = 0;
    for (int32 SlotIndex = 0;
        SlotIndex < ControlSlots.Num()
            && SlotIndex < CMControl::MaxKeysPerPlayer;
        ++SlotIndex)
    {
        if (IsControlSlotEnabled(SlotIndex))
        {
            ++EnabledCount;
        }
    }
    return EnabledCount;
}

bool ACMControlBody::IsControlSlotEnabled(int32 SlotIndex) const
{
    return bControlInputEnabled
        && ControlSlots.IsValidIndex(SlotIndex)
        && SlotIndex < CMControl::MaxKeysPerPlayer
        && (DisabledControlSlotMask & (1u << SlotIndex)) == 0;
}

const TArray<FCMPartSlotAddress>& ACMControlBody::GetControlSlots() const
{
    return ControlSlots;
}

void ACMControlBody::SetOwnedSegmentIndex(int32 NewSegmentIndex)
{
    if (!HasAuthority() || OwnedSegmentIndex == NewSegmentIndex)
    {
        return;
    }

    ClearPressedControlSlots();
    OwnedSegmentIndex = NewSegmentIndex;
    DisabledControlSlotMask = 0;
    bControlInputEnabled = true;
    if (ACMPlayerState* CMPlayerState = GetPlayerState<ACMPlayerState>())
    {
        CMPlayerState->SetParticipationState(
            ECMPlayerParticipationState::Active);
    }
    OnRep_ControlState();
    ForceNetUpdate();

    UE_LOG(LogChimeraControlBody, Log,
        TEXT("[Owned Segment] ControlBody=%s owns Segment=%d"),
        *GetName(),
        OwnedSegmentIndex);
}

int32 ACMControlBody::GetOwnedSegmentIndex() const
{
    return OwnedSegmentIndex;
}

bool ACMControlBody::IsControlInputEnabled() const
{
    return bControlInputEnabled;
}

void ACMControlBody::HandleSegmentDestroyed(int32 DestroyedSegmentIndex)
{
    if (!HasAuthority() || DestroyedSegmentIndex < 0)
    {
        return;
    }

    uint8 NewlyDisabledMask = 0;
    if (DestroyedSegmentIndex == OwnedSegmentIndex)
    {
        NewlyDisabledMask = 0x03; // Q/W
    }
    else if (DestroyedSegmentIndex == OwnedSegmentIndex + 1)
    {
        NewlyDisabledMask = 0x0C; // E/R
    }
    NewlyDisabledMask &= ~DisabledControlSlotMask;
    if (NewlyDisabledMask == 0)
    {
        return;
    }

    ACMChimera* SharedChimera = GetSharedChimera();
    for (int32 SlotIndex = 0;
        SlotIndex < CMControl::MaxKeysPerPlayer;
        ++SlotIndex)
    {
        if ((NewlyDisabledMask & (1u << SlotIndex)) == 0)
        {
            continue;
        }

        FCMPartSlotAddress& PressedPartSlot = PressedPartSlots[SlotIndex];
        if (SharedChimera && CMControl::IsValidPartSlot(PressedPartSlot))
        {
            SharedChimera->SetPartSlotPressed(PressedPartSlot, false);
        }
        PressedPartSlot = FCMPartSlotAddress();
    }

    DisabledControlSlotMask |= NewlyDisabledMask;
    bControlInputEnabled = GetEnabledControlCount() > 0;
    if (!bControlInputEnabled)
    {
        if (ACMPlayerState* CMPlayerState = GetPlayerState<ACMPlayerState>())
        {
            CMPlayerState->SetParticipationState(
                ECMPlayerParticipationState::Defeated);
        }
    }
    OnRep_ControlState();
    ForceNetUpdate();

    if (bControlInputEnabled)
    {
        UE_LOG(LogChimeraControlBody, Log,
            TEXT("[Segment Controls Lost] ControlBody=%s Segment=%d DisabledMask=0x%02X RemainingControls=%d"),
            *GetName(),
            DestroyedSegmentIndex,
            DisabledControlSlotMask,
            GetEnabledControlCount());
    }
    else
    {
        UE_LOG(LogChimeraControlBody, Warning,
            TEXT("[Player Control Defeated] ControlBody=%s Segment=%d DisabledMask=0x%02X; all Q/W/E/R input is disabled."),
            *GetName(),
            DestroyedSegmentIndex,
            DisabledControlSlotMask);
    }
}

void ACMControlBody::RestoreControlsAfterRespawn()
{
    if (!HasAuthority())
    {
        return;
    }

    ClearPressedControlSlots();
    DisabledControlSlotMask = 0;
    bControlInputEnabled = true;
    if (ACMPlayerState* CMPlayerState = GetPlayerState<ACMPlayerState>())
    {
        CMPlayerState->SetParticipationState(
            ECMPlayerParticipationState::Active);
    }
    OnRep_ControlState();
    ForceNetUpdate();
}

void ACMControlBody::ClearPressedControlSlots()
{
    if (!HasAuthority())
    {
        return;
    }

    ACMChimera* SharedChimera = GetSharedChimera();
    for (FCMPartSlotAddress& PressedPartSlot : PressedPartSlots)
    {
        if (SharedChimera
            && CMControl::IsValidPartSlot(PressedPartSlot))
        {
            SharedChimera->SetPartSlotPressed(
                PressedPartSlot,
                false
            );
        }
        PressedPartSlot = FCMPartSlotAddress();
    }
}

void ACMControlBody::OnRep_ControlSlots()
{
    OnControlSlotsChanged.Broadcast();

    UE_LOG(LogChimeraControlBody, Log,
        TEXT("[Control Slots] ControlBody=%s received %d slot(s)."),
        *GetName(),
        ControlSlots.Num());
}

void ACMControlBody::OnRep_ControlState()
{
    OnControlSlotsChanged.Broadcast();

    UE_LOG(LogChimeraControlBody, Log,
        TEXT("[Control State] ControlBody=%s OwnedSegments=%d,%d DisabledMask=0x%02X InputEnabled=%s"),
        *GetName(),
        OwnedSegmentIndex,
        OwnedSegmentIndex >= 0
            ? OwnedSegmentIndex + 1
            : INDEX_NONE,
        DisabledControlSlotMask,
        bControlInputEnabled ? TEXT("true") : TEXT("false"));
}
