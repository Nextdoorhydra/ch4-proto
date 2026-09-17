#include "DataForgeDependencyGraph.h"

#include "DataForgeRuleSet.h"

namespace DataForgeDependencyGraph
{
	enum class EVisitState : uint8
	{
		Visiting,
		Visited
	};

	void AddError(TArray<FDataForgeDiagnostic>& Diagnostics, const TCHAR* Code, const FString& Message)
	{
		FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = EDataForgeSeverity::Error;
		Diagnostic.Code = Code;
		Diagnostic.Message = Message;
	}

	FString NodePath(const UDataForgeRuleSet& RuleSet)
	{
		return RuleSet.GetPathName();
	}

	bool Visit(
		const UDataForgeRuleSet& RuleSet,
		TMap<FString, EVisitState>& States,
		TArray<const UDataForgeRuleSet*>& Stack,
		TArray<const UDataForgeRuleSet*>& Order,
		TArray<FDataForgeDiagnostic>& Diagnostics)
	{
		const FString Path = NodePath(RuleSet);
		if (const EVisitState* State = States.Find(Path))
		{
			if (*State == EVisitState::Visited)
			{
				return true;
			}

			TArray<FString> Cycle;
			const int32 Start = Stack.IndexOfByPredicate([&Path](const UDataForgeRuleSet* Node)
			{
				return Node && NodePath(*Node) == Path;
			});
			for (int32 Index = FMath::Max(0, Start); Index < Stack.Num(); ++Index)
			{
				Cycle.Add(NodePath(*Stack[Index]));
			}
			Cycle.Add(Path);
			AddError(Diagnostics, TEXT("DF1302"), FString::Printf(TEXT("RuleSet dependency cycle detected: %s"), *FString::Join(Cycle, TEXT(" -> "))));
			return false;
		}

		States.Add(Path, EVisitState::Visiting);
		Stack.Add(&RuleSet);

		TArray<const FDataForgeDependencyRule*> Dependencies;
		for (const FDataForgeDependencyRule& Dependency : RuleSet.Dependencies)
		{
			Dependencies.Add(&Dependency);
		}
		Dependencies.Sort([](const FDataForgeDependencyRule& Left, const FDataForgeDependencyRule& Right)
		{
			return Left.RuleSet.ToSoftObjectPath().ToString() < Right.RuleSet.ToSoftObjectPath().ToString();
		});

		bool bSuccess = true;
		TSet<FString> SeenDependencies;
		for (const FDataForgeDependencyRule* Dependency : Dependencies)
		{
			const FSoftObjectPath DependencyPath = Dependency->RuleSet.ToSoftObjectPath();
			if (DependencyPath.IsNull())
			{
				AddError(Diagnostics, TEXT("DF1300"), FString::Printf(TEXT("RuleSet '%s' contains an empty dependency."), *Path));
				bSuccess = false;
				continue;
			}

			const FString DependencyPathString = DependencyPath.ToString();
			if (SeenDependencies.Contains(DependencyPathString))
			{
				continue;
			}
			SeenDependencies.Add(DependencyPathString);

			const UDataForgeRuleSet* DependencyRuleSet = Dependency->RuleSet.LoadSynchronous();
			if (!DependencyRuleSet)
			{
				AddError(Diagnostics, TEXT("DF1301"), FString::Printf(TEXT("RuleSet dependency could not be loaded: %s"), *DependencyPathString));
				bSuccess = false;
				continue;
			}
			bSuccess &= Visit(*DependencyRuleSet, States, Stack, Order, Diagnostics);
		}

		Stack.Pop();
		States[Path] = EVisitState::Visited;
		if (bSuccess)
		{
			Order.Add(&RuleSet);
		}
		return bSuccess;
	}
}

bool FDataForgeDependencyGraph::BuildExecutionOrder(
	TConstArrayView<const UDataForgeRuleSet*> Roots,
	TArray<const UDataForgeRuleSet*>& OutExecutionOrder,
	TArray<FDataForgeDiagnostic>& OutDiagnostics)
{
	OutExecutionOrder.Reset();

	TArray<const UDataForgeRuleSet*> SortedRoots;
	for (const UDataForgeRuleSet* Root : Roots)
	{
		if (Root)
		{
			SortedRoots.Add(Root);
		}
	}
	SortedRoots.Sort([](const UDataForgeRuleSet& Left, const UDataForgeRuleSet& Right)
	{
		return Left.GetPathName() < Right.GetPathName();
	});

	TMap<FString, DataForgeDependencyGraph::EVisitState> States;
	TArray<const UDataForgeRuleSet*> Stack;
	bool bSuccess = true;
	for (const UDataForgeRuleSet* Root : SortedRoots)
	{
		bSuccess &= DataForgeDependencyGraph::Visit(*Root, States, Stack, OutExecutionOrder, OutDiagnostics);
	}
	if (!bSuccess)
	{
		OutExecutionOrder.Reset();
	}
	return bSuccess;
}
