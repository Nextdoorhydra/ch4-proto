#include "Ping/CMPingSelectorWidget.h"

#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"

namespace
{
    constexpr float WheelSize = 320.0f;
    constexpr float OuterRadius = 126.0f;
    constexpr float InnerRadius = 42.0f;
    constexpr float IconRadius = 84.0f;

    FVector2f DirectionAtDegrees(float Degrees)
    {
        const float Radians = FMath::DegreesToRadians(Degrees);
        return FVector2f(FMath::Cos(Radians), FMath::Sin(Radians));
    }

    TArray<FVector2f> MakeArc(
        const FVector2f& Center,
        float Radius,
        float StartDegrees,
        float EndDegrees,
        int32 SegmentCount = 32)
    {
        TArray<FVector2f> Points;
        Points.Reserve(SegmentCount + 1);
        for (int32 Index = 0; Index <= SegmentCount; ++Index)
        {
            const float Alpha = static_cast<float>(Index) / SegmentCount;
            Points.Add(Center + DirectionAtDegrees(
                FMath::Lerp(StartDegrees, EndDegrees, Alpha)) * Radius);
        }
        return Points;
    }
}

class SCMPingRadialPanel final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCMPingRadialPanel) {}
        SLATE_ARGUMENT(const FSlateBrush*, GoHereBrush)
        SLATE_ARGUMENT(const FSlateBrush*, LookHereBrush)
        SLATE_ARGUMENT(const FSlateBrush*, SwapPartsBrush)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        Brushes[0] = InArgs._GoHereBrush;
        Brushes[1] = InArgs._LookHereBrush;
        Brushes[2] = InArgs._SwapPartsBrush;
    }

    void SetSelection(
        const FVector2D& InDrag,
        bool bInHasSelection,
        ECMPingType InSelectedType)
    {
        Drag = FVector2f(InDrag);
        bHasSelection = bInHasSelection;
        SelectedType = InSelectedType;
        Invalidate(EInvalidateWidgetReason::Paint);
    }

    virtual FVector2D ComputeDesiredSize(float) const override
    {
        return FVector2D(WheelSize, WheelSize);
    }

    virtual int32 OnPaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled) const override
    {
        const FVector2f Center = AllottedGeometry.GetLocalSize() * 0.5f;
        const FLinearColor FrameColor(0.76f, 0.75f, 0.73f, 0.94f);
        const FLinearColor DividerColor(0.22f, 0.21f, 0.22f, 0.98f);

        const TArray<FVector2f> OuterArc = MakeArc(
            Center, OuterRadius, -180.0f, 180.0f, 72);
        const TArray<FVector2f> InnerArc = MakeArc(
            Center, InnerRadius, -180.0f, 180.0f, 40);
        FSlateDrawElement::MakeLines(
            OutDrawElements, LayerId,
            AllottedGeometry.ToPaintGeometry(), OuterArc,
            ESlateDrawEffect::None, FrameColor, true, 18.0f);
        FSlateDrawElement::MakeLines(
            OutDrawElements, LayerId + 1,
            AllottedGeometry.ToPaintGeometry(), OuterArc,
            ESlateDrawEffect::None, FrameColor, true, 2.0f);
        FSlateDrawElement::MakeLines(
            OutDrawElements, LayerId + 1,
            AllottedGeometry.ToPaintGeometry(), InnerArc,
            ESlateDrawEffect::None, DividerColor, true, 3.0f);

        const float DividerAngles[] = { -30.0f, 90.0f, 210.0f };
        for (const float Angle : DividerAngles)
        {
            const FVector2f Direction = DirectionAtDegrees(Angle);
            const TArray<FVector2f> Divider = {
                Center + Direction * InnerRadius,
                Center + Direction * OuterRadius
            };
            FSlateDrawElement::MakeLines(
                OutDrawElements, LayerId + 1,
                AllottedGeometry.ToPaintGeometry(), Divider,
                ESlateDrawEffect::None, DividerColor, true, 3.0f);
        }

        if (bHasSelection)
        {
            float StartDegrees = -150.0f;
            switch (SelectedType)
            {
            case ECMPingType::LookHere:
                StartDegrees = -30.0f;
                break;
            case ECMPingType::SwapParts:
                StartDegrees = 90.0f;
                break;
            default:
                break;
            }
            const TArray<FVector2f> Highlight = MakeArc(
                Center, OuterRadius, StartDegrees,
                StartDegrees + 120.0f, 24);
            FSlateDrawElement::MakeLines(
                OutDrawElements, LayerId + 2,
                AllottedGeometry.ToPaintGeometry(), Highlight,
                ESlateDrawEffect::None,
                CMPing::GetTypeColor(SelectedType), true, 18.0f);
        }

        const float IconAngles[] = { -90.0f, 30.0f, 150.0f };
        constexpr float IconSize = 54.0f;
        for (int32 Index = 0; Index < 3; ++Index)
        {
            if (!Brushes[Index])
            {
                continue;
            }
            const FVector2f IconCenter = Center
                + DirectionAtDegrees(IconAngles[Index]) * IconRadius;
            FSlateDrawElement::MakeBox(
                OutDrawElements, LayerId + 3,
                AllottedGeometry.ToPaintGeometry(
                    FVector2f(IconSize),
                    FSlateLayoutTransform(
                        IconCenter - FVector2f(IconSize * 0.5f))),
                Brushes[Index], ESlateDrawEffect::None,
                InWidgetStyle.GetColorAndOpacityTint());
        }

        const float DragLength = Drag.Size();
        if (DragLength > 1.0f)
        {
            const FVector2f LineEnd = Center + Drag.GetSafeNormal()
                * FMath::Min(DragLength, OuterRadius - 10.0f);
            const TArray<FVector2f> GuideLine = { Center, LineEnd };
            FSlateDrawElement::MakeLines(
                OutDrawElements, LayerId + 4,
                AllottedGeometry.ToPaintGeometry(), GuideLine,
                ESlateDrawEffect::None,
                bHasSelection ? CMPing::GetTypeColor(SelectedType)
                    : FLinearColor::White,
                true, 5.0f);
        }

        return LayerId + 5;
    }

