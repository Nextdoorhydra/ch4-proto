#include "Player/CMChimeraWrapTentacleComponent.h"

#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "SkeletalRenderPublic.h"
#include "StaticMeshResources.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr int32 WrapSurfaceCandidateMultiplier = 12;
constexpr int32 WrapTubeSideCount = 8;
constexpr float WrapTubeBaseRadius = 4.0f;
constexpr float WrapTubeUVTileLength = 20.0f;
DEFINE_LOG_CATEGORY_STATIC(
    LogCMChimeraWrapTentacle,
    Log,
    All);

struct FWeightedTriangle
{
    int32 TriangleBase = 0;
    float CumulativeArea = 0.0f;
};

void ResolveBarycentricWeights(
    FRandomStream& RandomStream,
    float& OutA,
    float& OutB,
    float& OutC)
{
    const float Root = FMath::Sqrt(RandomStream.GetFraction());
    const float Split = RandomStream.GetFraction();
    OutA = 1.0f - Root;
    OutB = Root * (1.0f - Split);
    OutC = Root * Split;
}

int32 PickWeightedTriangle(
    const TConstArrayView<FWeightedTriangle> Triangles,
    const float TotalArea,
    FRandomStream& RandomStream)
{
    const float Pick = RandomStream.FRandRange(0.0f, TotalArea);
    int32 Index = 0;
    while (Index + 1 < Triangles.Num()
        && Pick > Triangles[Index].CumulativeArea)
    {
        ++Index;
    }
    return Index;
}
}

float CMChimeraWrapTentacle::AdvanceAlpha(
    const float CurrentAlpha,
    const bool bExtending,
    const float DeltaTime,
    const float ExtensionDuration,
    const float RetractionDuration)
{
    const float Target = bExtending ? 1.0f : 0.0f;
    const float Duration = bExtending
        ? ExtensionDuration
        : RetractionDuration;
    return FMath::FInterpConstantTo(
        FMath::Clamp(CurrentAlpha, 0.0f, 1.0f),
        Target,
        FMath::Max(DeltaTime, 0.0f),
        1.0f / FMath::Max(Duration, UE_SMALL_NUMBER));
}

float CMChimeraWrapTentacle::ResolvePointRevealAlpha(
    const float ExtensionAlpha,
    const int32 PointIndex,
    const int32 PointCount)
{
    if (PointIndex <= 0)
    {
        return 1.0f;
    }
    const int32 SpanCount = FMath::Max(PointCount - 1, 1);
    const float SpanProgress = FMath::Clamp(ExtensionAlpha, 0.0f, 1.0f)
        * static_cast<float>(SpanCount);
    return FMath::SmoothStep(
        0.0f,
        1.0f,
        FMath::Clamp(
            SpanProgress - static_cast<float>(PointIndex - 1),
            0.0f,
            1.0f));
}

bool CMChimeraWrapTentacle::IsWithinActivationDistance(
    const FVector& SourcePosition,
    const FBoxSphereBounds& TargetBounds,
    const float ActivationDistance)
{
    return TargetBounds.GetBox().ComputeSquaredDistanceToPoint(SourcePosition)
        <= FMath::Square(FMath::Max(ActivationDistance, 0.0f));
}

UCMChimeraWrapTentacleComponent::UCMChimeraWrapTentacleComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    SetCanEverAffectNavigation(false);

    static ConstructorHelpers::FObjectFinder<UMaterialInterface>
        DefaultTentacleMaterial(
            TEXT("/Game/Vefects/Tentacles_VFX/VFX/Goo/Materials/"
                "MI_VFX_Goo_Arm_01.MI_VFX_Goo_Arm_01"));
    if (DefaultTentacleMaterial.Succeeded())
    {
        TentacleMaterial = DefaultTentacleMaterial.Object;
    }
}

void UCMChimeraWrapTentacleComponent::BeginPlay()
{
    Super::BeginPlay();

    const AActor* Owner = GetOwner();
    if (!Owner || Owner->GetNetMode() == NM_DedicatedServer)
    {
        bEffectActive = false;
        SetComponentTickEnabled(false);
        return;
    }
    RefreshTickEnabled();
}

