#pragma once

#include "ListenServerNetworkTypes.h"

namespace ListenServerNetworkDiagnostics
{
	EListenServerConnectionQuality EvaluateQuality(
		int32 PingMilliseconds,
		float IncomingPacketLossPercent,
		float OutgoingPacketLossPercent
	);
}
