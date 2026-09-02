// Safe editor-side bridge for wiring Control Rig controls to AnimInstance properties.
// This deliberately uses reflection for private serialized node fields so the project can
// configure an AnimGraphNode_ControlRig without relying on crash-prone Python graph surgery.

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Animation/AnimBlueprint.h"
#include "AnimGraphNode_ControlRig.h"
#include "AnimGraphNode_CustomProperty.h"
#include "AnimNode_ControlRig.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UnrealType.h"

namespace ChimeraControlRigAnimMapping
{
    static FProperty* FindProperty(UStruct* Struct, const TCHAR* Name)
    {
        return Struct ? Struct->FindPropertyByName(FName(Name)) : nullptr;
    }

    static bool SetExposedControlPins(
        UAnimGraphNode_ControlRig& ControlRigNode,
        const TSet<FName>& Controls,
        FString& OutError)
    {
        FArrayProperty* CustomPinProperty = CastField<FArrayProperty>(
            FindProperty(UAnimGraphNode_CustomProperty::StaticClass(), TEXT("CustomPinProperties")));
        FStructProperty* RecordProperty = CustomPinProperty
            ? CastField<FStructProperty>(CustomPinProperty->Inner)
            : nullptr;
        if (!CustomPinProperty || !RecordProperty)
        {
            OutError = TEXT("Unable to locate Control Rig CustomPinProperties reflection property.");
            return false;
        }

        FNameProperty* NameProperty = CastField<FNameProperty>(
            FindProperty(RecordProperty->Struct, TEXT("PropertyName")));
        FBoolProperty* ShowProperty = CastField<FBoolProperty>(
            FindProperty(RecordProperty->Struct, TEXT("bShowPin")));
        FBoolProperty* CustomizedProperty = CastField<FBoolProperty>(
            FindProperty(RecordProperty->Struct, TEXT("bPropertyIsCustomized")));
        if (!NameProperty || !ShowProperty || !CustomizedProperty)
        {
            OutError = TEXT("Unable to locate optional-pin record fields.");
            return false;
        }

        void* ArrayData = CustomPinProperty->ContainerPtrToValuePtr<void>(&ControlRigNode);
        FScriptArrayHelper ArrayHelper(CustomPinProperty, ArrayData);
        bool bFoundAny = false;
        for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
        {
            uint8* RecordData = ArrayHelper.GetRawPtr(Index);
            const FName PropertyName = NameProperty->GetPropertyValue(
                NameProperty->ContainerPtrToValuePtr<void>(RecordData));
            if (Controls.Contains(PropertyName))
            {
                ShowProperty->SetPropertyValue(
                    ShowProperty->ContainerPtrToValuePtr<void>(RecordData), true);
                CustomizedProperty->SetPropertyValue(
                    CustomizedProperty->ContainerPtrToValuePtr<void>(RecordData), true);
                bFoundAny = true;
            }
        }

        if (!bFoundAny)
        {
            OutError = TEXT("Control Rig optional-pin records were not generated for the assigned class.");
            return false;
        }
        return true;
    }

    static void Configure(const TArray<FString>& Args)
    {
        const FString BlueprintPath = Args.Num() > 0
            ? Args[0]
            : TEXT("/Game/Chimera/ABP_CMLegLProcedural");

        UAnimBlueprint* AnimBlueprint = LoadObject<UAnimBlueprint>(nullptr, *BlueprintPath);
        if (!AnimBlueprint)
        {
            UE_LOG(LogTemp, Error, TEXT("[Chimera] Control Rig mapping failed: AnimBlueprint '%s' not found."), *BlueprintPath);
            return;
        }

        TArray<UEdGraph*> Graphs;
        AnimBlueprint->GetAllGraphs(Graphs);
        UAnimGraphNode_ControlRig* ControlRigNode = nullptr;
        for (UEdGraph* Graph : Graphs)
        {
            if (!Graph)
            {
                continue;
            }
            for (UEdGraphNode* GraphNode : Graph->Nodes)
            {
                if (UAnimGraphNode_ControlRig* Candidate = Cast<UAnimGraphNode_ControlRig>(GraphNode))
                {
                    ControlRigNode = Candidate;
                    break;
                }
            }
            if (ControlRigNode)
            {
                break;
            }
        }

        if (!ControlRigNode)
        {
            UE_LOG(LogTemp, Error, TEXT("[Chimera] Control Rig mapping failed: no AnimGraphNode_ControlRig found in '%s'."), *BlueprintPath);
            return;
        }

        ControlRigNode->Modify();
        // Refresh optional records after assigning the Control Rig class. This is an editor-only
        // operation and does not remove or recreate graph nodes (which avoids the prior crash path).
        ControlRigNode->ReconstructNode();

        FString Error;
        const TSet<FName> Controls = {
            FName(TEXT("CTRL_CM_LegHip")),
            FName(TEXT("CTRL_CM_LegFootIK")),
            FName(TEXT("CTRL_CM_LegKneePole")),
            FName(TEXT("CTRL_CM_LegPlantAlpha")),
        };
        if (!SetExposedControlPins(*ControlRigNode, Controls, Error))
        {
            UE_LOG(LogTemp, Error, TEXT("[Chimera] Control Rig pin exposure failed: %s"), *Error);
            return;
        }

        // Rebuild the graph pins after toggling bShowPin. This preserves the serialized
        // optional-pin records and materializes the four control inputs on the node.
        ControlRigNode->ReconstructNode();

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
        AnimBlueprint->MarkPackageDirty();
        UE_LOG(LogTemp, Display,
            TEXT("[Chimera] Configured Control Rig mappings and exposed pins for '%s' (Pins=%d)."),
            *BlueprintPath,
            ControlRigNode->Pins.Num());
        for (const UEdGraphPin* Pin : ControlRigNode->Pins)
        {
            if (Pin)
            {
                UE_LOG(LogTemp, Verbose, TEXT("[Chimera] Control Rig pin: %s"), *Pin->PinName.ToString());
            }
        }
    }

