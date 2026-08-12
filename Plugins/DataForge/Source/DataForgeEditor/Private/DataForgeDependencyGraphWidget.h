#pragma once

#include "CoreMinimal.h"

class SWidget;
class UDataForgeRuleSet;

TSharedRef<SWidget> CreateDataForgeDependencyGraphWidget(UDataForgeRuleSet& RuleSet);
