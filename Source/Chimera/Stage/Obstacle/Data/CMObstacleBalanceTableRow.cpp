#include "Stage/Obstacle/Data/CMObstacleBalanceTableRow.h"

FCMResolvedObstacleBalance CMObstacleBalance::Resolve(
    const FCMObstacleDamageBalanceTableRow* DamageRow,
    const FCMObstacleBalanceSelection& Selection,
    const float StageDamageMultiplier)
{
    FCMResolvedObstacleBalance Result;
    // Custom damage is a complete per-instance override and must not require a
    // damage-table row merely to make the selection valid.
    Result.bValid = DamageRow
        || Selection.DamageMode == ECMObstacleDamageMode::Custom;
    const float SheetDamage = DamageRow
        ? (Selection.DamageMode == ECMObstacleDamageMode::High
            ? DamageRow->HighDamage : DamageRow->DefaultDamage)
        : 0.0f;
    Result.Damage = Selection.DamageMode == ECMObstacleDamageMode::Custom
        ? FMath::Max(Selection.CustomDamage, 0.0f)
        : FMath::Max(SheetDamage * StageDamageMultiplier, 0.0f);
    return Result;
}
