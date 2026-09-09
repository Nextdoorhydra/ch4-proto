#pragma once

#include "CoreMinimal.h"
#include "UI/NKMUIActivatableWidget.h"

#include "CMControlHUDWidget.generated.h"

class ACMControlBody;
class ACMChimera;
class ACMPartActorBase;
class ACMPlayerState;
class ACMWireframeHUDCaptureActor;
class AActor;
class UBorder;
class UCanvasPanel;
class UCurveLinearColor;
class UImage;
class UCMPartSlotComponent;
class UProgressBar;
class UTextBlock;
class UTexture2D;
class UWidget;
struct FOnAttributeChangeData;

/** WBP-authored Chimera body map; C++ only binds live gameplay data. */
UCLASS(Blueprintable)
class UI_API UCMControlHUDWidget
    : public UNKMUIActivatableWidget
{
    GENERATED_BODY()

public:
    UCMControlHUDWidget(
        const FObjectInitializer& ObjectInitializer);

    void SetControlBody(ACMControlBody* NewControlBody);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(
        const FGeometry& MyGeometry,
        float InDeltaTime
    ) override;
    virtual int32 NativePaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled
    ) const override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera HUD|Part Images")
    TObjectPtr<UTexture2D> HeadPartTexture;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera HUD|Part Images")
    TObjectPtr<UTexture2D> ArmPartTexture;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera HUD|Part Images")
    TObjectPtr<UTexture2D> LegPartTexture;

    /** 0=dark red, 0.5=red, 1=green by default. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera HUD|Wireframe")
    TObjectPtr<UCurveLinearColor> WireframeHealthColorCurve;

    /** Screen orientation for the fixed top-down scene capture. Only yaw is used. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera HUD|Wireframe|Camera")
    FRotator WireframeCameraRotation = FRotator(-90.0f, -90.0f, 0.0f);

    /** Normalized Canvas anchor used by the runtime wireframe image. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera HUD|Wireframe|Layout")
    FVector2D WireframePanelAnchor = FVector2D(0.0f, 1.0f);

    /** Canvas alignment for the wireframe image. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera HUD|Wireframe|Layout")
    FVector2D WireframePanelAlignment = FVector2D(0.0f, 1.0f);

    /** Pixel offset from WireframePanelAnchor; defaults to the lower-left. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera HUD|Wireframe|Layout")
    FVector2D WireframePanelOffset = FVector2D(168.0f, -28.0f);

    /** Orthographic zoom change per mouse-wheel notch. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera HUD|Wireframe|Camera",
        meta = (ClampMin = "0.01", UIMin = "0.01"))
    float WireframeZoomStep = 0.12f;

    /** Minimum and maximum orthographic zoom multiplier. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera HUD|Wireframe|Camera")
    FVector2D WireframeZoomLimits = FVector2D(0.4f, 2.5f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera HUD|Wireframe|Label")
    FSlateFontInfo WireframeLabelFont;

private:
    struct FPartSlotVisual
    {
        TObjectPtr<UWidget> Root;
        TObjectPtr<UImage> BaseImage;
        TObjectPtr<UProgressBar> HealthFill;
        TObjectPtr<UTextBlock> KeyText;
        TObjectPtr<UTextBlock> PartText;
    };

    struct FSegmentRowVisual
    {
        TObjectPtr<UWidget> Root;
        TObjectPtr<UBorder> BodyBorder;
        TObjectPtr<UProgressBar> BodyHealthFill;
        TObjectPtr<UTextBlock> BodyControlText;
        TObjectPtr<UBorder> BodyStrikeLine;
    };

    struct FWireframeCallout
    {
        FName StableId;
        FVector2D AnchorPosition = FVector2D::ZeroVector;
        FVector2D LabelPosition = FVector2D::ZeroVector;
        FVector2D LabelSize = FVector2D(140.0f, 22.0f);
        FText PlayerName;
        TArray<FText> StatusTexts;
        FLinearColor PlayerColor = FLinearColor::White;
        bool bRightSide = false;
        int32 FontSize = 14;
    };

    bool CacheWidgetTreeReferences();
    void BindStateDelegates();
    void UnbindStateDelegates();
    void RebindObservedPlayerState();
    void RebindObservedSlotsAndParts();
    void UnbindObservedSlotsAndParts();
    void RefreshAll();
    void RefreshStamina();
    void RefreshBodySegments();
    void RefreshAssignedParts();
    void RefreshPartHealth();
    void ResetPartSlotVisual(FPartSlotVisual& Visual);
    void SetControlSlotHighlighted(int32 ControlIndex, bool bPressed);
    void InitializeWireframeHUD();
    void TeardownWireframeHUD();
    void UpdateWireframePanelLayout(const FGeometry& MyGeometry);
    void UpdateWireframeCameraInput();
    void InitializeRetryVoteHUD();
    void RefreshRetryVoteHUD();
    void InitializeApmHUD();
    void RefreshApmHUD();
    void RefreshWireframeCallouts(
        const FGeometry& MyGeometry,
        float InDeltaTime
    );
    UCurveLinearColor* GetOrCreateWireframeHealthCurve();
    UTexture2D* GetPartTexture(const ACMPartActorBase* PartActor) const;
    FText GetPartLabel(const ACMPartActorBase* PartActor) const;

    UFUNCTION()
    void HandleControlSlotsChanged();

    UFUNCTION()
    void HandleControlInputChanged(int32 SlotIndex, bool bPressed);

    UFUNCTION()
    void HandleControlPlayerStateChanged();

    UFUNCTION()
    void HandleSegmentStatesChanged();

    UFUNCTION()
    void HandlePartAttachmentChanged(
        UCMPartSlotComponent* PartSlot,
        AActor* AttachedPart
    );

    UFUNCTION()
    void HandlePartHealthChanged(
        float PreviousHealth,
        float CurrentHealth,
        float MaxHealth
    );

    UFUNCTION()
    void HandlePlayerColorChanged();

    void HandleStaminaChanged(const FOnAttributeChangeData& ChangeData);

    TWeakObjectPtr<ACMControlBody> ControlBody;
    TWeakObjectPtr<ACMChimera> SharedChimera;
    TWeakObjectPtr<ACMPlayerState> ObservedPlayerState;

    UPROPERTY(Transient)
    TObjectPtr<UWidget> CachedHUDContainer;

    UPROPERTY(Transient)
    TObjectPtr<UProgressBar> CachedStaminaProgressBar;

    UPROPERTY(Transient)
    TObjectPtr<UImage> WireframeRenderImage;

    UPROPERTY(Transient)
    TObjectPtr<UWidget> RetryVotePanelRoot;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> RetryVoteTitleText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> RetryVoteStatusText;

    UPROPERTY(Transient)
    TObjectPtr<UProgressBar> RetryVoteHoldProgressBar;

    UPROPERTY(Transient)
    TObjectPtr<UWidget> ApmPanelRoot;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ApmText;

    UPROPERTY(Transient)
    TObjectPtr<UCanvasPanel> WireframeCanvas;

    UPROPERTY(Transient)
    TObjectPtr<ACMWireframeHUDCaptureActor> WireframeCaptureActor;

    UPROPERTY(Transient)
    TObjectPtr<UCurveLinearColor> TransientHealthColorCurve;

    TArray<FSegmentRowVisual> SegmentRows;
    TArray<FPartSlotVisual> PhysicalPartSlots;
    TArray<FWireframeCallout> WireframeCallouts;
    TMap<FName, FVector2D> SmoothedCalloutPositions;
    TMap<FName, bool> CalloutRightSideById;
    FVector2D WireframeImageTopLeft = FVector2D::ZeroVector;
    FVector2D WireframeImageSize = FVector2D::ZeroVector;
    float RuntimeWireframeZoom = 1.0f;
    float ApmRefreshElapsed = 0.0f;
    TArray<TWeakObjectPtr<UCMPartSlotComponent>> ObservedPartSlots;
    TArray<TWeakObjectPtr<ACMPartActorBase>> ObservedParts;
    FDelegateHandle StaminaChangedHandle;
    FDelegateHandle MaxStaminaChangedHandle;
};
