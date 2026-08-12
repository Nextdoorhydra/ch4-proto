#pragma once

#include "IDetailCustomization.h"

class UDataForgeRuleSet;

class FDataForgeRuleSetCustomization final : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	TWeakObjectPtr<UDataForgeRuleSet> RuleSet;
};
