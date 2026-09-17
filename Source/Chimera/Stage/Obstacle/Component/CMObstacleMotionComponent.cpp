#include "Stage/Obstacle/Component/CMObstacleMotionComponent.h"

UCMObstacleMotionComponent::UCMObstacleMotionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

// 장애물의 레벨 배치 Transform을 초기 상태로 저장
void UCMObstacleMotionComponent::BeginPlay()
{
    Super::BeginPlay();
    if (const AActor* Owner = GetOwner())
    {
        InitialTransform = Owner->GetActorTransform();
    }
}

// 서버에서만 공통 회전, 단방향, 왕복 이동을 계산
void UCMObstacleMotionComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    AActor* Owner = GetOwner();
    if (!bMotionRunning || !IsValid(Owner) || !Owner->HasAuthority())
    {
        return;
    }

    switch (MotionType)
    {
    case ECMObstacleMotionType::Rotation:
        TickRotation(DeltaTime);
        break;
    case ECMObstacleMotionType::RotationToAngle:
        TickRotationToAngle(DeltaTime);
        break;
    case ECMObstacleMotionType::Linear:
        TickTranslation(DeltaTime, false);
        break;
    case ECMObstacleMotionType::PingPong:
        TickTranslation(DeltaTime, true);
        break;
    default:
        StopMotion();
        break;
    }
}

// 이동 상태를 시작하고 하위 구현에 알림
void UCMObstacleMotionComponent::StartMotion()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || MotionType == ECMObstacleMotionType::None)
    {
        return;
    }

    if (MotionType == ECMObstacleMotionType::RotationToAngle)
    {
        DirectionSign = 1.0f;
    }

    bMotionRunning = true;
    SetComponentTickEnabled(true);
    OnMotionStateChanged(bMotionRunning, DirectionSign);
}

// 이동 상태를 정지하고 하위 구현에 알림
void UCMObstacleMotionComponent::StopMotion()
{
    if (MotionType == ECMObstacleMotionType::RotationToAngle
        && GetOwner() && GetOwner()->HasAuthority()
        && CurrentRotationAngle > KINDA_SMALL_NUMBER)
    {
        DirectionSign = -1.0f;
        bMotionRunning = true;
        SetComponentTickEnabled(true);
        OnMotionStateChanged(bMotionRunning, DirectionSign);
        return;
    }

    bMotionRunning = false;
    SetComponentTickEnabled(false);
    OnMotionStateChanged(bMotionRunning, DirectionSign);
}

// 현재 이동 방향을 반전
void UCMObstacleMotionComponent::ReverseMotion()
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    DirectionSign *= -1.0f;
    OnMotionStateChanged(bMotionRunning, DirectionSign);
}

// 소유 액터를 배치 위치로 되돌리고 이동 상태를 초기화
void UCMObstacleMotionComponent::ResetMotion()
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    bMotionRunning = false;
    DirectionSign = 1.0f;
    TravelledDistance = 0.0f;
    CurrentRotationAngle = 0.0f;
    SetComponentTickEnabled(false);
    if (AActor* Owner = GetOwner(); IsValid(Owner))
    {
        Owner->SetActorTransform(InitialTransform);
    }
    OnMotionStateChanged(bMotionRunning, DirectionSign);
}

// 최초 배치 회전을 기준으로 로컬 이동축을 월드 방향으로 변환
FVector UCMObstacleMotionComponent::GetWorldMotionAxis() const
{
    return InitialTransform.TransformVectorNoScale(MotionAxis).GetSafeNormal();
}

// 설정 축을 중심으로 초당 Speed 도만큼 장애물 회전
void UCMObstacleMotionComponent::TickRotation(float DeltaTime)
{
    AActor* Owner = GetOwner();
    const FVector WorldAxis = GetWorldMotionAxis();
    if (!IsValid(Owner) || WorldAxis.IsNearlyZero() || FMath::IsNearlyZero(Speed))
    {
        return;
    }

    const float AngleRadians = FMath::DegreesToRadians(Speed * DirectionSign * DeltaTime);
    const FQuat DeltaRotation(WorldAxis, AngleRadians);
    Owner->SetActorRotation(DeltaRotation * Owner->GetActorQuat());
}

// 활성화 시 설정 각도까지 회전하고 비활성화 시 최초 배치 회전으로 복귀
void UCMObstacleMotionComponent::TickRotationToAngle(float DeltaTime)
{
    AActor* Owner = GetOwner();
    const FVector WorldAxis = GetWorldMotionAxis();
    if (!IsValid(Owner) || WorldAxis.IsNearlyZero() || RotationAngle <= 0.0f || FMath::IsNearlyZero(Speed))
    {
        return;
    }

    const float TargetAngle = DirectionSign > 0.0f ? RotationAngle : 0.0f;
    CurrentRotationAngle = FMath::FInterpConstantTo(
        CurrentRotationAngle,
        TargetAngle,
        DeltaTime,
        FMath::Abs(Speed));

    const FQuat RotationOffset(WorldAxis, FMath::DegreesToRadians(CurrentRotationAngle));
    Owner->SetActorRotation(RotationOffset * InitialTransform.GetRotation());

    if (FMath::IsNearlyEqual(CurrentRotationAngle, TargetAngle, KINDA_SMALL_NUMBER))
    {
        bMotionRunning = false;
        SetComponentTickEnabled(false);
        OnMotionStateChanged(bMotionRunning, DirectionSign);
    }
}

// 시작점과 설정 거리 사이에서 단방향 또는 왕복 이동 처리
void UCMObstacleMotionComponent::TickTranslation(float DeltaTime, bool bShouldPingPong)
{
    AActor* Owner = GetOwner();
    const FVector WorldAxis = GetWorldMotionAxis();
    if (!IsValid(Owner) || WorldAxis.IsNearlyZero() || MoveDistance <= 0.0f || FMath::IsNearlyZero(Speed))
    {
        return;
    }

    TravelledDistance += FMath::Abs(Speed) * DirectionSign * DeltaTime;

    if (bShouldPingPong)
    {
        while (TravelledDistance < 0.0f || TravelledDistance > MoveDistance)
        {
            if (TravelledDistance > MoveDistance)
            {
                TravelledDistance = MoveDistance - (TravelledDistance - MoveDistance);
                DirectionSign = -1.0f;
            }
            else
            {
                TravelledDistance = -TravelledDistance;
                DirectionSign = 1.0f;
            }
        }
    }
    else if (TravelledDistance >= MoveDistance)
    {
        TravelledDistance = MoveDistance;
    }
    else if (TravelledDistance <= 0.0f)
    {
        TravelledDistance = 0.0f;
    }

    Owner->SetActorLocation(InitialTransform.GetLocation() + WorldAxis * TravelledDistance);

    if (!bShouldPingPong && (TravelledDistance <= 0.0f || TravelledDistance >= MoveDistance))
    {
        StopMotion();
    }
}
