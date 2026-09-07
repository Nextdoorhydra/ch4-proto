#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CMInteractionHighlightComponent.generated.h"

class UMaterialInterface;
class UMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCMInteractionHighlightSignature, bool, bHighlighted);

// Replicates interaction availability and presents it through a non-destructive mesh overlay.
UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
class CHIMERA_API UCMInteractionHighlightComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMInteractionHighlightComponent();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintCallable, Category = "Chimera|Interaction Highlight")
    void AddHighlightTarget(UMeshComponent* Target);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Interaction Highlight")
    void SetHighlighted(bool bNewHighlighted);

    UFUNCTION(BlueprintPure, Category = "Chimera|Interaction Highlight")
    bool IsHighlighted() const { return bHighlighted; }

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Interaction Highlight")
    TObjectPtr<UMaterialInterface> HighlightMaterial;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Interaction Highlight")
    FCMInteractionHighlightSignature OnHighlightChanged;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UFUNCTION()
    void OnRep_Highlighted();

    void ApplyHighlight();

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMeshComponent>> HighlightTargets;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInterface>> OriginalOverlayMaterials;

    UPROPERTY(ReplicatedUsing = OnRep_Highlighted)
    bool bHighlighted = false;
};
