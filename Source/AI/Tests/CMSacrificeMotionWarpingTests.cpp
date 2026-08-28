#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "MotionWarpingComponent.h"
#include "Sacrifice/CMSacrificeCharacter.h"

namespace
{
    FTransform EvaluateBoneWithLockedRoot(const UAnimSequence& Animation, const FName BoneName, const int32 KeyIndex)
    {
        const FReferenceSkeleton& ReferenceSkeleton = Animation.GetSkeleton()->GetReferenceSkeleton();
        int32 BoneIndex = ReferenceSkeleton.FindBoneIndex(BoneName);
        FTransform ComponentTransform = FTransform::Identity;
        while (BoneIndex != INDEX_NONE)
        {
            const FName CurrentBoneName = ReferenceSkeleton.GetBoneName(BoneIndex);
            const FTransform LocalTransform = BoneIndex == 0 ? FTransform::Identity : Animation.GetDataModel()->EvaluateBoneTrackTransform(CurrentBoneName, KeyIndex, EAnimInterpolationType::Step);
            ComponentTransform *= LocalTransform;
            BoneIndex = ReferenceSkeleton.GetParentIndex(BoneIndex);
        }
        return ComponentTransform;
    }
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCMSacrificeMotionWarpingTest, "Chimera.AI.Sacrifice.MotionWarping", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMSacrificeMotionWarpingTest::RunTest(const FString& Parameters)
{
    const ACMSacrificeCharacter* Character = GetDefault<ACMSacrificeCharacter>();
    TestNotNull(TEXT("Sacrifice owns a motion warping component"), Character->FindComponentByClass<UMotionWarpingComponent>());
    TestTrue(TEXT("Front hit uses a valid standing-up duration"), Character->GetGettingUpDuration(ECMSacrificeHitReactionDirection::Front) > 0.1f);
    TestTrue(TEXT("Back hit uses a valid standing-up duration"), Character->GetGettingUpDuration(ECMSacrificeHitReactionDirection::Back) > 0.1f);

    const TCHAR* RootMotionAssetPaths[] = {TEXT("/Game/Chimera/AI/Sacrifice/Anim/AS_FrontHit_Fall.AS_FrontHit_Fall"), TEXT("/Game/Chimera/AI/Sacrifice/Anim/AS_BackHit_Fall.AS_BackHit_Fall"), TEXT("/Game/Chimera/AI/Sacrifice/Anim/AS_Stumble_Backwards.AS_Stumble_Backwards")};
    for (const TCHAR* AssetPath : RootMotionAssetPaths)
    {
        const UAnimSequence* Animation = LoadObject<UAnimSequence>(nullptr, AssetPath);
        TestNotNull(AssetPath, Animation);
        if (Animation)
        {
            TestTrue(*FString::Printf(TEXT("%s enables root motion"), AssetPath), Animation->bEnableRootMotion);
            TestFalse(*FString::Printf(TEXT("%s does not force root lock"), AssetPath), Animation->bForceRootLock);

            const FTransform FirstRoot = Animation->ExtractRootTrackTransform(FAnimExtractContext(0.0), nullptr);
            AddInfo(FString::Printf(TEXT("%s first root: translation=%s rotation=%s lock=%d"), AssetPath, *FirstRoot.GetTranslation().ToCompactString(), *FirstRoot.Rotator().ToCompactString(), static_cast<int32>(Animation->RootMotionRootLock)));
            TestTrue(*FString::Printf(TEXT("%s starts with an identity root"), AssetPath), FirstRoot.Equals(FTransform::Identity, 1.0f));
            TestEqual(*FString::Printf(TEXT("%s locks root to its normalized first frame"), AssetPath), Animation->RootMotionRootLock.GetValue(), ERootMotionRootLock::AnimFirstFrame);

            const FTransform RootMotion = Animation->ExtractRootMotionFromRange(0.0, Animation->GetPlayLength(), FAnimExtractContext());
            const FVector Translation = RootMotion.GetTranslation();
            const FRotator Rotation = RootMotion.Rotator();
            AddInfo(FString::Printf(TEXT("%s root motion: translation=%s rotation=%s"), AssetPath, *Translation.ToCompactString(), *Rotation.ToCompactString()));
            TestTrue(*FString::Printf(TEXT("%s contains planar root motion"), AssetPath), Translation.Size2D() > 50.0f);
            TestTrue(*FString::Printf(TEXT("%s keeps vertical root motion near zero"), AssetPath), FMath::Abs(Translation.Z) < 1.0f);
            TestTrue(*FString::Printf(TEXT("%s keeps root pitch and roll near zero"), AssetPath), FMath::Abs(Rotation.Pitch) < 1.0f && FMath::Abs(Rotation.Roll) < 1.0f);
        }
    }

    const UAnimSequence* CrawlAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Chimera/AI/Sacrifice/Anim/AS_Crawl_Backwards.AS_Crawl_Backwards"));
    TestNotNull(TEXT("Back crawl animation exists"), CrawlAnimation);
    if (CrawlAnimation)
    {
        TestFalse(TEXT("Back crawl remains non-root-motion"), CrawlAnimation->bEnableRootMotion);
    }

    const UAnimSequence* InjuredCrawlAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Chimera/AI/Sacrifice/Anim/AS_Injured_Crawl.AS_Injured_Crawl"));
    TestNotNull(TEXT("Injured crawl animation exists"), InjuredCrawlAnimation);
    if (InjuredCrawlAnimation)
    {
        const UAnimSequence* BackHitAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Chimera/AI/Sacrifice/Anim/AS_BackHit_Fall.AS_BackHit_Fall"));
        if (BackHitAnimation)
        {
            const int32 BackHitLastKey = BackHitAnimation->GetDataModel()->GetNumberOfKeys() - 1;
            const FTransform BackHitPelvis = EvaluateBoneWithLockedRoot(*BackHitAnimation, TEXT("pelvis"), BackHitLastKey);
            const FTransform InjuredCrawlPelvis = EvaluateBoneWithLockedRoot(*InjuredCrawlAnimation, TEXT("pelvis"), 0);
            const FVector PoseDelta = InjuredCrawlPelvis.GetTranslation() - BackHitPelvis.GetTranslation();
            AddInfo(FString::Printf(
                TEXT("Back-hit end pelvis=%s, injured-crawl start pelvis=%s, delta=%s"), *BackHitPelvis.GetTranslation().ToCompactString(), *InjuredCrawlPelvis.GetTranslation().ToCompactString(), *(InjuredCrawlPelvis.GetTranslation() - BackHitPelvis.GetTranslation()).ToCompactString()
            ));
            TestTrue(TEXT("Back-hit end and injured-crawl start align in the movement plane"), PoseDelta.Size2D() < 1.0f);
        }
    }
    return true;
}

#endif
