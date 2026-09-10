#include "Sacrifice/CMSacrificeCharacter.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Common/Ability/CMAIGameplayTags.h"
#include "Common/Ability/CMAIStateGameplayEffect.h"
#include "Components/AudioComponent.h"
#include "Sacrifice/CMSacrificeActionAbilities.h"
#include "Sacrifice/CMSacrificeAIController.h"
#include "Sacrifice/CMSacrificeAttributeSet.h"
#include "Sacrifice/CMSacrificeGameplayEffects.h"
#include "Sacrifice/CMSacrificeRules.h"
#include "Sacrifice/CMSacrificeStateComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gore/CMDismembermentComponent.h"
#include "MotionWarpingComponent.h"
#include "Net/UnrealNetwork.h"
#include "RootMotionModifier_SkewWarp.h"
#include "Sound/CMSoundPlayback.h"
#include "Sound/CMSoundTags.h"
#include "UObject/ConstructorHelpers.h"

// 분리 가능한 신체와 GAS·모션 워핑·상태 컴포넌트를 가진 Sacrifice 캐릭터를 구성한다.
ACMSacrificeCharacter::ACMSacrificeCharacter()
{
    bReplicates = true;
    AIControllerClass = ACMSacrificeAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Full);
    SacrificeAttributeSet = CreateDefaultSubobject<UCMSacrificeAttributeSet>(TEXT("SacrificeAttributeSet"));
    MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));

    DismembermentComponent = CreateDefaultSubobject<UCMDismembermentComponent>(TEXT("DismembermentComponent"));
    DismembermentComponent->bIncludeTorsoInFallbackDefinition = false;

    SacrificeStateComponent = CreateDefaultSubobject<UCMSacrificeStateComponent>(TEXT("SacrificeStateComponent"));

    Head = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Head"));
    Arn_L = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Arn_L"));
    Arn_R = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Arn_R"));
    Leg_L = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Leg_L"));
    Leg_R = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Leg_R"));

    const TArray<USkeletalMeshComponent*> BodyParts = {Head, Arn_L, Arn_R, Leg_L, Leg_R};

    for (USkeletalMeshComponent* BodyPart : BodyParts)
    {
        BodyPart->SetupAttachment(GetMesh());
        BodyPart->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    static ConstructorHelpers::FObjectFinder<USkeletalMesh> TorsoMesh(TEXT("/Game/CoreC/02BaseBody/SKM/male_Torso.male_Torso"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> HeadMesh(TEXT("/Game/CoreC/02BaseBody/SKM/male_Head.male_Head"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> ArmLeftMesh(TEXT("/Game/CoreC/02BaseBody/SKM/male_Arm_L.male_Arm_L"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> ArmRightMesh(TEXT("/Game/CoreC/02BaseBody/SKM/male_Arm_R.male_Arm_R"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> LegLeftMesh(TEXT("/Game/CoreC/02BaseBody/SKM/male_Leg_L.male_Leg_L"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> LegRightMesh(TEXT("/Game/CoreC/02BaseBody/SKM/male_Leg_R.male_Leg_R"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> FrontHitFall(TEXT("/Game/Chimera/AI/Sacrifice/Anim/AS_FrontHit_Fall.AS_FrontHit_Fall"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> BackHitFall(TEXT("/Game/Chimera/AI/Sacrifice/Anim/AS_BackHit_Fall.AS_BackHit_Fall"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> BackFall(TEXT("/Game/Chimera/AI/Sacrifice/Anim/AS_Stumble_Backwards.AS_Stumble_Backwards"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> SafetyInjuryFall(TEXT("/Game/Chimera/AI/Sacrifice/Anim/AS_FallDown_Before_Writhing.AS_FallDown_Before_Writhing"));

    static ConstructorHelpers::FObjectFinder<UAnimSequence> StandingUpBack(TEXT("/Game/Chimera/AI/Sacrifice/Anim/AS_Standing_Up_Back.AS_Standing_Up_Back"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> StandingUpFront(TEXT("/Game/Chimera/AI/Sacrifice/Anim/AS_Standing_Up_Front.AS_Standing_Up_Front"));

    GetMesh()->SetSkeletalMeshAsset(TorsoMesh.Object);
    Head->SetSkeletalMeshAsset(HeadMesh.Object);
    Arn_L->SetSkeletalMeshAsset(ArmLeftMesh.Object);
    Arn_R->SetSkeletalMeshAsset(ArmRightMesh.Object);
    Leg_L->SetSkeletalMeshAsset(LegLeftMesh.Object);
    Leg_R->SetSkeletalMeshAsset(LegRightMesh.Object);

    FrontHitFallAnimation = FrontHitFall.Object;
    BackHitFallAnimation = BackHitFall.Object;
    BackFallAnimation = BackFall.Object;
    SafetyInjuryFallAnimation = SafetyInjuryFall.Object;

    StandingUpBackAnimation = StandingUpBack.Object;
    StandingUpFrontAnimation = StandingUpFront.Object;

    const TArray<UAnimSequence*> RootMotionAnimations = {FrontHitFallAnimation, BackHitFallAnimation, BackFallAnimation};

    for (UAnimSequence* Animation : RootMotionAnimations)
    {
        if (Animation)
        {
            Animation->bEnableRootMotion = true;
            Animation->bForceRootLock = false;
            Animation->RootMotionRootLock = ERootMotionRootLock::AnimFirstFrame;
        }
    }

    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->bUseControllerDesiredRotation = false;
}

// 애니메이션과 GAS를 초기화하고 절단·출혈·사망 상태 이벤트를 연결한다.
void ACMSacrificeCharacter::BeginPlay()
{
    Super::BeginPlay();

    const FRotator CurrentMeshRotation = GetMesh()->GetRelativeRotation();
    GetMesh()->SetRelativeRotation(FRotator(CurrentMeshRotation.Pitch, CharacterMeshYawOffsetDegrees, CurrentMeshRotation.Roll));
    if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
    {
        AnimInstance->SetRootMotionMode(ERootMotionMode::RootMotionFromEverything);
    }

    AbilitySystemComponent->InitAbilityActorInfo(this, this);
    RefreshMovementSpeed();

    SacrificeStateComponent->OnBodyPartSevered.AddDynamic(this, &ThisClass::HandleBodyPartSevered);
    SacrificeStateComponent->OnMissingPartsChanged.AddDynamic(this, &ThisClass::HandleMissingPartsChanged);
    SacrificeStateComponent->OnBleedingStateChanged.AddDynamic(this, &ThisClass::HandleBleedingStateChanged);
    SacrificeStateComponent->OnSacrificeDied.AddDynamic(this, &ThisClass::HandleSacrificeDied);
    RefreshBleedingLoopSound(SacrificeStateComponent->IsBleeding());

    if (HasAuthority())
    {
        InitializeAbilitySystem();
        GrantActionAbilities();
    }
}

void ACMSacrificeCharacter::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    StopBleedingLoopSound();
    Super::EndPlay(EndPlayReason);
}

void ACMSacrificeCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACMSacrificeCharacter, CurrentActionState);
    DOREPLIFETIME(ACMSacrificeCharacter, CurrentThreat);
    DOREPLIFETIME(ACMSacrificeCharacter, bHitReacting);
    DOREPLIFETIME(ACMSacrificeCharacter, HitReactionDirection);
    DOREPLIFETIME(ACMSacrificeCharacter, GettingUpDirection);
    DOREPLIFETIME(ACMSacrificeCharacter, HitKnockbackDirection);
}

UAbilitySystemComponent* ACMSacrificeCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

// 절단 판정을 상태 컴포넌트에 위임하고 승인된 피격만 AI 반응 이벤트로 전달한다.
int32 ACMSacrificeCharacter::ReceiveDismembermentHit_Implementation(const FCMDismembermentHitRequest& Request)
{
    const int32 Result = SacrificeStateComponent ? SacrificeStateComponent->ResolveDismembermentHit(Request) : 0;
    if (Result > 0 && HasAuthority())
    {
        MulticastPlayVocalSound(CMSoundTags::AI_Sacrifice_HitScream);
        OnSacrificeHitAccepted.Broadcast(Request.Attacker, Request.SourcePart, Request.ImpactDirection);
    }
    return Result;
}

float ACMSacrificeCharacter::GetFleeCharges() const
{
    return SacrificeAttributeSet ? SacrificeAttributeSet->GetFleeCharges() : 0.0f;
}

float ACMSacrificeCharacter::GetMaxFleeCharges() const
{
    return SacrificeAttributeSet ? SacrificeAttributeSet->GetMaxFleeCharges() : 0.0f;
}

bool ACMSacrificeCharacter::HasLostLeg() const
{
    return SacrificeStateComponent && (!SacrificeStateComponent->HasBodyPart(ECMBodyPart::LegLeft) || !SacrificeStateComponent->HasBodyPart(ECMBodyPart::LegRight));
}

bool ACMSacrificeCharacter::HasLostArm() const
{
    return SacrificeStateComponent && (!SacrificeStateComponent->HasBodyPart(ECMBodyPart::ArmLeft) || !SacrificeStateComponent->HasBodyPart(ECMBodyPart::ArmRight));
}

bool ACMSacrificeCharacter::HasLostArmOrLeg() const
{
    return HasLostArm() || HasLostLeg();
}

FVector ACMSacrificeCharacter::GetSacrificeVisionOrigin() const
{
    if (Head && Head->DoesSocketExist(VisionBoneName))
    {
        return Head->GetSocketLocation(VisionBoneName);
    }
    return Head ? Head->GetComponentLocation() : GetActorLocation();
}

FVector ACMSacrificeCharacter::GetSacrificeVisionForward() const
{
    if (Head && Head->DoesSocketExist(VisionBoneName))
    {
        const FVector LocalForward = VisionBoneLocalForwardAxis.GetSafeNormal(SMALL_NUMBER, FVector::YAxisVector);

        return Head->GetSocketQuaternion(VisionBoneName).RotateVector(LocalForward).GetSafeNormal(SMALL_NUMBER, GetActorForwardVector());
    }
    return GetActorForwardVector();
}

// 실행 중인 Sacrifice 행동을 취소하고 요청한 단일 행동 어빌리티로 교체한다.
bool ACMSacrificeCharacter::ActivateSacrificeAction(TSubclassOf<UGameplayAbility> AbilityClass)
{
    if (!HasAuthority() || !AbilitySystemComponent || !AbilityClass || !SacrificeStateComponent->IsAlive())
    {
        return false;
    }

    CancelSacrificeActions();

    return AbilitySystemComponent->TryActivateAbilityByClass(AbilityClass);
}

void ACMSacrificeCharacter::CancelSacrificeActions()
{
    if (!AbilitySystemComponent)
    {
        return;
    }

    FGameplayTagContainer WithTags;
    WithTags.AddTag(CMAIGameplayTags::Ability_Sacrifice_Action);
    AbilitySystemComponent->CancelAbilities(&WithTags);
}

// 서버 행동 상태와 대응 태그를 함께 적용하고 이동속도 및 복제 상태를 갱신한다.
void ACMSacrificeCharacter::SetSacrificeActionState(const ECMSacrificeActionState NewState, const FGameplayTag StateTag)
{
    if (!HasAuthority())
    {
        return;
    }

    CurrentActionState = NewState;
    ApplyStateTag(StateTag, true);
    RefreshMovementSpeed();
    ForceNetUpdate();
}

// 종료되는 행동의 태그를 제거하고 여전히 같은 상태일 때만 기본 상태로 되돌린다.
void ACMSacrificeCharacter::ClearSacrificeActionState(const ECMSacrificeActionState State, const FGameplayTag StateTag)
{
    if (!HasAuthority())
    {
        return;
    }

    ApplyStateTag(StateTag, false);
    if (CurrentActionState == State)
    {
        CurrentActionState = ECMSacrificeActionState::None;
        RefreshMovementSpeed();
        ForceNetUpdate();
    }
}

void ACMSacrificeCharacter::SetCurrentThreat(AActor* NewThreat)
{
    if (HasAuthority() && CurrentThreat != NewThreat)
    {
        CurrentThreat = NewThreat;
        ForceNetUpdate();
    }
}

void ACMSacrificeCharacter::PlayThreatScream()
{
    if (HasAuthority())
    {
        MulticastPlayVocalSound(CMSoundTags::AI_Sacrifice_ThreatScream);
    }
}

void ACMSacrificeCharacter::ConsumeFleeCharge()
{
    if (HasAuthority() && AbilitySystemComponent && GetFleeCharges() > 0.0f)
    {
        AbilitySystemComponent->ApplyGameplayEffectToSelf(UCMSacrificeConsumeFleeGameplayEffect::StaticClass()->GetDefaultObject<UGameplayEffect>(), 1.0f, AbilitySystemComponent->MakeEffectContext());
    }
}

void ACMSacrificeCharacter::RefillFleeCharges()
{
    if (HasAuthority() && AbilitySystemComponent)
    {
        AbilitySystemComponent->ApplyGameplayEffectToSelf(UCMSacrificeRefillFleeGameplayEffect::StaticClass()->GetDefaultObject<UGameplayEffect>(), 1.0f, AbilitySystemComponent->MakeEffectContext());
    }
}

// 피격 방향을 앞·뒤 넘어짐으로 변환하고 목표 거리까지 모션 워핑되는 반응을 시작한다.
float ACMSacrificeCharacter::BeginHitReaction(const FVector& ImpactDirection)
{
    if (!HasAuthority() || bHitReacting || !SacrificeStateComponent || !SacrificeStateComponent->IsAlive() || CurrentActionState == ECMSacrificeActionState::Incapacitated || CurrentActionState == ECMSacrificeActionState::Dead)
    {
        return 0.0f;
    }

    // 수직 충격은 넘어짐 방향을 불안정하게 만들므로 캐릭터 평면 기준으로만 판정한다.
    const FVector PlanarDirection = ImpactDirection.GetSafeNormal2D(SMALL_NUMBER, -GetActorForwardVector());
    HitReactionDirection = FCMSacrificeRules::SelectHitReactionDirection(GetActorForwardVector(), PlanarDirection);
    HitKnockbackDirection = PlanarDirection;
    bHitReacting = true;

    MulticastStopGettingUpAnimation();
    GetCharacterMovement()->StopMovementImmediately();
    MulticastConfigureHitWarp(HitReactionDirection, GetActorLocation() + PlanarDirection * HitKnockbackDistanceCm, GetActorRotation());
    ForceNetUpdate();

    const UAnimSequence* SelectedAnimation = HitReactionDirection == ECMSacrificeHitReactionDirection::Front ? FrontHitFallAnimation : BackHitFallAnimation;
    return SelectedAnimation ? SelectedAnimation->GetPlayLength() : 0.0f;
}

// 안전 지점에 도착한 부상 개체를 무력화 자세로 연결할 제자리 넘어짐을 시작한다.
float ACMSacrificeCharacter::BeginSafetyInjuryFall()
{
    if (!HasAuthority() || bHitReacting || !SafetyInjuryFallAnimation || !SacrificeStateComponent || !SacrificeStateComponent->IsAlive())
    {
        return 0.0f;
    }

    bHitReacting = true;
    MulticastStopGettingUpAnimation();
    GetCharacterMovement()->StopMovementImmediately();
    MulticastPlaySafetyInjuryFallAnimation();
    ForceNetUpdate();

    return SafetyInjuryFallAnimation->GetPlayLength() / FMath::Max(FMath::Abs(SafetyInjuryFallAnimation->RateScale), SMALL_NUMBER);
}

// 피격 모션 워핑과 반응 방향을 정리해 다음 AI 상태 전환을 허용한다.
void ACMSacrificeCharacter::FinishHitReaction()
{
    if (!HasAuthority() || !bHitReacting)
    {
        return;
    }

    GetCharacterMovement()->StopMovementImmediately();
    MulticastClearMotionWarping();
    bHitReacting = false;
    HitReactionDirection = ECMSacrificeHitReactionDirection::None;
    HitKnockbackDirection = FVector::ZeroVector;
    ForceNetUpdate();
}

// 현재 전방 반대쪽 목표로 비틀거리는 루트 모션을 워핑해 후진 기기를 준비한다.
float ACMSacrificeCharacter::BeginBackFallRootMotion()
{
    if (!HasAuthority() || !BackFallAnimation)
    {
        return 0.0f;
    }

    GetCharacterMovement()->StopMovementImmediately();
    const FVector Backward = -GetActorForwardVector().GetSafeNormal2D();
    MulticastConfigureBackFallWarp(GetActorLocation() + Backward * StumbleBackwardDistanceCm, GetActorRotation());

    return BackFallAnimation->GetPlayLength();
}

void ACMSacrificeCharacter::FinishBackFallRootMotion()
{
    if (HasAuthority())
    {
        GetCharacterMovement()->StopMovementImmediately();
        MulticastClearMotionWarping();
    }
}

// 애니메이션 전체 루트 이동을 지정한 평면 목표에 맞추는 Skew Warp를 구성한다.
void ACMSacrificeCharacter::ConfigureWarpedAnimation(UAnimSequence* Animation, const FName WarpTargetName, const FTransform& TargetTransform)
{
    if (!MotionWarpingComponent || !Animation)
        return;

    ClearMotionWarping();
    MotionWarpingComponent->AddOrUpdateWarpTargetFromTransform(WarpTargetName, TargetTransform);
    URootMotionModifier_SkewWarp::AddRootMotionModifierSkewWarp(
        MotionWarpingComponent,
        Animation,
        0.0f,
        Animation->GetPlayLength(),
        WarpTargetName,
        EWarpPointAnimProvider::None,
        FTransform::Identity,
        NAME_None,
        true,  // 목표까지의 평면 이동 거리는 워핑한다.
        true,  // 지면 높이는 애니메이션과 충돌 처리가 유지하도록 제외한다.
        false, // 피격 시 캐릭터가 바라보는 회전은 바꾸지 않는다.
        EMotionWarpRotationType::Default,
        EMotionWarpRotationMethod::Slerp,
        1.0f,
        0.0f
    );
}

void ACMSacrificeCharacter::ClearMotionWarping()
{
    if (MotionWarpingComponent)
    {
        MotionWarpingComponent->DisableAllRootMotionModifiers();
        MotionWarpingComponent->RemoveAllWarpTargets();
    }
}

void ACMSacrificeCharacter::MulticastConfigureHitWarp_Implementation(const ECMSacrificeHitReactionDirection Direction, const FVector TargetLocation, const FRotator TargetRotation)
{
    UAnimSequence* Animation = Direction == ECMSacrificeHitReactionDirection::Front ? FrontHitFallAnimation : BackHitFallAnimation;
    ConfigureWarpedAnimation(Animation, TEXT("HitWarp"), FTransform(TargetRotation, TargetLocation));
}

void ACMSacrificeCharacter::MulticastConfigureBackFallWarp_Implementation(const FVector TargetLocation, const FRotator TargetRotation)
{
    ConfigureWarpedAnimation(BackFallAnimation, TEXT("StumbleWarp"), FTransform(TargetRotation, TargetLocation));
}

void ACMSacrificeCharacter::MulticastClearMotionWarping_Implementation()
{
    ClearMotionWarping();
}

float ACMSacrificeCharacter::GetBackFallDuration() const
{
    return BackFallAnimation ? BackFallAnimation->GetPlayLength() : 0.8f;
}

// 넘어져 있는 방향에 맞는 기상 애니메이션을 모든 클라이언트에서 시작한다.
float ACMSacrificeCharacter::BeginGettingUp(const ECMSacrificeHitReactionDirection Direction)
{
    if (!HasAuthority())
        return 0.0f;

    GettingUpDirection = Direction == ECMSacrificeHitReactionDirection::Front ? ECMSacrificeHitReactionDirection::Front : ECMSacrificeHitReactionDirection::Back;
    ForceNetUpdate();
    MulticastPlayGettingUpAnimation(GettingUpDirection);

    return GetGettingUpDuration(GettingUpDirection);
}

void ACMSacrificeCharacter::FinishGettingUp()
{
    if (!HasAuthority())
        return;
    GettingUpDirection = ECMSacrificeHitReactionDirection::Back;
    ForceNetUpdate();
}

void ACMSacrificeCharacter::MulticastPlayGettingUpAnimation_Implementation(const ECMSacrificeHitReactionDirection Direction)
{
    UAnimSequence* Animation = Direction == ECMSacrificeHitReactionDirection::Front ? StandingUpFrontAnimation : StandingUpBackAnimation;

    UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;

    if (!Animation || !AnimInstance)
    {
        return;
    }

    if (ActiveGettingUpMontage && AnimInstance->Montage_IsPlaying(ActiveGettingUpMontage))
    {
        AnimInstance->Montage_Stop(0.0f, ActiveGettingUpMontage);
    }
    ActiveGettingUpMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(
        Animation,
        TEXT("DefaultSlot"),
        0.05f,
        0.05f,
        // 동적 몽타주 구간이 시퀀스 RateScale을 이미 적용하므로 추가 재생 배율은 사용하지 않는다.
        1.0f,
        1,
        -1.0f,
        0.0f
    );
}

void ACMSacrificeCharacter::MulticastStopGettingUpAnimation_Implementation()
{
    UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
    if (AnimInstance && ActiveGettingUpMontage)
    {
        AnimInstance->Montage_Stop(0.05f, ActiveGettingUpMontage);
    }
    ActiveGettingUpMontage = nullptr;
}

void ACMSacrificeCharacter::MulticastPlaySafetyInjuryFallAnimation_Implementation()
{
    UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
    if (!SafetyInjuryFallAnimation || !AnimInstance)
    {
        return;
    }

    AnimInstance->PlaySlotAnimationAsDynamicMontage(SafetyInjuryFallAnimation, TEXT("DefaultSlot"), 0.05f, 0.0f, 1.0f, 1, -1.0f, 0.0f);
}

float ACMSacrificeCharacter::GetGettingUpDuration(const ECMSacrificeHitReactionDirection Direction) const
{
    const UAnimSequence* Animation = Direction == ECMSacrificeHitReactionDirection::Front ? StandingUpFrontAnimation : StandingUpBackAnimation;
    if (!Animation)
    {
        return 0.1f;
    }
    const float EffectiveDuration = Animation->GetPlayLength() / FMath::Max(FMath::Abs(Animation->RateScale), SMALL_NUMBER);

    return FMath::Max(EffectiveDuration - GettingUpStateTransitionLeadTime, 0.1f);
}

// 부상 개체의 행동과 이동을 중단하고 무력화 어빌리티 상태로 고정한다.
void ACMSacrificeCharacter::EnterIncapacitated()
{
    if (!HasAuthority() || !SacrificeStateComponent || !SacrificeStateComponent->IsAlive())
    {
        return;
    }

    FinishHitReaction();
    ActivateSacrificeAction(UCMSacrificeIncapacitatedAbility::StaticClass());
    if (AAIController* SacrificeController = Cast<AAIController>(GetController()))
    {
        SacrificeController->StopMovement();
    }
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
}

// 피격 애니메이션 도중 예약된 무력화를 반응 종료 뒤 안전하게 적용한다.
bool ACMSacrificeCharacter::EnterPendingIncapacitation()
{
    if (!HasAuthority() || !bPendingIncapacitation)
    {
        return false;
    }

    bPendingIncapacitation = false;
    EnterIncapacitated();

    return true;
}

// 절단 부위의 부상 태그를 적용하고 일정 확률로 피격 종료 후 무력화를 예약한다.
void ACMSacrificeCharacter::HandleBodyPartSevered(const ECMBodyPart BodyPart)
{
    if (!HasAuthority() || BodyPart == ECMBodyPart::Head || !SacrificeStateComponent->IsAlive())
    {
        return;
    }

    const bool bArm = BodyPart == ECMBodyPart::ArmLeft || BodyPart == ECMBodyPart::ArmRight;
    ApplyStateTag(bArm ? CMAIGameplayTags::State_Sacrifice_InjuredArm : CMAIGameplayTags::State_Sacrifice_InjuredLeg, true);

    // 즉시 무력화하면 피격 모션이 중단되므로 완료 단계에서 처리하도록 예약한다.
    if (FMath::FRand() < 0.2f)
    {
        bPendingIncapacitation = true;
    }
    RefreshMovementSpeed();
}

void ACMSacrificeCharacter::HandleMissingPartsChanged(int32 MissingPartCount)
{
    if (HasAuthority())
    {
        RefreshMovementSpeed();
    }
}

void ACMSacrificeCharacter::HandleBleedingStateChanged(const bool bBleeding)
{
    RefreshBleedingLoopSound(bBleeding);
    if (HasAuthority())
    {
        ApplyStateTag(CMAIGameplayTags::State_Sacrifice_Bleeding, bBleeding);
    }
}

void ACMSacrificeCharacter::RefreshBleedingLoopSound(const bool bBleeding)
{
    if (!bBleeding)
    {
        StopBleedingLoopSound();
        return;
    }
    if (IsValid(BleedingLoopSoundComponent)
        && BleedingLoopSoundComponent->IsPlaying())
    {
        return;
    }

    BleedingLoopSoundComponent = FCMSoundPlayback::PlayAttachedSFX(
        GetRootComponent(),
        CMSoundTags::AI_Sacrifice_BleedingLoop);
}

void ACMSacrificeCharacter::StopBleedingLoopSound()
{
    if (IsValid(BleedingLoopSoundComponent))
    {
        BleedingLoopSoundComponent->Stop();
        BleedingLoopSoundComponent = nullptr;
    }
}

void ACMSacrificeCharacter::MulticastPlayVocalSound_Implementation(
    const FGameplayTag SoundTag)
{
    FCMSoundPlayback::PlaySFXAtActor(this, SoundTag);
}

// 모든 신체 애니메이션과 행동을 멈추고 사망 어빌리티 및 이동 불가 상태를 적용한다.
void ACMSacrificeCharacter::HandleSacrificeDied()
{
    TArray<USkeletalMeshComponent*> BodyMeshes;
    GetComponents(BodyMeshes);
    for (USkeletalMeshComponent* BodyMesh : BodyMeshes)
    {
        if (BodyMesh)
        {
            BodyMesh->bPauseAnims = true;
        }
    }

    if (!HasAuthority())
    {
        return;
    }

    bPendingIncapacitation = false;
    if (bHitReacting)
    {
        FinishHitReaction();
    }
    else
    {
        FinishBackFallRootMotion();
    }
    SetCurrentThreat(nullptr);
    CancelSacrificeActions();
    AbilitySystemComponent->TryActivateAbilityByClass(UCMSacrificeDeathAbility::StaticClass());
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
}

// 서버에서 개체별 최대 도주 횟수를 무작위로 정해 초기 Gameplay Effect를 적용한다.
void ACMSacrificeCharacter::InitializeAbilitySystem()
{
    const int32 InitialCharges = FMath::RandRange(3, 5);
    FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(UCMSacrificeInitializeGameplayEffect::StaticClass(), 1.0f, AbilitySystemComponent->MakeEffectContext());
    if (Spec.IsValid())
    {
        Spec.Data->SetSetByCallerMagnitude(UCMSacrificeInitializeGameplayEffect::MaxFleeChargesDataName, static_cast<float>(InitialCharges));
        AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
    }
}

// AI 상태 머신이 전환할 수 있는 모든 Sacrifice 행동 어빌리티를 서버에서 부여한다.
void ACMSacrificeCharacter::GrantActionAbilities()
{
    const TArray<TSubclassOf<UGameplayAbility>> AbilityClasses = {UCMSacrificePrayerAbility::StaticClass(),
         UCMSacrificeLookAroundAbility::StaticClass(),
         UCMSacrificeWanderAbility::StaticClass(),
         UCMSacrificeBackFallAbility::StaticClass(),
         UCMSacrificeBackCrawlAbility::StaticClass(),
         UCMSacrificeFleeAbility::StaticClass(),
         UCMSacrificeInjuredCrawlAbility::StaticClass(),
         UCMSacrificeGettingUpAbility::StaticClass(),
         UCMSacrificeExhaustedWalkAbility::StaticClass(),
         UCMSacrificeVigilantAbility::StaticClass(),
         UCMSacrificeIncapacitatedAbility::StaticClass(),
         UCMSacrificeDeathAbility::StaticClass()};
    for (const TSubclassOf<UGameplayAbility> AbilityClass : AbilityClasses)
    {
        AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass));
    }
}

// 상태 태그별 Gameplay Effect 핸들을 추적해 중복 없이 적용하고 정확히 제거한다.
void ACMSacrificeCharacter::ApplyStateTag(const FGameplayTag StateTag, const bool bEnabled)
{
    if (!StateTag.IsValid() || !AbilitySystemComponent)
    {
        return;
    }

    if (!bEnabled)
    {
        if (FActiveGameplayEffectHandle* Handle = StateEffectHandles.Find(StateTag))
        {
            AbilitySystemComponent->RemoveActiveGameplayEffect(*Handle);
            StateEffectHandles.Remove(StateTag);
        }
        return;
    }

    if (StateEffectHandles.Contains(StateTag))
    {
        return;
    }

    FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(UCMAIStateGameplayEffect::StaticClass(), 1.0f, AbilitySystemComponent->MakeEffectContext());
    if (Spec.IsValid())
    {
        Spec.Data->DynamicGrantedTags.AddTag(StateTag);
        const FActiveGameplayEffectHandle Handle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
        if (Handle.IsValid())
        {
            StateEffectHandles.Add(StateTag, Handle);
        }
    }
}

// 현재 행동 속도에 팔·다리 부상 상한을 적용해 최종 이동속도를 계산한다.
float ACMSacrificeCharacter::CalculateMovementSpeed() const
{
    float TargetSpeed = DefaultWalkSpeed;
    switch (CurrentActionState)
    {
    case ECMSacrificeActionState::Wander:
        TargetSpeed = WanderWalkSpeed;
        break;
    case ECMSacrificeActionState::Flee:
        TargetSpeed = FleeRunSpeed;
        break;
    case ECMSacrificeActionState::BackCrawl:
        TargetSpeed = BackCrawlSpeed;
        break;
    case ECMSacrificeActionState::InjuredCrawl:
        TargetSpeed = InjuredCrawlSpeed;
        break;
    case ECMSacrificeActionState::ExhaustedWalk:
        TargetSpeed = ExhaustedWalkSpeed;
        break;
    default:
        break;
    }

    if (AbilitySystemComponent->HasMatchingGameplayTag(CMAIGameplayTags::State_Sacrifice_InjuredArm))
    {
        TargetSpeed = FMath::Min(TargetSpeed, InjuredArmMaxSpeed);
    }
    if (AbilitySystemComponent->HasMatchingGameplayTag(CMAIGameplayTags::State_Sacrifice_InjuredLeg))
    {
        TargetSpeed = FMath::Min(TargetSpeed, InjuredLegMaxSpeed);
    }

    return TargetSpeed;
}

void ACMSacrificeCharacter::RefreshMovementSpeed()
{
    GetCharacterMovement()->MaxWalkSpeed = CalculateMovementSpeed();
}
