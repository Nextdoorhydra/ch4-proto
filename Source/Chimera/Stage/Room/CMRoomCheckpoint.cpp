#include "Stage/Room/CMRoomCheckpoint.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

ACMRoomCheckpoint::ACMRoomCheckpoint()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    SpawnDirection = CreateDefaultSubobject<UArrowComponent>(TEXT("SpawnDirection"));
    SpawnDirection->SetupAttachment(SceneRoot);
}

#if WITH_EDITOR
EDataValidationResult ACMRoomCheckpoint::IsDataValid(
    FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);
    if (RoomId.IsNone())
    {
        Context.AddWarning(FText::FromString(
            TEXT("RoomCheckpoint의 RoomId가 비어 있습니다.")));
    }
    return Result;
}
#endif
