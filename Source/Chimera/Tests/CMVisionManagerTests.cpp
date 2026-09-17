#include "Vision/CMVisionManagerSubsystem.h"

#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Parts/Head/CMVisionComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCMVisionManagerSourceVisibilityTest, "Chimera.Vision.Manager.SourceVisibility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCMVisionManagerProjectionTest, "Chimera.Vision.Manager.ProjectionFailure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMVisionManagerProjectionTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues InitValues;
    InitValues.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).RequiresHitProxies(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &InitValues);
    UCMVisionManagerSubsystem* VisionManager = World->GetSubsystem<UCMVisionManagerSubsystem>();
    FVector2D Pixel(123.0f, 456.0f);

    TestFalse(TEXT("A world point without a valid player projection is rejected"), VisionManager->WorldToScreenMaskPixel(FVector::ZeroVector, 2048, 2048, Pixel));
    TestEqual(TEXT("A failed projection does not fabricate an off-screen mask point"), Pixel, FVector2D(123.0f, 456.0f));

    World->DestroyWorld(false);
    return true;
}

bool FCMVisionManagerSourceVisibilityTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues InitValues;
    InitValues.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).RequiresHitProxies(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &InitValues);
    AActor* Owner = World->SpawnActor<AActor>();
    UCMVisionComponent* Vision = NewObject<UCMVisionComponent>(Owner);
    Vision->RegisterComponent();
    Vision->SetVisionActive(true);
    UCMVisionManagerSubsystem* VisionManager = World->GetSubsystem<UCMVisionManagerSubsystem>();
    VisionManager->RegisterVisionSource(Vision);

    TArray<UCMVisionComponent*> ActiveSources;
    VisionManager->GetActiveVisionSources(ActiveSources);
    TestTrue(TEXT("A visible source in a visible level is rendered"), ActiveSources.Contains(Vision));

    Owner->SetActorHiddenInGame(true);
    VisionManager->GetActiveVisionSources(ActiveSources);
    TestFalse(TEXT("A hidden owner's source is not rendered"), ActiveSources.Contains(Vision));
    Owner->SetActorHiddenInGame(false);

    ULevel* Level = Owner->GetLevel();
    const bool bWasLevelVisible = Level->bIsVisible;
    Level->bIsVisible = false;
    VisionManager->GetActiveVisionSources(ActiveSources);
    TestFalse(TEXT("A source in a hidden preloaded streaming level is not rendered"), ActiveSources.Contains(Vision));
    Level->bIsVisible = bWasLevelVisible;

    World->DestroyWorld(false);
    return true;
}

#endif
