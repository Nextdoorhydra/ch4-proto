#pragma once

#include "CoreMinimal.h"

class UNKMSoundSubsystem;

class NKMSOUNDRUNTIME_API FNKMSoundHelper
{
public:
	static UNKMSoundSubsystem* Get(const UObject* WorldContextObject);
};