    static FAutoConsoleCommand ConfigureCommand(
        TEXT("Chimera.ConfigureLegControlRigAnimNode"),
        TEXT("Map planted-foot AnimInstance targets to the leg Control Rig node. Optional arg: AnimBlueprint path."),
        FConsoleCommandWithArgsDelegate::CreateStatic(&Configure));

    static UEdGraphNode* FindNodeByObjectName(UEdGraph& Graph, const FName NodeName)
    {
        for (UEdGraphNode* Node : Graph.Nodes)
        {
            if (Node && Node->GetFName() == NodeName)
            {
                return Node;
            }
        }
        return nullptr;
    }

    static UEdGraphPin* FindPosePin(
        UEdGraphNode& Node,
        const FName PinName,
        const EEdGraphPinDirection Direction)
    {
        for (UEdGraphPin* Pin : Node.Pins)
        {
            if (Pin && Pin->PinName == PinName && Pin->Direction == Direction)
            {
                return Pin;
            }
        }
        return nullptr;
    }

    // The migrated leg graph used to feed the rigid-body node through the legacy
    // Modify Bone -> Two Bone IK path, then solved the same thigh/calf/foot chain
    // again in the modern Control Rig. Bypass that obsolete solve while preserving
    // the rigid-body pass and the final Control Rig.
    static void RepairLegSkinning(const TArray<FString>& Args)
    {
        const FString BlueprintPath = Args.Num() > 0
            ? Args[0]
            : TEXT("/Game/Chimera/ABP_CMLegLProcedural");

        UAnimBlueprint* AnimBlueprint = LoadObject<UAnimBlueprint>(nullptr, *BlueprintPath);
        if (!AnimBlueprint)
        {
            UE_LOG(LogTemp, Error,
                TEXT("[Chimera] Leg skinning repair failed: AnimBlueprint '%s' not found."),
                *BlueprintPath);
            return;
        }

        UEdGraph* AnimGraph = nullptr;
        TArray<UEdGraph*> Graphs;
        AnimBlueprint->GetAllGraphs(Graphs);
        for (UEdGraph* Graph : Graphs)
        {
            if (Graph && Graph->GetFName() == FName(TEXT("AnimGraph")))
            {
                AnimGraph = Graph;
                break;
            }
        }
        if (!AnimGraph)
        {
            UE_LOG(LogTemp, Error,
                TEXT("[Chimera] Leg skinning repair failed: AnimGraph not found in '%s'."),
                *BlueprintPath);
            return;
        }

        UEdGraphNode* BaseComponentPoseNode = FindNodeByObjectName(
            *AnimGraph, FName(TEXT("AnimGraphNode_LocalToComponentSpace_0")));
        UEdGraphNode* RigidBodyNode = FindNodeByObjectName(
            *AnimGraph, FName(TEXT("AnimGraphNode_RigidBody_0")));
        if (!BaseComponentPoseNode || !RigidBodyNode)
        {
            UE_LOG(LogTemp, Error,
                TEXT("[Chimera] Leg skinning repair failed: expected base/rigid-body nodes are missing."));
            return;
        }

        UEdGraphPin* BaseOutput = FindPosePin(
            *BaseComponentPoseNode, FName(TEXT("ComponentPose")), EGPD_Output);
        UEdGraphPin* RigidBodyInput = FindPosePin(
            *RigidBodyNode, FName(TEXT("ComponentPose")), EGPD_Input);
        if (!BaseOutput || !RigidBodyInput)
        {
            UE_LOG(LogTemp, Error,
                TEXT("[Chimera] Leg skinning repair failed: expected component-pose pins are missing."));
            return;
        }

        if (RigidBodyInput->LinkedTo.Num() == 1 && RigidBodyInput->LinkedTo[0] == BaseOutput)
        {
            UE_LOG(LogTemp, Display,
                TEXT("[Chimera] Leg skinning graph is already repaired for '%s'."),
                *BlueprintPath);
            return;
        }

        AnimBlueprint->Modify();
        AnimGraph->Modify();
        BaseComponentPoseNode->Modify();
        RigidBodyNode->Modify();
        RigidBodyInput->Modify();
        RigidBodyInput->BreakAllPinLinks(true);

        const UEdGraphSchema* Schema = AnimGraph->GetSchema();
        if (!Schema || !Schema->TryCreateConnection(BaseOutput, RigidBodyInput))
        {
            UE_LOG(LogTemp, Error,
                TEXT("[Chimera] Leg skinning repair failed: could not connect base pose directly to rigid body."));
            return;
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
        FKismetEditorUtilities::CompileBlueprint(AnimBlueprint);
        AnimBlueprint->MarkPackageDirty();
        UE_LOG(LogTemp, Display,
            TEXT("[Chimera] Repaired leg AnimGraph '%s': bypassed legacy ModifyBone/TwoBoneIK double solve."),
            *BlueprintPath);
    }

    static FAutoConsoleCommand RepairLegSkinningCommand(
        TEXT("Chimera.RepairLegAnimGraphSkinning"),
        TEXT("Bypass the legacy leg IK path and keep RigidBody -> modern Control Rig. Optional arg: AnimBlueprint path."),
        FConsoleCommandWithArgsDelegate::CreateStatic(&RepairLegSkinning));
}

#endif // WITH_EDITOR
