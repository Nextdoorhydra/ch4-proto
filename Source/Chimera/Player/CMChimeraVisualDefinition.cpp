#include "Player/CMChimeraVisualDefinition.h"

#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSampling.h"
#include "Misc/DataValidation.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"

namespace
{
const FName RootBoneName(TEXT("root"));
const FName CoreRootBoneName(TEXT("core_root"));
const FName ShellARootBoneName(TEXT("shell_a_root"));
const FName ShellBRootBoneName(TEXT("shell_b_root"));
const FName ShellCRootBoneName(TEXT("shell_c_root"));

FText ValidationMessage(
    const FString& Role,
    const FString& Message)
{
    return FText::FromString(FString::Printf(
        TEXT("%s: %s"),
        *Role,
        *Message));
}

bool HasBreakableConstraintForBone(
    const UPhysicsAsset& PhysicsAsset,
    const FName BoneName)
{
    for (const UPhysicsConstraintTemplate* Constraint
        : PhysicsAsset.ConstraintSetup)
    {
        if (!Constraint)
        {
            continue;
        }

        const FConstraintInstance& Instance = Constraint->DefaultInstance;
        const bool bTouchesBone = Instance.ConstraintBone1 == BoneName
            || Instance.ConstraintBone2 == BoneName;
        if (bTouchesBone
            && (Instance.IsLinearBreakable()
                || Instance.IsAngularBreakable()))
        {
            return true;
        }
    }
    return false;
}
}

UCMChimeraVisualDefinition::UCMChimeraVisualDefinition()
{
    RequiredBones = {
        RootBoneName,
        CoreRootBoneName,
        ShellARootBoneName,
        ShellBRootBoneName,
        ShellCRootBoneName
    };
    RequiredSamplingRegions = {
        FName(TEXT("IdleTentacleTop")),
        FName(TEXT("UnderbodyArms"))
    };
    RequiredPhysicsBodies = {
        CoreRootBoneName,
        ShellARootBoneName,
        ShellBRootBoneName,
        ShellCRootBoneName
    };
}

const FCMChimeraSegmentVisualPreset&
UCMChimeraVisualDefinition::GetPreset(
    const ECMChimeraSegmentVisualRole Role) const
{
    switch (Role)
    {
    case ECMChimeraSegmentVisualRole::Head:
        return Head;
    case ECMChimeraSegmentVisualRole::Tail:
        return Tail;
    default:
        return Body;
    }
}