void UCMChimeraWrapTentacleComponent::TickComponent(
    const float DeltaTime,
    const ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    ElapsedTime += FMath::Max(DeltaTime, 0.0f);
    if (!IsValid(TargetMesh))
    {
        TargetMesh = nullptr;
        ResetTargetState();
        RefreshTickEnabled();
        return;
    }
    const bool bShouldExtend = bEffectActive
        && TargetMesh
        && IsTargetInRange();
    if (bShouldExtend
        && !bPathsReady
        && ElapsedTime >= NextSamplingTime)
    {
        const bool bBuiltCandidates = BuildSurfaceCandidates();
        const bool bAssignedPaths = bBuiltCandidates
            && AssignDistanceOrderedPaths();
        bPathsReady = bAssignedPaths;
        if (!bPathsReady)
        {
            NextSamplingTime = ElapsedTime
                + FMath::Max(SamplingRetryInterval, 0.1f);
            if (!bLoggedSamplingFailure)
            {
                UE_LOG(LogCMChimeraWrapTentacle, Warning,
                    TEXT("Could not sample wrap-tentacle paths from '%s'. "
                        "CandidatesBuilt=%s, CandidateCount=%d, "
                        "PathsAssigned=%s. Verify the target type, LOD, "
                        "CPU access, and minimum anchor spacing."),
                    *GetPathNameSafe(TargetMesh),
                    bBuiltCandidates ? TEXT("true") : TEXT("false"),
                    SurfaceCandidates.Num(),
                    bAssignedPaths ? TEXT("true") : TEXT("false"));
                bLoggedSamplingFailure = true;
            }
        }
    }

    ExtensionAlpha = CMChimeraWrapTentacle::AdvanceAlpha(
        ExtensionAlpha,
        bShouldExtend && bPathsReady,
        DeltaTime,
        ExtensionDuration,
        RetractionDuration);
    if (ExtensionAlpha <= UE_KINDA_SMALL_NUMBER)
    {
        HidePool();
        return;
    }
    UpdateTentacleMeshes();
}

void UCMChimeraWrapTentacleComponent::ConfigureSource(
    UMeshComponent* InSourceMesh,
    const int32 SegmentIndex)
{
    SourceMesh = InSourceMesh;
    RandomStream.Initialize(3571 + SegmentIndex * 65537);
    ResetTargetState();
    RefreshTickEnabled();
}

void UCMChimeraWrapTentacleComponent::SetEffectActive(
    const bool bInActive)
{
    const AActor* Owner = GetOwner();
    bEffectActive = bInActive
        && Owner
        && Owner->GetNetMode() != NM_DedicatedServer;
    if (!bEffectActive)
    {
        ExtensionAlpha = 0.0f;
        HidePool();
    }
    RefreshTickEnabled();
}

void UCMChimeraWrapTentacleComponent::SetTargetMesh(
    UMeshComponent* InTargetMesh)
{
    if (TargetMesh == InTargetMesh)
    {
        return;
    }
    TargetMesh = InTargetMesh;
    ResetTargetState();
    RefreshTickEnabled();
}

int32 UCMChimeraWrapTentacleComponent::GetActiveTentacleCount() const
{
    return ExtensionAlpha > UE_KINDA_SMALL_NUMBER && bPathsReady
        ? RuntimeTentacles.Num()
        : 0;
}

void UCMChimeraWrapTentacleComponent::OnComponentDestroyed(
    const bool bDestroyingHierarchy)
{
    DestroyPool();
    Super::OnComponentDestroyed(bDestroyingHierarchy);
}

bool UCMChimeraWrapTentacleComponent::BuildSurfaceCandidates()
{
    SurfaceCandidates.Reset();
    if (UStaticMeshComponent* StaticTarget =
        Cast<UStaticMeshComponent>(TargetMesh))
    {
        return BuildStaticSurfaceCandidates(*StaticTarget);
    }
    if (USkeletalMeshComponent* SkeletalTarget =
        Cast<USkeletalMeshComponent>(TargetMesh))
    {
        return BuildSkeletalSurfaceCandidates(*SkeletalTarget);
    }
    return false;
}

