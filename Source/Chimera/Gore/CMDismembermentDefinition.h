#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "CMDismembermentDefinition.generated.h"

class UPhysicsAsset;
class USkeletalMesh;

UENUM(BlueprintType)
enum class ECMBodyPart : uint8
{
    None,
    Head,
    Torso,
    ArmLeft,
    ArmRight,
    LegLeft,
    LegRight
};

UENUM(BlueprintType)
enum class ECMBodyPartState : uint8
{
    AttachedAlive,
    CorpseAttached,
    Severed,
    Destroyed
};

/** Authoring data for one independently severable human body part. */
USTRUCT(BlueprintType)
struct FCMDismembermentPartDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment")
    ECMBodyPart BodyPart = ECMBodyPart::None;

    /** SkeletalMeshComponent variable name on the modular character. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment")
    FName ComponentName = NAME_None;

    /** Bone names that resolve a master-mesh hit to this body part. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment")
    TArray<FName> BoneNames;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment")
    TSoftObjectPtr<USkeletalMesh> DetachedMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment")
    TSoftObjectPtr<UPhysicsAsset> DetachedPhysicsAsset;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment|Impulse",
        meta = (ClampMin = "0.0"))
    float ImpulseMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment|Impulse",
        meta = (ClampMin = "0.0"))
    float MaxImpulse = 2500.0f;
};

/** Data-driven body-part layout shared by death, severing, and gore phases. */
UCLASS(BlueprintType)
class CHIMERA_API UCMDismembermentDefinition : public UDataAsset
{
    GENERATED_BODY()

public:
    UCMDismembermentDefinition();

    const FCMDismembermentPartDefinition* FindPart(
        ECMBodyPart BodyPart
    ) const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Dismemberment")
    bool GetPartDefinition(
        ECMBodyPart BodyPart,
        FCMDismembermentPartDefinition& OutDefinition
    ) const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Dismemberment")
    int32 GetDefinedPartCount() const
    {
        return Parts.Num();
    }

    static TArray<FCMDismembermentPartDefinition> MakeDefaultHumanParts();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment")
    TArray<FCMDismembermentPartDefinition> Parts;
};
