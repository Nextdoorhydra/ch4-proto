#include "CMMenuButtonWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"

void UCMMenuButtonWidget::NativeConstruct()
{
    Super::NativeConstruct();

    TArray<UWidget*> Widgets;
    WidgetTree->GetAllWidgets(Widgets);
    for (UWidget* Widget : Widgets)
    {
        if (UButton* Button = Cast<UButton>(Widget))
        {
            InternalButton = Button;
            InternalButton->OnClicked.AddUniqueDynamic(
                this, &ThisClass::HandleButtonClicked);
            break;
        }
    }
}

void UCMMenuButtonWidget::NativeDestruct()
{
    if (InternalButton)
    {
        InternalButton->OnClicked.RemoveDynamic(
            this, &ThisClass::HandleButtonClicked);
        InternalButton = nullptr;
    }

    Super::NativeDestruct();
}

void UCMMenuButtonWidget::HandleButtonClicked()
{
    OnClicked.Broadcast();
}
