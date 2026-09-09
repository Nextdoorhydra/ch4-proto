#include "Sound/CMSoundTags.h"

namespace CMSoundTags
{
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(BGM_Menu, "Chimera.Sound.BGM.Menu", "메인 메뉴 BGM");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(BGM_Stage_Stage01, "Chimera.Sound.BGM.Stage.Stage01", "Stage 1 BGM");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(BGM_Result_Victory, "Chimera.Sound.BGM.Result.Victory", "스테이지 성공 BGM");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(BGM_Result_Defeat, "Chimera.Sound.BGM.Result.Defeat", "스테이지 실패 BGM");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(BGM_Ending, "Chimera.Sound.BGM.Ending", "엔딩 BGM");

    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Body_Footstep, "Chimera.Sound.Body.Footstep", "공용 몸통 발걸음");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Body_BloodDragLoop, "Chimera.Sound.Body.BloodDragLoop", "키메라 혈흔 끌림 루프");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Body_Hit, "Chimera.Sound.Body.Hit", "공용 몸통 피격");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Body_Death, "Chimera.Sound.Body.Death", "공용 몸통 사망");

    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Part_Pickup, "Chimera.Sound.Part.Pickup", "부위 획득");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Part_Attach, "Chimera.Sound.Part.Attach", "부위 장착");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Part_Detach, "Chimera.Sound.Part.Detach", "부위 탈착");

    UE_DEFINE_GAMEPLAY_TAG_COMMENT(AI_Aggressive_TargetSpotted, "Chimera.Sound.AI.Aggressive.TargetSpotted", "공격형 AI 타깃 최초 발견");

    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stage_Button_Press, "Chimera.Sound.Stage.Button.Press", "스테이지 버튼 입력");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stage_PressurePlate_WeightChanged, "Chimera.Sound.Stage.PressurePlate.WeightChanged", "감압판 무게 변화");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stage_Obstacle_RotatingBlade_ActiveLoop, "Chimera.Sound.Stage.Obstacle.RotatingBlade.ActiveLoop", "회전 톱날 활성 루프");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stage_Obstacle_Fan_ActiveLoop, "Chimera.Sound.Stage.Obstacle.Fan.ActiveLoop", "팬 활성 루프");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stage_Obstacle_Laser_On, "Chimera.Sound.Stage.Obstacle.Laser.On", "레이저 켜짐");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stage_Obstacle_Laser_Off, "Chimera.Sound.Stage.Obstacle.Laser.Off", "레이저 꺼짐");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stage_Obstacle_Laser_ActiveLoop, "Chimera.Sound.Stage.Obstacle.Laser.ActiveLoop", "레이저 활성 험 루프");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stage_Obstacle_PushBox_MoveLoop, "Chimera.Sound.Stage.Obstacle.PushBox.MoveLoop", "밀기 상자 이동 루프");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stage_Lever_MoveLoop, "Chimera.Sound.Stage.Lever.MoveLoop", "레버 이동 루프");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stage_Lever_Settle, "Chimera.Sound.Stage.Lever.Settle", "레버 끝 위치 정착");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stage_VisionStone_OnLoop, "Chimera.Sound.Stage.VisionStone.OnLoop", "시야석 조건 충족 루프");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stage_VisionStone_OffLoop, "Chimera.Sound.Stage.VisionStone.OffLoop", "시야석 조건 미충족 루프");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stage_Door_Open, "Chimera.Sound.Stage.Door.Open", "스테이지 문 열림");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stage_Door_Close, "Chimera.Sound.Stage.Door.Close", "스테이지 문 닫힘");

    UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Click, "Chimera.Sound.UI.Click", "UI 기본 클릭");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Confirm, "Chimera.Sound.UI.Confirm", "UI 확인");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Cancel, "Chimera.Sound.UI.Cancel", "UI 취소");
}