bool UCMChimeraWrapTentacleComponent::BuildSkeletalSurfaceCandidates(
    USkeletalMeshComponent& SkeletalTarget)
{
    if (!SkeletalTarget.GetMeshObject())
    {
        return false;
    }
    USkeletalMesh* Mesh = SkeletalTarget.GetSkeletalMeshAsset();
    FSkeletalMeshRenderData* RenderData = Mesh
        ? Mesh->GetResourceForRendering()
        : nullptr;
    if (!RenderData
        || !RenderData->LODRenderData.IsValidIndex(TargetLODIndex))
    {
        return false;
    }

    TArray<FFinalSkinVertex> Vertices;
    SkeletalTarget.GetCPUSkinnedVertices(Vertices, TargetLODIndex);
    TArray<uint32> Indices;
    RenderData->LODRenderData[TargetLODIndex]
        .MultiSizeIndexContainer.GetIndexBuffer(Indices);
    if (Vertices.IsEmpty() || Indices.Num() < 3)
    {
        return false;
    }

    TArray<FWeightedTriangle> Triangles;
    float TotalArea = 0.0f;
    for (int32 TriangleBase = 0;
        TriangleBase + 2 < Indices.Num();
        TriangleBase += 3)
    {
        const uint32 IA = Indices[TriangleBase];
        const uint32 IB = Indices[TriangleBase + 1];
        const uint32 IC = Indices[TriangleBase + 2];
        if (!Vertices.IsValidIndex(IA)
            || !Vertices.IsValidIndex(IB)
            || !Vertices.IsValidIndex(IC))
        {
            continue;
        }
        const FVector A(Vertices[IA].Position);
        const FVector B(Vertices[IB].Position);
        const FVector C(Vertices[IC].Position);
        const float Area = FVector::CrossProduct(B - A, C - A).Length()
            * 0.5f;
        if (Area <= UE_SMALL_NUMBER)
        {
            continue;
        }
        TotalArea += Area;
        FWeightedTriangle& Triangle = Triangles.AddDefaulted_GetRef();
        Triangle.TriangleBase = TriangleBase;
        Triangle.CumulativeArea = TotalArea;
    }
    if (Triangles.IsEmpty())
    {
        return false;
    }

    const int32 CandidateCount = FMath::Max(
        TentacleCount * FMath::Max(SplinePointCount - 1, 1)
            * WrapSurfaceCandidateMultiplier,
        TentacleCount);
    SurfaceCandidates.Reserve(CandidateCount);
    for (int32 CandidateIndex = 0;
        CandidateIndex < CandidateCount;
        ++CandidateIndex)
    {
        const FWeightedTriangle& Triangle = Triangles[
            PickWeightedTriangle(Triangles, TotalArea, RandomStream)];
        const uint32 VertexIndices[] = {
            Indices[Triangle.TriangleBase],
            Indices[Triangle.TriangleBase + 1],
            Indices[Triangle.TriangleBase + 2]
        };
        float WA;
        float WB;
        float WC;
        ResolveBarycentricWeights(RandomStream, WA, WB, WC);
        const float Weights[] = {WA, WB, WC};
        FVector Position = FVector::ZeroVector;
        FVector Normal = FVector::ZeroVector;
        for (int32 Corner = 0; Corner < 3; ++Corner)
        {
            Position += FVector(Vertices[VertexIndices[Corner]].Position)
                * Weights[Corner];
            Normal += Vertices[VertexIndices[Corner]].TangentZ.ToFVector()
                * Weights[Corner];
        }
        AddSurfaceCandidate(
            Position,
            Normal.GetSafeNormal(),
            &SkeletalTarget);
    }
    return !SurfaceCandidates.IsEmpty();
}

