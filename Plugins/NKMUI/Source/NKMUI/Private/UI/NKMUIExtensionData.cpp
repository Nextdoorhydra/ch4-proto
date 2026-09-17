#include "UI/NKMUIExtensionData.h"

void UNKMUIExtensionData::GetExtensionEntries(TArray<FNKMUIExtensionEntry>& OutEntries) const
{
	OutEntries = ExtensionEntries;
}
