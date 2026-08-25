#include "Parts/Head/CMHeadPartActor.h"

#include "AsyncLoad/CMStageLoadCoordinatorSubsystem.h"
#include "AsyncLoad/CMStageLoadLog.h"
#include "Data/Head/CMHeadDefinition.h"
#include "Parts/Head/CMVisionComponent.h"

ACMHeadPartActor::ACMHeadPartActor()
{
    PartType = ECMPartSlotType::Head;
    GrantedAbilityClass = nullptr;

    VisionComponent = CreateDefaultSubobject<UCMVisionComponent>(
        TEXT("VisionComponent")
    );
    VisionComponent->SetupAttachment(SceneRoot);
}

void ACMHeadPartActor::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        VisionComponent->SetVisionActive(false);
    }

    if (UGameInstance* GameInstance = GetWorld()
        ? GetWorld()->GetGameInstance() : nullptr)
    {
        if (UCMStageLoadCoordinatorSubsystem* Coordinator =
            GameInstance->GetSubsystem<UCMStageLoadCoordinatorSubsystem>())
        {
            Coordinator->OnLoadGroupFinished.AddUniqueDynamic(
                this, &ThisClass::HandleLoadGroupFinished);
        }
    }
    RefreshDefinitionState();
}

void ACMHeadPartActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UGameInstance* GameInstance = GetWorld()
        ? GetWorld()->GetGameInstance() : nullptr)
    {
        if (UCMStageLoadCoordinatorSubsystem* Coordinator =
            GameInstance->GetSubsystem<UCMStageLoadCoordinatorSubsystem>())
        {
            Coordinator->OnLoadGroupFinished.RemoveAll(this);
        }
    }
    Super::EndPlay(EndPlayReason);
}

void ACMHeadPartActor::OnAttachedToPartSlot_Implementation(
    UCMPartSlotComponent* PartSlot
)
{
    Super::OnAttachedToPartSlot_Implementation(PartSlot);

    if (HasAuthority())
    {
        if (bDefinitionReady)
        {
            ApplyLoadedDefinition();
            VisionComponent->SetVisionActive(true);
        }
        else
        {
            VisionComponent->SetVisionActive(false);
        }
    }
}

void ACMHeadPartActor::OnDetachedFromPartSlot_Implementation(
    UCMPartSlotComponent* PartSlot
)
{
    if (HasAuthority())
    {
        VisionComponent->SetVisionActive(false);
    }

    Super::OnDetachedFromPartSlot_Implementation(PartSlot);
}

UCMVisionComponent* ACMHeadPartActor::GetVisionComponent() const
{
    return VisionComponent;
}

void ACMHeadPartActor::RefreshDefinitionState()
{
    if (Definition.IsNull() || bDefinitionReady || bDefinitionFailed)
    {
        if (Definition.IsNull())
        {
            MarkDefinitionFailed(TEXT("Definition is empty"));
        }
        return;
    }
    if (LoadGroupId.IsNone())
    {
        MarkDefinitionFailed(TEXT("LoadGroupId is empty"));
        return;
    }

    UGameInstance* GameInstance = GetWorld()
        ? GetWorld()->GetGameInstance() : nullptr;
    const UCMStageLoadCoordinatorSubsystem* Coordinator = GameInstance
        ? GameInstance->GetSubsystem<UCMStageLoadCoordinatorSubsystem>()
        : nullptr;
    if (!Coordinator)
    {
        MarkDefinitionFailed(TEXT("StageLoadCoordinator is missing"));
        return;
    }

    const ECMStageLoadGroupState State = Coordinator->GetLoadGroupState(
        LoadGroupId);
    if (State == ECMStageLoadGroupState::Ready)
    {
        TryResolveLoadedDefinition();
    }
    else if (State == ECMStageLoadGroupState::Failed
        || State == ECMStageLoadGroupState::Released)
    {
        MarkDefinitionFailed(TEXT("LoadGroup is not available"));
    }
}

void ACMHeadPartActor::HandleLoadGroupFinished(
    FName FinishedLoadGroupId,
    EAsyncLoadResult Result,
    bool bReleasedImmediately
)
{
    if (FinishedLoadGroupId != LoadGroupId
        || bDefinitionReady
        || bDefinitionFailed)
    {
        return;
    }
    if (Result != EAsyncLoadResult::Succeeded || bReleasedImmediately)
    {
        MarkDefinitionFailed(
            TEXT("LoadGroup failed or was released immediately"));
        return;
    }
    TryResolveLoadedDefinition();
}

bool ACMHeadPartActor::TryResolveLoadedDefinition()
{
    if (!Definition.Get())
    {
        MarkDefinitionFailed(
            TEXT("Definition was not loaded by the assigned LoadGroup"));
        return false;
    }

    bDefinitionReady = true;
    bDefinitionFailed = false;
    if (HasAuthority())
    {
        ApplyLoadedDefinition();
        VisionComponent->SetVisionActive(IsAttached());
    }
    return true;
}

void ACMHeadPartActor::MarkDefinitionFailed(const TCHAR* Reason)
{
    if (bDefinitionFailed)
    {
        return;
    }

    bDefinitionReady = false;
    bDefinitionFailed = true;
    if (HasAuthority())
    {
        VisionComponent->SetVisionActive(false);
    }
    UE_LOG(LogChimeraStageLoad, Error,
        TEXT("Head Definition failed. Actor=%s Definition=%s LoadGroup=%s Reason=%s"),
        *GetName(),
        *Definition.ToSoftObjectPath().ToString(),
        *LoadGroupId.ToString(),
        Reason);
}

void ACMHeadPartActor::ApplyLoadedDefinition()
{
    if (const UCMHeadDefinition* LoadedDefinition = Definition.Get())
    {
        VisionComponent->ConfigureVision(
            LoadedDefinition->VisionAngle,
            LoadedDefinition->VisionRange,
            LoadedDefinition->NearVisionRadius
        );
    }
}
