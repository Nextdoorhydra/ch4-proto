#include "Stage/Device/Component/CMInteractionHighlightComponent.h"

#include "Components/MeshComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UCMInteractionHighlightComponent::UCMInteractionHighlightComponent()
{
    SetIsReplicatedByDefault(true);
}

void UCMInteractionHighlightComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ThisClass, bHighlighted);
}

void UCMInteractionHighlightComponent::AddHighlightTarget(UMeshComponent* Target)
{
    if (!IsValid(Target) || HighlightTargets.Contains(Target)) return;

    HighlightTargets.Add(Target);
    OriginalOverlayMaterials.Add(Target->GetOverlayMaterial());
    Target->SetOverlayMaterial(bHighlighted && IsValid(HighlightMaterial)
        ? HighlightMaterial.Get()
        : OriginalOverlayMaterials.Last().Get());
}

void UCMInteractionHighlightComponent::SetHighlighted(bool bNewHighlighted)
{
    AActor* Owner = GetOwner();
    if (!IsValid(Owner) || !Owner->HasAuthority() || bHighlighted == bNewHighlighted) return;

    bHighlighted = bNewHighlighted;
    ApplyHighlight();
    Owner->ForceNetUpdate();
}

void UCMInteractionHighlightComponent::OnRep_Highlighted()
{
    ApplyHighlight();
}

void UCMInteractionHighlightComponent::ApplyHighlight()
{
    for (int32 Index = 0; Index < HighlightTargets.Num(); ++Index)
    {
        UMeshComponent* Target = HighlightTargets[Index];
        if (!IsValid(Target)) continue;

        UMaterialInterface* Original = OriginalOverlayMaterials.IsValidIndex(Index)
            ? OriginalOverlayMaterials[Index].Get()
            : nullptr;
        Target->SetOverlayMaterial(bHighlighted && IsValid(HighlightMaterial)
            ? HighlightMaterial.Get()
            : Original);
    }

    OnHighlightChanged.Broadcast(bHighlighted);
}

void UCMInteractionHighlightComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    const bool bWasHighlighted = bHighlighted;
    bHighlighted = false;
    if (bWasHighlighted) ApplyHighlight();
    Super::EndPlay(EndPlayReason);
}