bool UCMChimeraWrapTentacleComponent::BuildStaticSurfaceCandidates(
    UStaticMeshComponent& StaticTarget)
{
    UStaticMesh* Mesh = StaticTarget.GetStaticMesh();
    const FStaticMeshRenderData* RenderData = Mesh
        ? Mesh->GetRenderData()
        : nullptr;
    if (!RenderData
        || !RenderData->LODResources.IsValidIndex(TargetLODIndex))
    {
        return false;
    }

    const FStaticMeshLODResources& LOD =
        RenderData->LODResources[TargetLODIndex];
    const FPositionVertexBuffer& Positions =
        LOD.VertexBuffers.PositionVertexBuffer;
    const FStaticMeshVertexBuffer& Vertices =
        LOD.VertexBuffers.StaticMeshVertexBuffer;
    const FIndexArrayView Indices = LOD.IndexBuffer.GetArrayView();
    if (Positions.GetNumVertices() == 0 || Indices.Num() < 3)
    {
        return false;
    }

    TArray<FWeightedTriangle> Triangles;
    float TotalArea = 0.0f;
    for (int32 TriangleBase = 0;
        TriangleBase + 2 < Indices.Num();
        TriangleBase += 3)
    {
        const uint32 IA = Indices[TriangleBase];
        const uint32 IB = Indices[TriangleBase + 1];
        const uint32 IC = Indices[TriangleBase + 2];
        if (IA >= Positions.GetNumVertices()
            || IB >= Positions.GetNumVertices()
            || IC >= Positions.GetNumVertices())
        {
            continue;
        }
        const FVector A(Positions.VertexPosition(IA));
        const FVector B(Positions.VertexPosition(IB));
        const FVector C(Positions.VertexPosition(IC));
        const float Area = FVector::CrossProduct(B - A, C - A).Length()
            * 0.5f;
        if (Area <= UE_SMALL_NUMBER)
        {
            continue;
        }
        TotalArea += Area;
        FWeightedTriangle& Triangle = Triangles.AddDefaulted_GetRef();
        Triangle.TriangleBase = TriangleBase;
        Triangle.CumulativeArea = TotalArea;
    }
    if (Triangles.IsEmpty())
    {
        return false;
    }

    const int32 CandidateCount = FMath::Max(
        TentacleCount * FMath::Max(SplinePointCount - 1, 1)
            * WrapSurfaceCandidateMultiplier,
        TentacleCount);
    SurfaceCandidates.Reserve(CandidateCount);
    for (int32 CandidateIndex = 0;
        CandidateIndex < CandidateCount;
        ++CandidateIndex)
    {
        const FWeightedTriangle& Triangle = Triangles[
            PickWeightedTriangle(Triangles, TotalArea, RandomStream)];
        const uint32 VertexIndices[] = {
            Indices[Triangle.TriangleBase],
            Indices[Triangle.TriangleBase + 1],
            Indices[Triangle.TriangleBase + 2]
        };
        float WA;
        float WB;
        float WC;
        ResolveBarycentricWeights(RandomStream, WA, WB, WC);
        const float Weights[] = {WA, WB, WC};
        FVector Position = FVector::ZeroVector;
        FVector Normal = FVector::ZeroVector;
        for (int32 Corner = 0; Corner < 3; ++Corner)
        {
            Position += FVector(
                Positions.VertexPosition(VertexIndices[Corner]))
                * Weights[Corner];
            const FVector4f TangentZ =
                Vertices.VertexTangentZ(VertexIndices[Corner]);
            Normal += FVector(TangentZ.X, TangentZ.Y, TangentZ.Z)
                * Weights[Corner];
        }
        AddSurfaceCandidate(
            Position,
            Normal.GetSafeNormal(),
            nullptr);
    }
    return !SurfaceCandidates.IsEmpty();
}

void UCMChimeraWrapTentacleComponent::AddSurfaceCandidate(
    const FVector& TargetLocalPosition,
    const FVector& TargetLocalNormal,
    USkeletalMeshComponent* SkeletalTarget)
{
    if (!TargetMesh)
    {
        return;
    }

    FCMChimeraWrapSurfaceAnchor& Anchor =
        SurfaceCandidates.AddDefaulted_GetRef();
    Anchor.TargetLocalPosition = TargetLocalPosition;
    Anchor.TargetLocalNormal = TargetLocalNormal.GetSafeNormal(
        UE_SMALL_NUMBER,
        FVector::UpVector);
    if (!SkeletalTarget)
    {
        return;
    }

    const FVector WorldPosition = SkeletalTarget->GetComponentTransform()
        .TransformPosition(TargetLocalPosition);
    FVector BoneWorldLocation;
    Anchor.BoneName = SkeletalTarget->FindClosestBone(
        WorldPosition,
        &BoneWorldLocation);
    const int32 BoneIndex = SkeletalTarget->GetBoneIndex(Anchor.BoneName);
    if (BoneIndex != INDEX_NONE)
    {
        const FTransform BoneTransform =
            SkeletalTarget->GetBoneTransform(BoneIndex);
        Anchor.BoneLocalPosition = BoneTransform.InverseTransformPosition(
            WorldPosition);
        const FVector WorldNormal = SkeletalTarget->GetComponentTransform()
            .TransformVectorNoScale(Anchor.TargetLocalNormal);
        Anchor.BoneLocalNormal = BoneTransform.InverseTransformVectorNoScale(
            WorldNormal).GetSafeNormal();
    }
}

