#pragma once

#include "NativeGameplayTags.h"

namespace AsyncPDALoaderTags
{
	// 게임 모듈이 UAsyncPDALoader에게 로드를 요청/완료 통지를 받을 때 쓰는 메시지 채널 
	ASYNCPDALOADER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Load_Request);
	ASYNCPDALOADER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Load_Complete);
}

