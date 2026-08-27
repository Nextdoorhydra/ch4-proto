#include "Character/Sacrifice/CMSacrificeCharacter.h"

#include "Character/Sacrifice/CMSacrificeStateComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Gore/CMDismembermentComponent.h"
#include "UObject/ConstructorHelpers.h"

ACMSacrificeCharacter::ACMSacrificeCharacter()
{
    bReplicates = true;

    DismembermentComponent = CreateDefaultSubobject<UCMDismembermentComponent>(
        TEXT("DismembermentComponent"));
    DismembermentComponent->bIncludeTorsoInFallbackDefinition = false;

    SacrificeStateComponent =
        CreateDefaultSubobject<UCMSacrificeStateComponent>(
            TEXT("SacrificeStateComponent"));

    Head = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Head"));
    Arn_L = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Arn_L"));
    Arn_R = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Arn_R"));
    Leg_L = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Leg_L"));
    Leg_R = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Leg_R"));

    const TArray<USkeletalMeshComponent*> BodyParts = {
        Head, Arn_L, Arn_R, Leg_L, Leg_R
    };
    for (USkeletalMeshComponent* BodyPart : BodyParts)
    {
        BodyPart->SetupAttachment(GetMesh());
        BodyPart->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    static ConstructorHelpers::FObjectFinder<USkeletalMesh> TorsoMesh(
        TEXT("/Game/CoreC/02BaseBody/SKM/male_Torso.male_Torso"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> HeadMesh(
        TEXT("/Game/CoreC/02BaseBody/SKM/male_Head.male_Head"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> ArmLeftMesh(
        TEXT("/Game/CoreC/02BaseBody/SKM/male_Arm_L.male_Arm_L"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> ArmRightMesh(
        TEXT("/Game/CoreC/02BaseBody/SKM/male_Arm_R.male_Arm_R"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> LegLeftMesh(
        TEXT("/Game/CoreC/02BaseBody/SKM/male_Leg_L.male_Leg_L"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> LegRightMesh(
        TEXT("/Game/CoreC/02BaseBody/SKM/male_Leg_R.male_Leg_R"));
    GetMesh()->SetSkeletalMeshAsset(TorsoMesh.Object);
    Head->SetSkeletalMeshAsset(HeadMesh.Object);
    Arn_L->SetSkeletalMeshAsset(ArmLeftMesh.Object);
    Arn_R->SetSkeletalMeshAsset(ArmRightMesh.Object);
    Leg_L->SetSkeletalMeshAsset(LegLeftMesh.Object);
    Leg_R->SetSkeletalMeshAsset(LegRightMesh.Object);
}

int32 ACMSacrificeCharacter::ReceiveDismembermentHit_Implementation(
    const FCMDismembermentHitRequest& Request
)
{
    return SacrificeStateComponent
        ? SacrificeStateComponent->ResolveDismembermentHit(Request)
        : 0;
}
