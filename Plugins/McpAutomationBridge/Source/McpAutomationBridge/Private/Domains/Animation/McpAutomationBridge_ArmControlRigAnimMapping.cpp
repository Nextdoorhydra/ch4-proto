// Project-specific, idempotent authoring bridge for the production arm Anim Blueprint.

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimNodeBase.h"
#include "AnimGraphNode_ControlRig.h"
#include "AnimGraphNode_CustomProperty.h"
#include "AnimGraphNode_Root.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
#include "ControlRigBlueprintLegacy.h"
#else
#include "ControlRigBlueprint.h"
#endif
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "HAL/IConsoleManager.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Safety/McpSafeOperations.h"
#include "UObject/UnrealType.h"

namespace ChimeraArmControlRigMapping
{
static bool IsPosePin(const UEdGraphPin* Pin)
{
    const UScriptStruct* Struct = Pin
        ? Cast<UScriptStruct>(Pin->PinType.PinSubCategoryObject.Get())
        : nullptr;
    return Struct && Struct->IsChildOf(FPoseLink::StaticStruct());
}

static bool AssignControlRigClass(
    UAnimGraphNode_ControlRig& Node,
    UClass* RigClass)
{
    FObjectPropertyBase* Property = CastField<FObjectPropertyBase>(
        FAnimNode_ControlRig::StaticStruct()->FindPropertyByName(
            TEXT("ControlRigClass")));
    if (!Property || !RigClass)
    {
        return false;
    }
    Property->SetObjectPropertyValue_InContainer(&Node.Node, RigClass);
    return true;
}

static UEdGraphPin* FindPosePin(UEdGraphNode& Node, EEdGraphPinDirection Direction)
{
    for (UEdGraphPin* Pin : Node.Pins)
    {
        if (Pin && Pin->Direction == Direction && IsPosePin(Pin))
        {
            return Pin;
        }
    }
    return nullptr;
}

static bool ExposeControls(
    UAnimGraphNode_ControlRig& Node,
    const TSet<FName>& ControlNames)
{
    FArrayProperty* ArrayProperty = CastField<FArrayProperty>(
        UAnimGraphNode_CustomProperty::StaticClass()->FindPropertyByName(
            TEXT("CustomPinProperties")));
    FStructProperty* RecordProperty = ArrayProperty
        ? CastField<FStructProperty>(ArrayProperty->Inner)
        : nullptr;
    if (!ArrayProperty || !RecordProperty)
    {
        return false;
    }

    FNameProperty* NameProperty = CastField<FNameProperty>(
        RecordProperty->Struct->FindPropertyByName(TEXT("PropertyName")));
    FBoolProperty* ShowProperty = CastField<FBoolProperty>(
        RecordProperty->Struct->FindPropertyByName(TEXT("bShowPin")));
    FBoolProperty* CustomizedProperty = CastField<FBoolProperty>(
        RecordProperty->Struct->FindPropertyByName(
            TEXT("bPropertyIsCustomized")));
    if (!NameProperty || !ShowProperty || !CustomizedProperty)
    {
        return false;
    }

    void* ArrayData = ArrayProperty->ContainerPtrToValuePtr<void>(&Node);
    FScriptArrayHelper Records(ArrayProperty, ArrayData);
    int32 ExposedCount = 0;
    for (int32 Index = 0; Index < Records.Num(); ++Index)
    {
        uint8* Record = Records.GetRawPtr(Index);
        const FName Name = NameProperty->GetPropertyValue(
            NameProperty->ContainerPtrToValuePtr<void>(Record));
        if (!ControlNames.Contains(Name))
        {
            continue;
        }
        ShowProperty->SetPropertyValue(
            ShowProperty->ContainerPtrToValuePtr<void>(Record), true);
        CustomizedProperty->SetPropertyValue(
            CustomizedProperty->ContainerPtrToValuePtr<void>(Record), true);
        ++ExposedCount;
    }
    return ExposedCount == ControlNames.Num();
}

static UK2Node_VariableGet* FindOrAddGetter(
    UEdGraph& Graph,
    const FName PropertyName,
    const int32 X,
    const int32 Y)
{
    for (UEdGraphNode* GraphNode : Graph.Nodes)
    {
        UK2Node_VariableGet* Getter = Cast<UK2Node_VariableGet>(GraphNode);
        if (Getter && Getter->VariableReference.GetMemberName() == PropertyName)
        {
            return Getter;
        }
    }

    FGraphNodeCreator<UK2Node_VariableGet> Creator(Graph);
    UK2Node_VariableGet* Getter = Creator.CreateNode();
    Getter->VariableReference.SetSelfMember(PropertyName);
    Getter->NodePosX = X;
    Getter->NodePosY = Y;
    Creator.Finalize();
    return Getter;
}

static bool ConnectGetter(
    UEdGraph& Graph,
    UK2Node_VariableGet& Getter,
    UAnimGraphNode_ControlRig& RigNode,
    const FName ControlName)
{
    UEdGraphPin* Source = Getter.GetValuePin();
    UEdGraphPin* Target = RigNode.FindPin(ControlName, EGPD_Input);
    if (!Source || !Target)
    {
        return false;
    }
    Target->BreakAllPinLinks(true);
    return Graph.GetSchema()->TryCreateConnection(Source, Target);
}

static void Configure()
{
    const TCHAR* AnimPath = TEXT("/Game/Chimera/ABP_CMArmLProcedural");
    const TCHAR* RigPath = TEXT(
        "/Game/Chimera/Character/Part/Arm/CR_CMArmLProcedural");
    UAnimBlueprint* AnimBlueprint = LoadObject<UAnimBlueprint>(nullptr, AnimPath);
    UControlRigBlueprint* RigBlueprint =
        LoadObject<UControlRigBlueprint>(nullptr, RigPath);
    if (!AnimBlueprint || !RigBlueprint || !RigBlueprint->GetControlRigClass())
    {
        UE_LOG(LogTemp, Error,
            TEXT("[Chimera] Arm Control Rig mapping assets are unavailable."));
        return;
    }

    UEdGraph* AnimGraph = nullptr;
    for (UEdGraph* Graph : AnimBlueprint->FunctionGraphs)
    {
        if (Graph && Graph->GetFName() == TEXT("AnimGraph"))
        {
            AnimGraph = Graph;
            break;
        }
    }
    if (!AnimGraph)
    {
        UE_LOG(LogTemp, Error, TEXT("[Chimera] Arm AnimGraph was not found."));
        return;
    }

    UAnimGraphNode_ControlRig* RigNode = nullptr;
    UAnimGraphNode_Root* RootNode = nullptr;
    for (UEdGraphNode* GraphNode : AnimGraph->Nodes)
    {
        if (!RigNode)
        {
            RigNode = Cast<UAnimGraphNode_ControlRig>(GraphNode);
        }
        if (!RootNode)
        {
            RootNode = Cast<UAnimGraphNode_Root>(GraphNode);
        }
    }
    if (!RootNode)
    {
        UE_LOG(LogTemp, Error, TEXT("[Chimera] Arm AnimGraph root was not found."));
        return;
    }

    if (!RigNode)
    {
        FGraphNodeCreator<UAnimGraphNode_ControlRig> Creator(*AnimGraph);
        RigNode = Creator.CreateNode();
        AssignControlRigClass(*RigNode, RigBlueprint->GetControlRigClass());
        RigNode->NodePosX = RootNode->NodePosX - 360;
        RigNode->NodePosY = RootNode->NodePosY;
        RigNode->NodeComment = TEXT("CM Arm Procedural Control Rig");
        Creator.Finalize();
    }
    else
    {
        RigNode->Modify();
        AssignControlRigClass(*RigNode, RigBlueprint->GetControlRigClass());
        RigNode->ReconstructNode();
    }

    const TSet<FName> Controls = {
        TEXT("CTRL_CM_ArmHandIK"),
        TEXT("CTRL_CM_ArmElbowPole"),
        TEXT("CTRL_CM_ArmIKAlpha"),
    };
    if (!ExposeControls(*RigNode, Controls))
    {
        UE_LOG(LogTemp, Error,
            TEXT("[Chimera] Failed to expose all arm Control Rig inputs."));
        return;
    }
    RigNode->ReconstructNode();

    UEdGraphPin* RootInput = FindPosePin(*RootNode, EGPD_Input);
    UEdGraphPin* RigInput = FindPosePin(*RigNode, EGPD_Input);
    UEdGraphPin* RigOutput = FindPosePin(*RigNode, EGPD_Output);
    if (!RootInput || !RigInput || !RigOutput)
    {
        UE_LOG(LogTemp, Error, TEXT("[Chimera] Arm pose pins were not found."));
        return;
    }
    if (!RootInput->LinkedTo.Contains(RigOutput))
    {
        UEdGraphPin* PreviousOutput = RootInput->LinkedTo.Num() > 0
            ? RootInput->LinkedTo[0]
            : nullptr;
        RootInput->BreakAllPinLinks(true);
        if (PreviousOutput && PreviousOutput != RigOutput)
        {
            AnimGraph->GetSchema()->TryCreateConnection(
                PreviousOutput,
                RigInput);
        }
        AnimGraph->GetSchema()->TryCreateConnection(RigOutput, RootInput);
    }

    const int32 GetterX = RigNode->NodePosX - 360;
    const bool bHandConnected = ConnectGetter(
        *AnimGraph,
        *FindOrAddGetter(*AnimGraph, TEXT("ArmHandTargetTransform"), GetterX, 80),
        *RigNode,
        TEXT("CTRL_CM_ArmHandIK"));
    const bool bElbowConnected = ConnectGetter(
        *AnimGraph,
        *FindOrAddGetter(*AnimGraph, TEXT("ArmElbowTargetLocation"), GetterX, 190),
        *RigNode,
        TEXT("CTRL_CM_ArmElbowPole"));
    const bool bAlphaConnected = ConnectGetter(
        *AnimGraph,
        *FindOrAddGetter(*AnimGraph, TEXT("ArmHandIKAlpha"), GetterX, 300),
        *RigNode,
        TEXT("CTRL_CM_ArmIKAlpha"));

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
    FKismetEditorUtilities::CompileBlueprint(AnimBlueprint);
    McpSafeOperations::McpSafeAssetSave(AnimBlueprint);
    UE_LOG(LogTemp, Display,
        TEXT("[Chimera] Arm Control Rig mapped. Hand=%s Elbow=%s Alpha=%s"),
        bHandConnected ? TEXT("true") : TEXT("false"),
        bElbowConnected ? TEXT("true") : TEXT("false"),
        bAlphaConnected ? TEXT("true") : TEXT("false"));
}

static FAutoConsoleCommand ConfigureCommand(
    TEXT("Chimera.ConfigureArmControlRigAnimNode"),
    TEXT("Insert and wire the procedural arm Control Rig in its Anim Blueprint."),
    FConsoleCommandDelegate::CreateStatic(&Configure));
}

#endif
