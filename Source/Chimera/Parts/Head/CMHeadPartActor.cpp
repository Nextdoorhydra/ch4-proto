#include "Parts/Head/CMHeadPartActor.h"

#include "Data/Head/CMHeadTableRow.h"
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

void ACMHeadPartActor::OnAttachedToPartSlot_Implementation(
    UCMPartSlotComponent* PartSlot
)
{
    Super::OnAttachedToPartSlot_Implementation(PartSlot);

    if (HasAuthority())
    {
        ApplyHeadData();
        VisionComponent->SetVisionActive(true);
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

void ACMHeadPartActor::ApplyHeadData()
{
    if (!HeadDataRow.DataTable || HeadDataRow.RowName.IsNone())
    {
        return;
    }

    const FCMHeadTableRow* HeadData = HeadDataRow.GetRow<FCMHeadTableRow>(
        TEXT("ACMHeadPartActor::ApplyHeadData")
    );
    if (HeadData)
    {
        VisionComponent->ConfigureVision(
            HeadData->VisionAngle,
            HeadData->VisionRange
        );
    }
}
