#include "ListenServerNetworkDiagnostics.h"

namespace ListenServerNetworkDiagnostics
{
	EListenServerConnectionQuality EvaluateQuality(
		int32 PingMilliseconds,
		float IncomingPacketLossPercent,
		float OutgoingPacketLossPercent
	)
	{
		if (PingMilliseconds < 0)
		{
			return EListenServerConnectionQuality::Unknown;
		}

		const float PacketLossPercent = FMath::Max(IncomingPacketLossPercent, OutgoingPacketLossPercent);
		if (PingMilliseconds < 100 && PacketLossPercent < 2.0f)
		{
			return EListenServerConnectionQuality::Good;
		}
		if (PingMilliseconds < 200 && PacketLossPercent < 5.0f)
		{
			return EListenServerConnectionQuality::Fair;
		}
		return EListenServerConnectionQuality::Poor;
	}
}
