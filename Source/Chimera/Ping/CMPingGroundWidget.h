#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "Ping/CMPingTypes.h"

#include "CMPingGroundWidget.generated.h"

class SImage;

/** Circular world-space surface rendered by a horizontal WidgetComponent. */
UCLASS()
class CHIMERA_API UCMPingGroundWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetPingType(ECMPingType Type);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    void ApplyPresentation();

    ECMPingType PingType = ECMPingType::GoHere;
    FSlateBrush GlowBrush;
    FSlateBrush CircleBrush;
    TSharedPtr<SImage> GlowImage;
    TSharedPtr<SImage> CircleImage;
};