bool UCMChimeraWrapTentacleComponent::AssignDistanceOrderedPaths()
{
    if (SurfaceCandidates.IsEmpty())
    {
        return false;
    }

    const FVector SourceWorld = GetSourceWorldPosition();
    SurfaceCandidates.Sort([this, &SourceWorld](
        const FCMChimeraWrapSurfaceAnchor& A,
        const FCMChimeraWrapSurfaceAnchor& B)
    {
        return FVector::DistSquared(
                ResolveAnchorWorldPosition(A), SourceWorld)
            < FVector::DistSquared(
                ResolveAnchorWorldPosition(B), SourceWorld);
    });

    EnsurePool();
    if (RuntimeTentacles.IsEmpty())
    {
        return false;
    }

    const int32 AnchorCountPerTentacle =
        FMath::Max(SplinePointCount, 3) - 1;
    TArray<TArray<int32>> PathIndices;
    PathIndices.SetNum(RuntimeTentacles.Num());
    TArray<int32> SelectedIndices;

    const auto IsAvailable = [this, &SelectedIndices](
        const int32 CandidateIndex)
    {
        if (SelectedIndices.Contains(CandidateIndex))
        {
            return false;
        }
        const FVector CandidateWorld = ResolveAnchorWorldPosition(
            SurfaceCandidates[CandidateIndex]);
        for (const int32 SelectedIndex : SelectedIndices)
        {
            if (FVector::DistSquared(
                    CandidateWorld,
                    ResolveAnchorWorldPosition(
                        SurfaceCandidates[SelectedIndex]))
                < FMath::Square(MinimumTargetAnchorDistance))
            {
                return false;
            }
        }
        return true;
    };

    for (int32 TentacleIndex = 0;
        TentacleIndex < RuntimeTentacles.Num();
        ++TentacleIndex)
    {
        int32 StartIndex = INDEX_NONE;
        for (int32 CandidateIndex = 0;
            CandidateIndex < SurfaceCandidates.Num();
            ++CandidateIndex)
        {
            if (IsAvailable(CandidateIndex))
            {
                StartIndex = CandidateIndex;
                break;
            }
        }
        if (StartIndex == INDEX_NONE)
        {
            return false;
        }
        PathIndices[TentacleIndex].Add(StartIndex);
        SelectedIndices.Add(StartIndex);
    }

    for (int32 AnchorIndex = 1;
        AnchorIndex < AnchorCountPerTentacle;
        ++AnchorIndex)
    {
        for (int32 TentacleIndex = 0;
            TentacleIndex < RuntimeTentacles.Num();
            ++TentacleIndex)
        {
            const int32 PreviousIndex = PathIndices[TentacleIndex].Last();
            const FVector PreviousWorld = ResolveAnchorWorldPosition(
                SurfaceCandidates[PreviousIndex]);
            const FVector PreviousNormal = ResolveAnchorWorldNormal(
                SurfaceCandidates[PreviousIndex]);
            int32 BestIndex = INDEX_NONE;
            float BestDistanceSquared = TNumericLimits<float>::Max();
            for (int32 CandidateIndex = 0;
                CandidateIndex < SurfaceCandidates.Num();
                ++CandidateIndex)
            {
                if (!IsAvailable(CandidateIndex)
                    || FVector::DotProduct(
                        PreviousNormal,
                        ResolveAnchorWorldNormal(
                            SurfaceCandidates[CandidateIndex])) < 0.0f)
                {
                    continue;
                }
                const float DistanceSquared = FVector::DistSquared(
                    PreviousWorld,
                    ResolveAnchorWorldPosition(
                        SurfaceCandidates[CandidateIndex]));
                if (DistanceSquared < BestDistanceSquared)
                {
                    BestDistanceSquared = DistanceSquared;
                    BestIndex = CandidateIndex;
                }
            }
            if (BestIndex == INDEX_NONE)
            {
                return false;
            }
            PathIndices[TentacleIndex].Add(BestIndex);
            SelectedIndices.Add(BestIndex);
        }
    }

    for (int32 TentacleIndex = 0;
        TentacleIndex < RuntimeTentacles.Num();
        ++TentacleIndex)
    {
        FCMChimeraWrapTentacleRuntime& Runtime =
            RuntimeTentacles[TentacleIndex];
        Runtime.TargetAnchors.Reset(AnchorCountPerTentacle);
        for (const int32 CandidateIndex : PathIndices[TentacleIndex])
        {
            Runtime.TargetAnchors.Add(SurfaceCandidates[CandidateIndex]);
        }
        Runtime.NoisePhase = RandomStream.FRandRange(0.0f, 2.0f * PI);
    }
    return !RuntimeTentacles.IsEmpty();
}

