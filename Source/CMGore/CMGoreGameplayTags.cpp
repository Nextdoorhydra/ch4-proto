#include "CMGoreGameplayTags.h"

namespace CMGoreGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Root,
		"CMGore",
		"Root gameplay tag for the CMGore runtime system."
	);

	namespace Message
	{
		namespace Blood
		{
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(
				Impact,
				"CM.Message.Gore.Blood.Impact",
				"Blood impact message channel."
			);

			UE_DEFINE_GAMEPLAY_TAG_COMMENT(
				Burst,
				"CM.Message.Gore.Blood.Burst",
				"Blood burst message channel."
			);

			namespace Bleed
			{
				UE_DEFINE_GAMEPLAY_TAG_COMMENT(
					Start,
					"CM.Message.Gore.Blood.Bleed.Start",
					"Starts a continuous blood bleeding source."
				);

				UE_DEFINE_GAMEPLAY_TAG_COMMENT(
					Stop,
					"CM.Message.Gore.Blood.Bleed.Stop",
					"Stops a continuous blood bleeding source."
				);
			}

			namespace Pool
			{
				UE_DEFINE_GAMEPLAY_TAG_COMMENT(
					Start,
					"CM.Message.Gore.Blood.Pool.Start",
					"Starts a blood pool source."
				);

				UE_DEFINE_GAMEPLAY_TAG_COMMENT(
					Stop,
					"CM.Message.Gore.Blood.Pool.Stop",
					"Stops a blood pool source."
				);
			}
		}
	}
}