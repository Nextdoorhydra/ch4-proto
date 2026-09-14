#include "Misc/AutomationTest.h"

#include "Common/CMAINavigationRules.h"
#include "Sacrifice/CMSacrificeCharacter.h"
#include "Sacrifice/CMSacrificeRules.h"
#include "Sacrifice/CMSacrificeStateComponent.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCMSacrificeRulesTest, "Chimera.AI.Sacrifice.Rules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMSacrificeRulesTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Sacrifice has five severable parts"), FCMSacrificeRules::GetSeverableBodyParts().Num(), 5);

    const float ExpectedDurations[] = {60.0f, 45.0f, 30.0f, 15.0f, 0.0f};
    for (int32 MissingCount = 1; MissingCount <= 5; ++MissingCount)
    {
        TestEqual(*FString::Printf(TEXT("Initial bleed duration for %d parts"), MissingCount), FCMSacrificeRules::GetInitialBleedDuration(MissingCount), ExpectedDurations[MissingCount - 1]);
    }
    TestEqual(TEXT("Two additional parts reduce thirty seconds"), FCMSacrificeRules::GetAdditionalBleedReduction(2), 30.0f);

    TestEqual(TEXT("A force travelling against forward is a front hit"), FCMSacrificeRules::SelectHitReactionDirection(FVector::ForwardVector, -FVector::ForwardVector), ECMSacrificeHitReactionDirection::Front);
    TestEqual(TEXT("A force travelling with forward is a back hit"), FCMSacrificeRules::SelectHitReactionDirection(FVector::ForwardVector, FVector::ForwardVector), ECMSacrificeHitReactionDirection::Back);

    TestFalse(TEXT("No limb loss gets up instead of injured crawl"), FCMSacrificeRules::ShouldUseInjuredCrawlAfterHit(false, false));
    TestTrue(TEXT("A back-crawling hit continues as injured crawl"), FCMSacrificeRules::ShouldUseInjuredCrawlAfterHit(true, false));
    TestTrue(TEXT("A lost arm or leg always uses injured crawl"), FCMSacrificeRules::ShouldUseInjuredCrawlAfterHit(false, true));
    TestTrue(TEXT("Back crawl is monitored as a movement action"), FCMSacrificeRules::IsMovementActionState(ECMSacrificeActionState::BackCrawl));
    TestTrue(TEXT("Flee is monitored as a movement action"), FCMSacrificeRules::IsMovementActionState(ECMSacrificeActionState::Flee));
    TestFalse(TEXT("Prayer is not monitored as a movement action"), FCMSacrificeRules::IsMovementActionState(ECMSacrificeActionState::Prayer));
    TestFalse(TEXT("A NavMesh projection collapsed near the sacrifice is rejected"), FCMSacrificeRules::HasMeaningfulProjectedMove(FVector::ZeroVector, FVector(50.0f, 0.0f, 0.0f), 50.0f, 100.0f));
    TestTrue(TEXT("A distant NavMesh projection remains a useful move"), FCMSacrificeRules::HasMeaningfulProjectedMove(FVector::ZeroVector, FVector(150.0f, 0.0f, 0.0f), 50.0f, 100.0f));
    TestTrue(TEXT("A location near its NavMesh projection remains contained"), FCMAINavigationRules::IsWithinProjectionTolerance(FVector::ZeroVector, FVector(25.0f, 0.0f, 100.0f), 25.0f));
    TestFalse(TEXT("A location beyond its NavMesh projection tolerance requires recovery"), FCMAINavigationRules::IsWithinProjectionTolerance(FVector::ZeroVector, FVector(25.1f, 0.0f, 0.0f), 25.0f));

    TestTrue(TEXT("A point in front is inside sacrifice vision"), FCMSacrificeRules::IsPointInsideVisionCone(FVector::ZeroVector, FVector::ForwardVector, 300.0f, 30.0f, 50.0f, 70.0f, FVector(200.0f, 0.0f, 0.0f)));
    TestFalse(TEXT("A point behind is outside sacrifice vision"), FCMSacrificeRules::IsPointInsideVisionCone(FVector::ZeroVector, FVector::ForwardVector, 300.0f, 30.0f, 50.0f, 70.0f, FVector(-200.0f, 0.0f, 0.0f)));
    TestFalse(TEXT("A point beyond range is outside sacrifice vision"), FCMSacrificeRules::IsPointInsideVisionCone(FVector::ZeroVector, FVector::ForwardVector, 300.0f, 30.0f, 50.0f, 70.0f, FVector(301.0f, 0.0f, 0.0f)));

    FRandomStream RandomStream(1337);
    const TArray<ECMBodyPart> AllParts = FCMSacrificeRules::GetSeverableBodyParts();
    const ECMBodyPart Selected = FCMSacrificeRules::SelectRandomPart(AllParts, RandomStream);
    TestTrue(TEXT("One hit selects one available part"), AllParts.Contains(Selected));

    FRandomStream EmptyRandomStream(42);
    TestEqual(TEXT("No attached parts produces no selection"), FCMSacrificeRules::SelectRandomPart({}, EmptyRandomStream), ECMBodyPart::None);

    FCMSacrificeAttackPartRule HeadRule;
    HeadRule.BodyPart = ECMBodyPart::Head;
    HeadRule.bPlayerCanAcquire = true;
    TestTrue(TEXT("Attack part rules can enable player acquisition"), HeadRule.bPlayerCanAcquire);
    TestFalse(TEXT("Player acquisition is disabled by default"), FCMSacrificeAttackPartRule().bPlayerCanAcquire);

    int32 AttackPartRulesPropertyCount = 0;
    int32 DismembermentImpulsePropertyCount = 0;
    int32 RewardPartsPropertyCount = 0;
    for (TFieldIterator<FProperty> It(ACMSacrificeCharacter::StaticClass(), EFieldIterationFlags::IncludeSuper); It; ++It)
    {
        AttackPartRulesPropertyCount += It->GetFName() == TEXT("AttackPartRules") ? 1 : 0;
        DismembermentImpulsePropertyCount += It->GetFName() == TEXT("DismembermentImpulse") ? 1 : 0;
        RewardPartsPropertyCount += It->GetFName() == TEXT("RewardParts") ? 1 : 0;
    }
    TestEqual(TEXT("Attack Part Rules is exposed once on the Sacrifice character"), AttackPartRulesPropertyCount, 1);
    TestEqual(TEXT("Dismemberment Impulse is exposed once on the Sacrifice character"), DismembermentImpulsePropertyCount, 1);
    TestEqual(TEXT("Reward Parts is no longer exposed on the Sacrifice character"), RewardPartsPropertyCount, 0);
    TestNull(TEXT("Sacrifice state component no longer exposes a duplicate Dismemberment Impulse"), FindFProperty<FProperty>(UCMSacrificeStateComponent::StaticClass(), TEXT("DismembermentImpulse")));

    return true;
}

#endif
