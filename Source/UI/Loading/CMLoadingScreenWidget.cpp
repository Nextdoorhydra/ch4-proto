#include "Loading/CMLoadingScreenWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/TextBlock.h"

void UCMLoadingScreenWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (LoadingSpinnerAnimation)
    {
        PlayAnimation(LoadingSpinnerAnimation, 0.0f, 0);
    }
}

void UCMLoadingScreenWidget::SetLoadingText(
    const FText& NewLoadingText)
{
    LoadingText->SetText(NewLoadingText);
}
