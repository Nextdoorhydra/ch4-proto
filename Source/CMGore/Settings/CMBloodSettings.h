#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "CMBloodSettings.generated.h"

class UCMBloodDefinitionRegistry;


/**
 * CMGore 프로젝트 전역 설정.
 */
UCLASS(
	Config = Game,
	DefaultConfig,
	meta = (DisplayName = "CM Gore")
)
class CMGORE_API UCMBloodSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/**
	 * 프로젝트에서 사용하는 Blood Definition Registry.
	 */
	UPROPERTY(
		Config,
		EditAnywhere,
		Category = "Blood Definitions"
	)
	TSoftObjectPtr<UCMBloodDefinitionRegistry> DefinitionRegistry;

	/**
	 * 메시지에서 DefinitionId가 지정되지 않았거나
	 * 잘못된 ID가 전달되었을 때 사용할 기본 Definition.
	 */
	UPROPERTY(
		Config,
		EditAnywhere,
		Category = "Blood Definitions"
	)
	FName DefaultDefinitionId = NAME_None;
};