void UCMChimeraWrapTentacleComponent::EnsurePool()
{
    if (!GetOwner())
    {
        return;
    }

    const int32 PointCount = FMath::Max(SplinePointCount, 3);
    const int32 DesiredTentacleCount = FMath::Max(TentacleCount, 1);
    while (RuntimeTentacles.Num() > DesiredTentacleCount)
    {
        FCMChimeraWrapTentacleRuntime& Runtime = RuntimeTentacles.Last();
        if (Runtime.TubeMesh)
        {
            Runtime.TubeMesh->DestroyComponent();
        }
        if (Runtime.Spline)
        {
            Runtime.Spline->DestroyComponent();
        }
        RuntimeTentacles.Pop();
    }
    while (RuntimeTentacles.Num() < DesiredTentacleCount)
    {
        FCMChimeraWrapTentacleRuntime& Runtime =
            RuntimeTentacles.AddDefaulted_GetRef();
        Runtime.Spline = NewObject<USplineComponent>(
            GetOwner(),
            MakeUniqueObjectName(
                GetOwner(),
                USplineComponent::StaticClass(),
                TEXT("WrapTentacleSpline")));
        Runtime.Spline->SetMobility(EComponentMobility::Movable);
        Runtime.Spline->SetupAttachment(this);
        GetOwner()->AddInstanceComponent(Runtime.Spline);
        Runtime.Spline->RegisterComponent();
        Runtime.Spline->ClearSplinePoints(false);
        for (int32 PointIndex = 0;
            PointIndex < PointCount;
            ++PointIndex)
        {
            Runtime.Spline->AddSplinePoint(
                FVector::ZeroVector,
                ESplineCoordinateSpace::Local,
                false);
            Runtime.Spline->SetSplinePointType(
                PointIndex,
                ESplinePointType::Linear,
                false);
        }
        Runtime.Spline->UpdateSpline();

        Runtime.TubeMesh = NewObject<UProceduralMeshComponent>(
            GetOwner(),
            MakeUniqueObjectName(
                GetOwner(),
                UProceduralMeshComponent::StaticClass(),
                TEXT("WrapTentacleTube")));
        Runtime.TubeMesh->SetMobility(EComponentMobility::Movable);
        Runtime.TubeMesh->SetupAttachment(this);
        Runtime.TubeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Runtime.TubeMesh->SetGenerateOverlapEvents(false);
        Runtime.TubeMesh->SetCanEverAffectNavigation(false);
        Runtime.TubeMesh->SetCastShadow(false);
        Runtime.TubeMesh->bUseAttachParentBound = false;
        if (TentacleMaterial)
        {
            Runtime.TubeMesh->SetMaterial(0, TentacleMaterial);
        }
        Runtime.TubeMesh->SetVisibility(false, true);
        Runtime.TubeMesh->SetHiddenInGame(true, true);
        GetOwner()->AddInstanceComponent(Runtime.TubeMesh);
        Runtime.TubeMesh->RegisterComponent();
    }
}

void UCMChimeraWrapTentacleComponent::DestroyPool()
{
    for (FCMChimeraWrapTentacleRuntime& Runtime : RuntimeTentacles)
    {
        if (Runtime.TubeMesh)
        {
            Runtime.TubeMesh->DestroyComponent();
        }
        if (Runtime.Spline)
        {
            Runtime.Spline->DestroyComponent();
        }
    }
    RuntimeTentacles.Reset();
}

void UCMChimeraWrapTentacleComponent::HidePool()
{
    for (FCMChimeraWrapTentacleRuntime& Runtime : RuntimeTentacles)
    {
        if (Runtime.TubeMesh)
        {
            Runtime.TubeMesh->SetVisibility(false, true);
            Runtime.TubeMesh->SetHiddenInGame(true, true);
        }
    }
}

