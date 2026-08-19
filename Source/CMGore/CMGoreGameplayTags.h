#pragma once

#include "NativeGameplayTags.h"

namespace CMGoreGameplayTags
{
	CMGORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root);

	namespace Message
	{
		namespace Blood
		{
			CMGORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Impact);
			CMGORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Burst);

			namespace Bleed
			{
				CMGORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Start);
				CMGORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stop);
			}

			namespace Pool
			{
				CMGORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Start);
				CMGORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stop);
			}
		}
	}
}