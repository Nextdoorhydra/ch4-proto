#pragma once

#include "Templates/SharedPointer.h"

class SWidget;
class UDataForgeRuleSet;

TSharedRef<SWidget> CreateDataForgeSemanticDiffWidget(UDataForgeRuleSet& RuleSet);