void UCMChimeraWrapTentacleComponent::UpdateTentacleMeshes()
{
    const int32 PointCount = FMath::Max(SplinePointCount, 3);
    const FVector SourceLocal = GetComponentTransform()
        .InverseTransformPosition(GetSourceWorldPosition());
    for (FCMChimeraWrapTentacleRuntime& Runtime : RuntimeTentacles)
    {
        if (!Runtime.Spline
            || Runtime.TargetAnchors.Num() != PointCount - 1)
        {
            continue;
        }

        Runtime.Spline->SetLocationAtSplinePoint(
            0,
            SourceLocal,
            ESplineCoordinateSpace::Local,
            false);
        for (int32 PointIndex = 1;
            PointIndex < PointCount;
            ++PointIndex)
        {
            const FCMChimeraWrapSurfaceAnchor& Anchor =
                Runtime.TargetAnchors[PointIndex - 1];
            const FVector AnchorWorld = ResolveAnchorWorldPosition(Anchor);
            const FVector NormalWorld = ResolveAnchorWorldNormal(Anchor);
            FVector AxisX;
            FVector AxisY;
            NormalWorld.FindBestAxisVectors(AxisX, AxisY);
            const float DistanceAlpha = static_cast<float>(PointIndex)
                / static_cast<float>(PointCount - 1);
            const float Phase = Runtime.NoisePhase
                + DistanceAlpha * EntanglementCycles * 2.0f * PI
                + ElapsedTime;
            const FVector EntanglementOffset =
                (AxisX * FMath::Sin(Phase)
                    + AxisY * FMath::Cos(Phase * 0.73f))
                * EntanglementAmplitude
                * Entanglement
                * FMath::Sin(PI * DistanceAlpha);
            const FVector TargetWorld = AnchorWorld
                + NormalWorld * SurfaceOffset
                + EntanglementOffset;
            const float RevealAlpha =
                CMChimeraWrapTentacle::ResolvePointRevealAlpha(
                    ExtensionAlpha,
                    PointIndex,
                    PointCount);
            Runtime.Spline->SetLocationAtSplinePoint(
                PointIndex,
                FMath::Lerp(
                    SourceLocal,
                    GetComponentTransform().InverseTransformPosition(
                        TargetWorld),
                    RevealAlpha),
                ESplineCoordinateSpace::Local,
                false);
        }
        Runtime.Spline->UpdateSpline();
        UpdateTubeMesh(Runtime);
    }
}

void UCMChimeraWrapTentacleComponent::UpdateTubeMesh(
    FCMChimeraWrapTentacleRuntime& Runtime)
{
    if (!Runtime.Spline || !Runtime.TubeMesh)
    {
        return;
    }

    const int32 PointCount = Runtime.Spline->GetNumberOfSplinePoints();
    if (PointCount < 2)
    {
        return;
    }

    TArray<FVector> Points;
    Points.Reserve(PointCount);
    for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
    {
        Points.Add(Runtime.Spline->GetLocationAtSplinePoint(
            PointIndex,
            ESplineCoordinateSpace::Local));
    }

    const int32 RingVertexCount = WrapTubeSideCount + 1;
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;
    Vertices.Reserve(PointCount * RingVertexCount);
    Normals.Reserve(PointCount * RingVertexCount);
    UVs.Reserve(PointCount * RingVertexCount);
    Colors.Reserve(PointCount * RingVertexCount);
    Tangents.Reserve(PointCount * RingVertexCount);
    Triangles.Reserve((PointCount - 1) * WrapTubeSideCount * 6);

    FVector FrameX = FVector::RightVector;
    FVector FrameY = FVector::UpVector;
    FVector PreviousTangent = FVector::ForwardVector;
    float AccumulatedLength = 0.0f;
    for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
    {
        if (PointIndex > 0)
        {
            AccumulatedLength += FVector::Distance(
                Points[PointIndex - 1],
                Points[PointIndex]);
        }

        FVector PathTangent;
        if (PointIndex == 0)
        {
            PathTangent = Points[1] - Points[0];
        }
        else if (PointIndex == PointCount - 1)
        {
            PathTangent = Points[PointIndex] - Points[PointIndex - 1];
        }
        else
        {
            PathTangent = Points[PointIndex + 1] - Points[PointIndex - 1];
        }
        PathTangent = PathTangent.GetSafeNormal(
            UE_SMALL_NUMBER,
            PreviousTangent);
        PreviousTangent = PathTangent;

        if (PointIndex == 0)
        {
            PathTangent.FindBestAxisVectors(FrameX, FrameY);
        }
        else
        {
            FrameX = (FrameX
                - PathTangent * FVector::DotProduct(FrameX, PathTangent))
                .GetSafeNormal();
            if (FrameX.IsNearlyZero())
            {
                PathTangent.FindBestAxisVectors(FrameX, FrameY);
            }
            else
            {
                FrameY = FVector::CrossProduct(PathTangent, FrameX)
                    .GetSafeNormal();
                FrameX = FVector::CrossProduct(FrameY, PathTangent)
                    .GetSafeNormal();
            }
        }

        const float LengthAlpha = static_cast<float>(PointIndex)
            / static_cast<float>(PointCount - 1);
        const float Radius = WrapTubeBaseRadius
            * TentacleWidth
            * FMath::Lerp(1.0f, 0.7f, LengthAlpha);
        for (int32 SideIndex = 0;
            SideIndex <= WrapTubeSideCount;
            ++SideIndex)
        {
            const float SideAlpha = static_cast<float>(SideIndex)
                / static_cast<float>(WrapTubeSideCount);
            const float Angle = SideAlpha * 2.0f * PI;
            const FVector Radial = FrameX * FMath::Cos(Angle)
                + FrameY * FMath::Sin(Angle);
            Vertices.Add(Points[PointIndex] + Radial * Radius);
            Normals.Add(Radial);
            UVs.Add(FVector2D(
                AccumulatedLength / WrapTubeUVTileLength,
                SideAlpha));
            Colors.Add(FLinearColor::White);
            Tangents.Add(FProcMeshTangent(PathTangent, false));
        }
    }

    for (int32 RingIndex = 1; RingIndex < PointCount; ++RingIndex)
    {
        const int32 PreviousRing = (RingIndex - 1) * RingVertexCount;
        const int32 CurrentRing = RingIndex * RingVertexCount;
        for (int32 SideIndex = 0;
            SideIndex < WrapTubeSideCount;
            ++SideIndex)
        {
            Triangles.Add(PreviousRing + SideIndex);
            Triangles.Add(PreviousRing + SideIndex + 1);
            Triangles.Add(CurrentRing + SideIndex);
            Triangles.Add(PreviousRing + SideIndex + 1);
            Triangles.Add(CurrentRing + SideIndex + 1);
            Triangles.Add(CurrentRing + SideIndex);
        }
    }

    Runtime.TubeMesh->CreateMeshSection_LinearColor(
        0,
        Vertices,
        Triangles,
        Normals,
        UVs,
        Colors,
        Tangents,
        false);
    Runtime.TubeMesh->SetHiddenInGame(false, true);
    Runtime.TubeMesh->SetVisibility(true, true);
}

