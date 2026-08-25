#include "Stage/Device/CMStageDoorBase.h"

ACMStageDoorBase::ACMStageDoorBase()
{
    bStartActive = false;
}

// Mechanism 활성 상태를 문의 열림 상태로 전달
void ACMStageDoorBase::HandleElementActiveChanged_Implementation(bool bIsActive)
{
    Super::HandleElementActiveChanged_Implementation(bIsActive);
    OnDoorOpenStateChanged(bIsActive);
}

// 문 전용 초기화 표현을 하위 구현에 전달
void ACMStageDoorBase::HandleElementReset_Implementation()
{
    Super::HandleElementReset_Implementation();
    OnDoorReset();
}