private:
    const FSlateBrush* Brushes[3] = {};
    FVector2f Drag = FVector2f::ZeroVector;
    ECMPingType SelectedType = ECMPingType::GoHere;
    bool bHasSelection = false;
};

TSharedRef<SWidget> UCMPingSelectorWidget::RebuildWidget()
{
    const auto ConfigureBrush = [](FSlateBrush& Brush, UObject* Resource)
    {
        Brush.DrawAs = ESlateBrushDrawType::Image;
        Brush.ImageSize = FVector2D(54.0f, 54.0f);
        Brush.SetResourceObject(Resource);
    };
    ConfigureBrush(GoHereBrush, LoadIcon(ECMPingType::GoHere));
    ConfigureBrush(LookHereBrush, LoadIcon(ECMPingType::LookHere));
    ConfigureBrush(SwapPartsBrush, LoadIcon(ECMPingType::SwapParts));

    TSharedRef<SWidget> Root =
        SAssignNew(RadialPanel, SCMPingRadialPanel)
        .GoHereBrush(&GoHereBrush)
        .LookHereBrush(&LookHereBrush)
        .SwapPartsBrush(&SwapPartsBrush);
    RefreshSelection();
    return Root;
}

void UCMPingSelectorWidget::BeginSelection(
    const FVector2D& ScreenPosition)
{
    SetDesiredSizeInViewport(FVector2D(WheelSize, WheelSize));
    SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
    SetPositionInViewport(ScreenPosition, false);
    CurrentDrag = FVector2D::ZeroVector;
    bHasSelection = false;
    RefreshSelection();
}

void UCMPingSelectorWidget::UpdateSelection(const FVector2D& Drag)
{
    CurrentDrag = Drag;
    ECMPingType NewType = SelectedType;
    bHasSelection = CMPing::TrySelectTypeFromDrag(Drag, NewType);
    if (bHasSelection)
    {
        SelectedType = NewType;
    }
    RefreshSelection();
}

bool UCMPingSelectorWidget::GetSelectedType(ECMPingType& OutType) const
{
    if (!bHasSelection)
    {
        return false;
    }
    OutType = SelectedType;
    return true;
}

void UCMPingSelectorWidget::RefreshSelection()
{
    if (RadialPanel)
    {
        RadialPanel->SetSelection(CurrentDrag, bHasSelection, SelectedType);
    }
}

UObject* UCMPingSelectorWidget::LoadIcon(ECMPingType Type)
{
    const TCHAR* AssetPath = nullptr;
    switch (Type)
    {
    case ECMPingType::GoHere:
        AssetPath = TEXT("/Game/Chimera/UI/Ping/T_Ping_GoHere.T_Ping_GoHere");
        break;
    case ECMPingType::LookHere:
        AssetPath = TEXT("/Game/Chimera/UI/Ping/T_Ping_LookHere.T_Ping_LookHere");
        break;
    case ECMPingType::SwapParts:
        AssetPath = TEXT("/Game/Chimera/UI/Ping/T_Ping_SwapParts.T_Ping_SwapParts");
        break;
    }
    return AssetPath ? LoadObject<UTexture2D>(nullptr, AssetPath) : nullptr;
}