FVector UCMChimeraWrapTentacleComponent::ResolveAnchorWorldPosition(
    const FCMChimeraWrapSurfaceAnchor& Anchor) const
{
    const USkeletalMeshComponent* SkeletalTarget =
        Cast<USkeletalMeshComponent>(TargetMesh);
    if (SkeletalTarget && !Anchor.BoneName.IsNone())
    {
        const int32 BoneIndex = SkeletalTarget->GetBoneIndex(Anchor.BoneName);
        if (BoneIndex != INDEX_NONE)
        {
            return SkeletalTarget->GetBoneTransform(BoneIndex)
                .TransformPosition(Anchor.BoneLocalPosition);
        }
    }
    return TargetMesh
        ? TargetMesh->GetComponentTransform().TransformPosition(
            Anchor.TargetLocalPosition)
        : FVector::ZeroVector;
}

FVector UCMChimeraWrapTentacleComponent::ResolveAnchorWorldNormal(
    const FCMChimeraWrapSurfaceAnchor& Anchor) const
{
    const USkeletalMeshComponent* SkeletalTarget =
        Cast<USkeletalMeshComponent>(TargetMesh);
    if (SkeletalTarget && !Anchor.BoneName.IsNone())
    {
        const int32 BoneIndex = SkeletalTarget->GetBoneIndex(Anchor.BoneName);
        if (BoneIndex != INDEX_NONE)
        {
            return SkeletalTarget->GetBoneTransform(BoneIndex)
                .TransformVectorNoScale(Anchor.BoneLocalNormal)
                .GetSafeNormal();
        }
    }
    return TargetMesh
        ? TargetMesh->GetComponentTransform().TransformVectorNoScale(
            Anchor.TargetLocalNormal).GetSafeNormal(
                UE_SMALL_NUMBER,
                FVector::UpVector)
        : FVector::UpVector;
}

FVector UCMChimeraWrapTentacleComponent::GetSourceWorldPosition() const
{
    return SourceMesh
        ? SourceMesh->Bounds.Origin
        : GetComponentLocation();
}

bool UCMChimeraWrapTentacleComponent::IsTargetInRange() const
{
    return TargetMesh
        && CMChimeraWrapTentacle::IsWithinActivationDistance(
            GetSourceWorldPosition(),
            TargetMesh->Bounds,
            ActivationDistance);
}

void UCMChimeraWrapTentacleComponent::ResetTargetState()
{
    SurfaceCandidates.Reset();
    bPathsReady = false;
    ExtensionAlpha = 0.0f;
    NextSamplingTime = 0.0f;
    bLoggedSamplingFailure = false;
    for (FCMChimeraWrapTentacleRuntime& Runtime : RuntimeTentacles)
    {
        Runtime.TargetAnchors.Reset();
    }
    HidePool();
}

void UCMChimeraWrapTentacleComponent::RefreshTickEnabled()
{
    SetComponentTickEnabled(
        bEffectActive
        && IsValid(TargetMesh));
}
