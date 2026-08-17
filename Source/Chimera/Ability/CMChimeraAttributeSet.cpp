#include "Ability/CMChimeraAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraAttributes, Log, All);

void UCMChimeraAttributeSet::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION_NOTIFY(
        UCMChimeraAttributeSet,
        Stamina,
        COND_None,
        REPNOTIFY_Always
    );
    DOREPLIFETIME_CONDITION_NOTIFY(
        UCMChimeraAttributeSet,
        MaxStamina,
        COND_None,
        REPNOTIFY_Always
    );
    DOREPLIFETIME_CONDITION_NOTIFY(
        UCMChimeraAttributeSet,
        StaminaRegen,
        COND_None,
        REPNOTIFY_Always
    );
}

void UCMChimeraAttributeSet::PreAttributeChange(
    const FGameplayAttribute& Attribute,
    float& NewValue
)
{
    Super::PreAttributeChange(Attribute, NewValue);

    // 서버와 클라이언트 모두 같은 범위 규칙을 사용해야 예측/복제 후 값이 일치한다.
    if (Attribute == GetStaminaAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxStamina());
    }
    else if (Attribute == GetMaxStaminaAttribute()
        || Attribute == GetStaminaRegenAttribute())
    {
        NewValue = FMath::Max(NewValue, 0.0f);
    }
}

void UCMChimeraAttributeSet::PostGameplayEffectExecute(
    const FGameplayEffectModCallbackData& Data
)
{
    Super::PostGameplayEffectExecute(Data);

    if (Data.EvaluatedData.Attribute == GetStaminaAttribute())
    {
        SetStamina(FMath::Clamp(
            GetStamina(),
            0.0f,
            GetMaxStamina()
        ));
    }
}

void UCMChimeraAttributeSet::OnRep_Stamina(
    const FGameplayAttributeData& OldValue
) const
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(
        UCMChimeraAttributeSet,
        Stamina,
        OldValue
    );

    // 회복 중에는 자주 호출될 수 있으므로 VeryVerbose로 두어 기본 로그를 오염시키지 않는다.
    UE_LOG(LogChimeraAttributes, VeryVerbose,
        TEXT("[Client Replication] Shared Stamina %.1f -> %.1f"),
        OldValue.GetCurrentValue(), Stamina.GetCurrentValue());
}

void UCMChimeraAttributeSet::OnRep_MaxStamina(
    const FGameplayAttributeData& OldValue
) const
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(
        UCMChimeraAttributeSet,
        MaxStamina,
        OldValue
    );

    UE_LOG(LogChimeraAttributes, Verbose,
        TEXT("[Client Replication] MaxStamina %.1f -> %.1f"),
        OldValue.GetCurrentValue(), MaxStamina.GetCurrentValue());
}

void UCMChimeraAttributeSet::OnRep_StaminaRegen(
    const FGameplayAttributeData& OldValue
) const
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(
        UCMChimeraAttributeSet,
        StaminaRegen,
        OldValue
    );

    UE_LOG(LogChimeraAttributes, Verbose,
        TEXT("[Client Replication] StaminaRegen %.1f -> %.1f"),
        OldValue.GetCurrentValue(), StaminaRegen.GetCurrentValue());
}
