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

	// ---------------------------------------------------------------------
	// Persistent Blood Surface
	// ---------------------------------------------------------------------

	UPROPERTY(
		Config,
		EditAnywhere,
		Category = "Blood Surface",
		meta = (ClampMin = "0"))
	int32 InitialBloodDecalPoolSize = 128;

	/**
	 * 하나의 Blood Event가 생성 가능한 최대 surface trace 수.
	 *
	 * Niagara particle collision 수와 무관하다.
	 */
	UPROPERTY(
		Config,
		EditAnywhere,
		Category = "Blood Surface",
		meta = (ClampMin = "0"))
	int32 MaxSurfaceSamplesPerEvent = 8;

	/**
	 * 하나의 World에 유지할 수 있는 logical Blood Mark 최대 개수.
	 *
	 * 초과 시 oldest mark부터 recycle.
	 */
	UPROPERTY(
		Config,
		EditAnywhere,
		Category = "Blood Surface",
		meta = (ClampMin = "0"))
	int32 MaxActiveBloodMarks = 1024;

	UPROPERTY(Config, EditAnywhere, Category = "Blood Stroke",
		meta = (ClampMin = "0"))
	int32 MaxActiveBloodStrokeMarks = 768;

	UPROPERTY(Config, EditAnywhere, Category = "Blood Stroke",
		meta = (ClampMin = "0"))
	int32 MaxStrokeStampsPerObject = 64;

	UPROPERTY(Config, EditAnywhere, Category = "Blood Stroke",
		meta = (ClampMin = "0"))
	int32 MaxStrokeStampsPerFrame = 32;

	UPROPERTY(Config, EditAnywhere, Category = "Blood Stroke",
		meta = (ClampMin = "0.1"))
	float MinimumStrokeSpacing = 8.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Blood Stroke",
		meta = (ClampMin = "0.0"))
	float StrokeCullDistance = 3000.0f;
};
