#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CMObstacleBalanceTableRow.generated.h"

UENUM(BlueprintType)
enum class ECMObstacleBalanceType : uint8
{
    Laser,
    Blade,
    Fan,
    FloorSurface,
    AirborneZone,
    Turret
};

UENUM(BlueprintType)
enum class ECMObstacleDamageMode : uint8
{
    Default,
    High,
    Custom
};

USTRUCT(BlueprintType)
struct CHIMERA_API FCMObstacleDamageBalanceTableRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ID;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ECMObstacleBalanceType ObstacleType = ECMObstacleBalanceType::Laser;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DefaultDamage = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float HighDamage = 0.0f;
};

USTRUCT(BlueprintType)
struct CHIMERA_API FCMStageObstacleBalanceTableRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ID;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0"))
    float DamageMultiplier = 1.0f;
};

USTRUCT(BlueprintType)
struct CHIMERA_API FCMObstacleBalanceSelection
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UDataTable> DamageBalanceTable;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName DamageBalanceRow;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) ECMObstacleDamageMode DamageMode = ECMObstacleDamageMode::Default;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0", EditCondition="DamageMode == ECMObstacleDamageMode::Custom"))
    float CustomDamage = 0.0f;
};

USTRUCT(BlueprintType)
struct CHIMERA_API FCMResolvedObstacleBalance
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) bool bValid = false;
    UPROPERTY(BlueprintReadOnly) float Damage = 0.0f;
};

namespace CMObstacleBalance
{
    CHIMERA_API FCMResolvedObstacleBalance Resolve(
        const FCMObstacleDamageBalanceTableRow* DamageRow,
        const FCMObstacleBalanceSelection& Selection,
        float StageDamageMultiplier);
}
