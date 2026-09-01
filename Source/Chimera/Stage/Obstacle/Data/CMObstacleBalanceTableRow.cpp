#include "Stage/Obstacle/Data/CMObstacleBalanceTableRow.h"

FCMResolvedObstacleBalance CMObstacleBalance::Resolve(
    const FCMObstacleDamageBalanceTableRow* DamageRow,
    const FCMObstacleStatusBalanceTableRow* StatusRow,
    const FCMObstacleBalanceSelection& Selection,
    const float StageDamageMultiplier)
{
    FCMResolvedObstacleBalance Result;
    // Custom damage is a complete per-instance override and must not require a
    // damage-table row merely to make the selection valid.
    Result.bValid = DamageRow || StatusRow
        || Selection.DamageMode == ECMObstacleDamageMode::Custom;
    const float SheetDamage = DamageRow
        ? (Selection.DamageMode == ECMObstacleDamageMode::High
            ? DamageRow->HighDamage : DamageRow->DefaultDamage)
        : 0.0f;
    Result.Damage = Selection.DamageMode == ECMObstacleDamageMode::Custom
        ? FMath::Max(Selection.CustomDamage, 0.0f)
        : FMath::Max(SheetDamage * StageDamageMultiplier, 0.0f);
    Result.Duration = Selection.DurationMode == ECMObstacleDurationMode::Custom
        ? FMath::Max(Selection.CustomDuration, 0.0f)
        : (StatusRow ? FMath::Max(StatusRow->DefaultDuration, 0.0f) : 0.0f);
    Result.StatusEffect = StatusRow
        ? StatusRow->StatusEffect : ECMObstacleStatusEffect::None;
    Result.PrimaryStatusValue = StatusRow ? StatusRow->PrimaryStatusValue : 0.0f;
    Result.SecondaryStatusValue = StatusRow ? StatusRow->SecondaryStatusValue : 0.0f;
    return Result;
}
