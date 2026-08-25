#include "Gore/CMDismembermentDefinition.h"

namespace
{
    FCMDismembermentPartDefinition MakePart(
        const ECMBodyPart BodyPart,
        const TCHAR* ComponentName,
        const TCHAR* MeshPath,
        const TCHAR* PhysicsAssetPath
    )
    {
        FCMDismembermentPartDefinition Result;
        Result.BodyPart = BodyPart;
        Result.ComponentName = FName(ComponentName);
        Result.DetachedMesh = TSoftObjectPtr<USkeletalMesh>(
            FSoftObjectPath(MeshPath));
        Result.DetachedPhysicsAsset = TSoftObjectPtr<UPhysicsAsset>(
            FSoftObjectPath(PhysicsAssetPath));
        return Result;
    }
}

UCMDismembermentDefinition::UCMDismembermentDefinition()
{
    Parts = MakeDefaultHumanParts();
}

const FCMDismembermentPartDefinition*
UCMDismembermentDefinition::FindPart(const ECMBodyPart BodyPart) const
{
    return Parts.FindByPredicate(
        [BodyPart](const FCMDismembermentPartDefinition& Part)
        {
            return Part.BodyPart == BodyPart;
        });
}

bool UCMDismembermentDefinition::GetPartDefinition(
    const ECMBodyPart BodyPart,
    FCMDismembermentPartDefinition& OutDefinition
) const
{
    const FCMDismembermentPartDefinition* Definition = FindPart(BodyPart);
    if (!Definition)
    {
        return false;
    }

    OutDefinition = *Definition;
    return true;
}

TArray<FCMDismembermentPartDefinition>
UCMDismembermentDefinition::MakeDefaultHumanParts()
{
    return {
        MakePart(
            ECMBodyPart::Head,
            TEXT("Head"),
            TEXT("/Game/CoreC/02BaseBody/SKM/male_Head.male_Head"),
            TEXT("/Game/CoreC/02BaseBody/SKM/male_Head_PhysicsAsset.male_Head_PhysicsAsset")),
        MakePart(
            ECMBodyPart::Torso,
            TEXT("Torso"),
            TEXT("/Game/CoreC/02BaseBody/SKM/male_Torso.male_Torso"),
            TEXT("/Game/CoreC/02BaseBody/SKM/male_Torso_PhysicsAsset.male_Torso_PhysicsAsset")),
        MakePart(
            ECMBodyPart::ArmLeft,
            TEXT("Arn_L"),
            TEXT("/Game/CoreC/02BaseBody/SKM/male_Arm_L.male_Arm_L"),
            TEXT("/Game/CoreC/02BaseBody/SKM/male_Arm_L_PhysicsAsset.male_Arm_L_PhysicsAsset")),
        MakePart(
            ECMBodyPart::ArmRight,
            TEXT("Arn_R"),
            TEXT("/Game/CoreC/02BaseBody/SKM/male_Arm_R.male_Arm_R"),
            TEXT("/Game/CoreC/02BaseBody/SKM/male_Arm_R_PhysicsAsset.male_Arm_R_PhysicsAsset")),
        MakePart(
            ECMBodyPart::LegLeft,
            TEXT("Leg_L"),
            TEXT("/Game/CoreC/02BaseBody/SKM/male_Leg_L.male_Leg_L"),
            TEXT("/Game/CoreC/02BaseBody/SKM/male_Leg_L_PhysicsAsset.male_Leg_L_PhysicsAsset")),
        MakePart(
            ECMBodyPart::LegRight,
            TEXT("Leg_R"),
            TEXT("/Game/CoreC/02BaseBody/SKM/male_Leg_R.male_Leg_R"),
            TEXT("/Game/CoreC/02BaseBody/SKM/male_Leg_R_PhysicsAsset.male_Leg_R_PhysicsAsset"))
    };
}
