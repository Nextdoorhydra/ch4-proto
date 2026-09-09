#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "Ping/CMPingTypes.h"

#include "CMPingSelectorWidget.generated.h"

class SCMPingRadialPanel;

/** Local radial menu shown while either Alt key is held. */
UCLASS()
class CHIMERA_API UCMPingSelectorWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void BeginSelection(const FVector2D& ScreenPosition);
    void UpdateSelection(const FVector2D& Drag);
    bool GetSelectedType(ECMPingType& OutType) const;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    void RefreshSelection();
    static UObject* LoadIcon(ECMPingType Type);

    ECMPingType SelectedType = ECMPingType::GoHere;
    bool bHasSelection = false;
    FVector2D CurrentDrag = FVector2D::ZeroVector;
    FSlateBrush GoHereBrush;
    FSlateBrush LookHereBrush;
    FSlateBrush SwapPartsBrush;
    TSharedPtr<SCMPingRadialPanel> RadialPanel;
};
