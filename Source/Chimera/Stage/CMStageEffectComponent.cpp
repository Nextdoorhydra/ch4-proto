#include "Stage/CMStageEffectComponent.h"

#include "NiagaraComponent.h"
#include "Stage/CMStageCommandTags.h"

UCMStageEffectComponent::UCMStageEffectComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    ExecutionPolicy = ECMStageCommandExecutionPolicy::AllMachines;
}

void UCMStageEffectComponent::ExecuteStageCommand_Implementation(
    FGameplayTag CommandTag,
    UObject* CommandInstigator)
{
    if (CommandTag.MatchesTagExact(CMStageCommandTags::Effect_Activate))
    {
        ActivateEffects(false);
    }
    else if (CommandTag.MatchesTagExact(CMStageCommandTags::Effect_Deactivate))
    {
        DeactivateEffects();
    }
    else if (CommandTag.MatchesTagExact(CMStageCommandTags::Effect_Restart))
    {
        RestartEffects();
    }
    else if (CommandTag.MatchesTagExact(CMStageCommandTags::Effect_Burst))
    {
        BurstEffects();
    }
}

void UCMStageEffectComponent::PreviewSelectedAction()
{
    switch (PreviewAction)
    {
    case ECMStageEffectPreviewAction::Activate:
        ActivateEffects(false);
        break;
    case ECMStageEffectPreviewAction::Deactivate:
        DeactivateEffects();
        break;
    case ECMStageEffectPreviewAction::Restart:
        RestartEffects();
        break;
    case ECMStageEffectPreviewAction::Burst:
        BurstEffects();
        break;
    default:
        break;
    }
}

void UCMStageEffectComponent::ActivateEffects(bool bReset)
{
    for (UNiagaraComponent* Effect : Effects)
    {
        if (IsValid(Effect))
        {
            Effect->Activate(bReset);
        }
    }
}

void UCMStageEffectComponent::DeactivateEffects()
{
    for (UNiagaraComponent* Effect : Effects)
    {
        if (IsValid(Effect))
        {
            Effect->Deactivate();
        }
    }
}

void UCMStageEffectComponent::RestartEffects()
{
    for (UNiagaraComponent* Effect : Effects)
    {
        if (IsValid(Effect))
        {
            Effect->ReinitializeSystem();
        }
    }
}

void UCMStageEffectComponent::BurstEffects()
{
    for (UNiagaraComponent* Effect : Effects)
    {
        if (IsValid(Effect))
        {
            Effect->DeactivateImmediate();
            Effect->Activate(true);
        }
    }
}
