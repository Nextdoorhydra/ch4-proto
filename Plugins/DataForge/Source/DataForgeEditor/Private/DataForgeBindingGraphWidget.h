#pragma once

#include "Templates/SharedPointer.h"

class SWidget;
class UDataForgeRuleSet;

TSharedRef<SWidget> CreateDataForgeBindingGraphWidget(UDataForgeRuleSet& RuleSet);
