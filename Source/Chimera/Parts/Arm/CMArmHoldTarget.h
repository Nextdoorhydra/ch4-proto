#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "CMArmHoldTarget.generated.h"

class ACMArmPart;
class UPrimitiveComponent;

USTRUCT(BlueprintType)
struct CHIMERA_API FCMArmHoldSpec
{
    GENERATED_BODY()

    // 값이 클수록 다른 홀드 후보보다 먼저 선택된다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chimera|Arm|Hold")
    int32 Priority = 0;

    // 손 IK와 실제 잡기에 사용할 월드 좌표다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chimera|Arm|Hold")
    FVector HoldLocation = FVector::ZeroVector;

    // 손바닥이 바라볼 월드 방향이다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chimera|Arm|Hold")
    FVector HoldNormal = FVector::UpVector;

    // 움직이는 대상의 손 IK 기준이다.
    // Physics Handle 사용 시 실제 물리 바디여야 한다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chimera|Arm|Hold")
    TObjectPtr<UPrimitiveComponent> TargetComponent = nullptr;

    // true면 키메라를 묶지 않고 Physics Handle로
    // 대상만 손 목표점을 따라오게 한다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chimera|Arm|Hold")
    bool bUsePhysicsHandle = false;
};

UINTERFACE(BlueprintType)
class CHIMERA_API UCMArmHoldTarget : public UInterface
{
    GENERATED_BODY()
};

// 버튼, 레버, 이동 물체, 전선 끝이 공통으로 구현할 팔 홀드 계약이다.
class CHIMERA_API ICMArmHoldTarget
{
    GENERATED_BODY()

public:
    // 현재 팔이 잡을 수 있는지 판단하고 우선순위와 잡기 정보를 반환한다.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable,
        Category = "Chimera|Arm|Hold")
    bool QueryArmHold(ACMArmPart* ArmPart, FCMArmHoldSpec& OutSpec) const;

    // 최종 후보로 선택된 뒤 대상의 홀드 상태를 시작한다.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable,
        Category = "Chimera|Arm|Hold")
    bool BeginArmHold(ACMArmPart* ArmPart);

    // 입력 해제나 팔 무효화 시 대상이 소유한 임시 홀드 상태를 정리한다.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable,
        Category = "Chimera|Arm|Hold")
    void EndArmHold(ACMArmPart* ArmPart);
};
