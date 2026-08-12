#include "DataForgeRuleSetFactory.h"

#include "DataForgeRuleSet.h"

UDataForgeRuleSetFactory::UDataForgeRuleSetFactory()
{
	SupportedClass = UDataForgeRuleSet::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UDataForgeRuleSetFactory::FactoryCreateNew(
	UClass* Class,
	UObject* InParent,
	FName Name,
	EObjectFlags Flags,
	UObject* Context,
	FFeedbackContext* Warn)
{
	UDataForgeRuleSet* RuleSet = NewObject<UDataForgeRuleSet>(InParent, Class, Name, Flags | RF_Transactional);
	RuleSet->RuleSetId = FGuid::NewGuid();
	return RuleSet;
}
