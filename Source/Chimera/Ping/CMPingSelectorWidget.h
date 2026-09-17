#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "Ping/CMPingTypes.h"

#include "CMPingSelectorWidget.generated.h"

class SCMPingRadialPanel;
class UTexture2D;

/** Local radial menu shown while either Alt key is held. */
UCLASS()
class CHIMERA_API UCMPingSelectorWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void BeginSelection(const FVector2D& ScreenPosition);
    void UpdateSelection(const FVector2D& Drag);
    bool GetSelectedType(ECMPingType& OutType) const;

#if WITH_DEV_AUTOMATION_TESTS
    void LoadIconTexturesForTest();
    bool HasValidIconTextures() const;
#endif

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    void RefreshSelection();
    void LoadIconTextures();
    static UTexture2D* LoadIcon(ECMPingType Type);

    ECMPingType SelectedType = ECMPingType::GoHere;
    bool bHasSelection = false;
    FVector2D CurrentDrag = FVector2D::ZeroVector;
    FSlateBrush GoHereBrush;
    FSlateBrush LookHereBrush;
    FSlateBrush SwapPartsBrush;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> GoHereTexture;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> LookHereTexture;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> SwapPartsTexture;

    TSharedPtr<SCMPingRadialPanel> RadialPanel;
};