#if WITH_EDITOR
EDataValidationResult UCMChimeraVisualDefinition::IsDataValid(
    FDataValidationContext& Context) const
{
    bool bHasErrors = false;
    const USkeletalMesh* FirstMesh = nullptr;
    const USkeleton* SharedSkeleton = nullptr;
    FString FirstRole;

    const auto ValidatePreset = [this,
                                 &Context,
                                 &bHasErrors,
                                 &FirstMesh,
                                 &SharedSkeleton,
                                 &FirstRole](
        const TCHAR* RoleName,
        const FCMChimeraSegmentVisualPreset& Preset)
    {
        const FString Role(RoleName);
        const USkeletalMesh* Mesh = Preset.Mesh;
        if (!Mesh)
        {
            Context.AddError(ValidationMessage(
                Role,
                TEXT("Skeletal Mesh is not assigned.")));
            bHasErrors = true;
            return;
        }

        if (!Preset.RelativeTransform.GetScale3D().Equals(
            FVector::OneVector,
            KINDA_SMALL_NUMBER))
        {
            Context.AddError(ValidationMessage(
                Role,
                TEXT("preset scale must remain (1, 1, 1).")));
            bHasErrors = true;
        }

        const USkeleton* Skeleton = Mesh->GetSkeleton();
        if (!Skeleton)
        {
            Context.AddError(ValidationMessage(
                Role,
                TEXT("mesh has no Skeleton.")));
            bHasErrors = true;
        }
        else if (!SharedSkeleton)
        {
            SharedSkeleton = Skeleton;
        }
        else if (SharedSkeleton != Skeleton)
        {
            Context.AddError(ValidationMessage(
                Role,
                TEXT("mesh does not use the shared Chimera Skeleton.")));
            bHasErrors = true;
        }

        const FReferenceSkeleton& RefSkeleton = Mesh->GetRefSkeleton();
        for (const FName RequiredBone : RequiredBones)
        {
            if (RefSkeleton.FindBoneIndex(RequiredBone) == INDEX_NONE)
            {
                Context.AddError(ValidationMessage(
                    Role,
                    FString::Printf(
                        TEXT("missing required bone '%s'."),
                        *RequiredBone.ToString())));
                bHasErrors = true;
            }
        }

        const FSkeletalMeshSamplingInfo& SamplingInfo =
            Mesh->GetSamplingInfo();
        for (const FName RequiredRegion : RequiredSamplingRegions)
        {
            const int32 RegionIndex = SamplingInfo.IndexOfRegion(
                RequiredRegion);
            const FSkeletalMeshSamplingRegion* Region =
                SamplingInfo.GetRegion(RequiredRegion);
            if (!Region)
            {
                Context.AddError(ValidationMessage(
                    Role,
                    FString::Printf(
                        TEXT("missing Sampling Region '%s'."),
                        *RequiredRegion.ToString())));
                bHasErrors = true;
            }
            else
            {
                const int32 ResolvedLOD = Region->LODIndex == INDEX_NONE
                    ? Mesh->GetLODNum() - 1
                    : Region->LODIndex;
                if (ResolvedLOD < 0
                    || ResolvedLOD >= Mesh->GetLODNum())
                {
                    Context.AddError(ValidationMessage(
                        Role,
                        FString::Printf(
                            TEXT("Sampling Region '%s' has invalid LOD %d."),
                            *RequiredRegion.ToString(),
                            Region->LODIndex)));
                    bHasErrors = true;
                }

                const FSkeletalMeshSamplingBuiltData& SamplingBuiltData =
                    SamplingInfo.GetBuiltData();
                if (!SamplingBuiltData.RegionBuiltData.IsValidIndex(
                        RegionIndex)
                    || SamplingBuiltData.RegionBuiltData[RegionIndex]
                        .TriangleIndices.IsEmpty())
                {
                    Context.AddError(ValidationMessage(
                        Role,
                        FString::Printf(
                            TEXT("Sampling Region '%s' has no built triangles."),
                            *RequiredRegion.ToString())));
                    bHasErrors = true;
                }
            }
        }

        const UPhysicsAsset* PhysicsAsset = Mesh->GetPhysicsAsset();
        if (!PhysicsAsset)
        {
            Context.AddError(ValidationMessage(
                Role,
                TEXT("mesh has no Physics Asset.")));
            bHasErrors = true;
        }
        else
        {
            for (const FName RequiredBody : RequiredPhysicsBodies)
            {
                if (PhysicsAsset->FindBodyIndex(RequiredBody) == INDEX_NONE)
                {
                    Context.AddError(ValidationMessage(
                        Role,
                        FString::Printf(
                            TEXT("Physics Asset has no body for '%s'."),
                            *RequiredBody.ToString())));
                    bHasErrors = true;
                }
            }

            const FName ShellBones[] = {
                ShellARootBoneName,
                ShellBRootBoneName,
                ShellCRootBoneName
            };
            for (const FName ShellBone : ShellBones)
            {
                if (!HasBreakableConstraintForBone(
                    *PhysicsAsset,
                    ShellBone))
                {
                    Context.AddError(ValidationMessage(
                        Role,
                        FString::Printf(
                            TEXT("'%s' has no breakable Physics Asset constraint."),
                            *ShellBone.ToString())));
                    bHasErrors = true;
                }
            }
        }

        if (!FirstMesh)
        {
            FirstMesh = Mesh;
            FirstRole = Role;
            return;
        }

        const FReferenceSkeleton& FirstRefSkeleton =
            FirstMesh->GetRefSkeleton();
        if (FirstRefSkeleton.GetNum() != RefSkeleton.GetNum())
        {
            Context.AddError(ValidationMessage(
                Role,
                FString::Printf(
                    TEXT("reference skeleton bone count differs from %s."),
                    *FirstRole)));
            bHasErrors = true;
            return;
        }

        bool bHierarchyMatches = true;
        bool bBindPoseMatches = true;
        const TArray<FTransform>& FirstPose =
            FirstRefSkeleton.GetRefBonePose();
        const TArray<FTransform>& Pose = RefSkeleton.GetRefBonePose();
        for (int32 BoneIndex = 0;
            BoneIndex < RefSkeleton.GetNum();
            ++BoneIndex)
        {
            if (FirstRefSkeleton.GetBoneName(BoneIndex)
                    != RefSkeleton.GetBoneName(BoneIndex)
                || FirstRefSkeleton.GetParentIndex(BoneIndex)
                    != RefSkeleton.GetParentIndex(BoneIndex))
            {
                bHierarchyMatches = false;
                break;
            }
            if (!FirstPose[BoneIndex].Equals(
                Pose[BoneIndex],
                0.01f))
            {
                bBindPoseMatches = false;
            }
        }

        if (!bHierarchyMatches)
        {
            Context.AddError(ValidationMessage(
                Role,
                FString::Printf(
                    TEXT("reference skeleton hierarchy differs from %s."),
                    *FirstRole)));
            bHasErrors = true;
        }
        if (!bBindPoseMatches)
        {
            Context.AddError(ValidationMessage(
                Role,
                FString::Printf(
                    TEXT("bind pose differs from %s."),
                    *FirstRole)));
            bHasErrors = true;
        }
    };

    ValidatePreset(TEXT("Head"), Head);
    ValidatePreset(TEXT("Body"), Body);
    ValidatePreset(TEXT("Tail"), Tail);

    return bHasErrors
        ? EDataValidationResult::Invalid
        : EDataValidationResult::Valid;
}
#endif
