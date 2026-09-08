#include "Player/CMChimeraIdleTentacleComponent.h"

#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSampling.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "SkeletalRenderPublic.h"
#include "StaticMeshResources.h"

namespace
{
constexpr int32 SurfaceCandidateMultiplier = 8;
constexpr int32 MaximumSurfaceBuildAttempts = 8;
constexpr float SurfaceBuildRetryInterval = 0.25f;
constexpr float MinimumRenderableLengthAlpha = 0.01f;
}

float CMChimeraIdleTentacle::ResolveLifecycleLengthAlpha(
    const float Age,
    const float GrowthDuration,
    const float IdleDuration,
    const float RetractionDuration)
{
    const float SafeGrowthDuration = FMath::Max(
        GrowthDuration,
        UE_SMALL_NUMBER);
    if (Age < SafeGrowthDuration)
    {
        return FMath::SmoothStep(
            0.0f,
            1.0f,
            FMath::Clamp(Age / SafeGrowthDuration, 0.0f, 1.0f));
    }

    const float RetractionStart = SafeGrowthDuration
        + FMath::Max(IdleDuration, 0.0f);
    if (Age <= RetractionStart)
    {
        return 1.0f;
    }

    const float RetractionProgress = FMath::Clamp(
        (Age - RetractionStart)
            / FMath::Max(RetractionDuration, UE_SMALL_NUMBER),
        0.0f,
        1.0f);
    return 1.0f - FMath::SmoothStep(
        0.0f,
        1.0f,
        RetractionProgress);
}

float CMChimeraIdleTentacle::ResolveRetractionAlpha(
    const float Age,
    const float IdleDuration,
    const float RetractionDuration)
{
    if (Age <= IdleDuration)
    {
        return 1.0f;
    }

    return 1.0f - FMath::Clamp(
        (Age - IdleDuration)
            / FMath::Max(RetractionDuration, UE_SMALL_NUMBER),
        0.0f,
        1.0f);
}

FVector CMChimeraIdleTentacle::EvaluateSplinePoint(
    const FVector& Anchor,
    const FVector& Normal,
    const FVector& AxisX,
    const FVector& AxisY,
    const float NormalizedDistance,
    const float Length,
    const float WaveAmplitude,
    const float WavePhase,
    const float WaveCycles,
    const float RetractionAlpha)
{
    const float DistanceAlpha = FMath::Clamp(
        NormalizedDistance,
        0.0f,
        1.0f);
    const float LengthAlpha = FMath::Clamp(
        RetractionAlpha,
        0.0f,
        1.0f);
    const float WaveEnvelope = FMath::Sin(PI * DistanceAlpha)
        * LengthAlpha;
    const float Phase = WavePhase
        + DistanceAlpha * WaveCycles * 2.0f * PI;
    const FVector WaveOffset =
        (AxisX * FMath::Sin(Phase)
            + AxisY * FMath::Cos(Phase))
        * WaveAmplitude
        * WaveEnvelope;

    return Anchor
        + Normal * Length * DistanceAlpha * LengthAlpha
        + WaveOffset;
}

bool CMChimeraIdleTentacle::IsSpacedFromActiveAnchors(
    const FVector& Candidate,
    const TConstArrayView<FVector> ActiveAnchors,
    const float MinimumDistance)
{
    const float MinimumDistanceSquared = FMath::Square(
        FMath::Max(MinimumDistance, 0.0f));
    for (const FVector& Anchor : ActiveAnchors)
    {
        if (FVector::DistSquared(Candidate, Anchor)
            < MinimumDistanceSquared)
        {
            return false;
        }
    }
    return true;
}

UCMChimeraIdleTentacleComponent::UCMChimeraIdleTentacleComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    SetCanEverAffectNavigation(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh>
        DefaultTentacleMesh(
            TEXT("/Game/Vefects/Tentacles_VFX/VFX/Goo/SM/SM_VFX_Arm_03.SM_VFX_Arm_03"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface>
        DefaultTentacleMaterial(
            TEXT("/Game/Vefects/Tentacles_VFX/VFX/Goo/Materials/MI_VFX_Goo_Arm_01.MI_VFX_Goo_Arm_01"));
    if (DefaultTentacleMesh.Succeeded())
    {
        TentacleMesh = DefaultTentacleMesh.Object;
    }
    if (DefaultTentacleMaterial.Succeeded())
    {
        TentacleMaterial = DefaultTentacleMaterial.Object;
    }
}

void UCMChimeraIdleTentacleComponent::BeginPlay()
{
    Super::BeginPlay();

    if (!SourceMesh)
    {
        SourceMesh = Cast<UMeshComponent>(GetAttachParent());
    }
}

void UCMChimeraIdleTentacleComponent::TickComponent(
    const float DeltaTime,
    const ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEffectActive)
    {
        return;
    }

    if (SurfaceCandidates.IsEmpty())
    {
        SurfaceBuildRetryTime -= DeltaTime;
        if (SurfaceBuildRetryTime <= 0.0f
            && SurfaceBuildAttempts < MaximumSurfaceBuildAttempts)
        {
            ++SurfaceBuildAttempts;
            SurfaceBuildRetryTime = SurfaceBuildRetryInterval;
            if (BuildSurfaceCandidates())
            {
                EnsurePool();
            }
            else if (SurfaceBuildAttempts >= MaximumSurfaceBuildAttempts)
            {
                SetComponentTickEnabled(false);
            }
        }
        return;
    }

    EnsurePool();
    for (FCMChimeraIdleTentacleRuntime& Runtime : RuntimeTentacles)
    {
        if (Runtime.bActive)
        {
            UpdateTentacle(Runtime, DeltaTime);
            continue;
        }

        Runtime.RespawnDelay -= DeltaTime;
        if (Runtime.RespawnDelay <= 0.0f)
        {
            TryActivateTentacle(Runtime);
        }
    }
}

void UCMChimeraIdleTentacleComponent::ConfigureSource(
    UMeshComponent* InSourceMesh,
    const int32 SegmentIndex)
{
    SourceMesh = InSourceMesh;
    RandomStream.Initialize(7919 + SegmentIndex * 104729);
    NotifySourceMeshChanged();
}

void UCMChimeraIdleTentacleComponent::NotifySourceMeshChanged()
{
    SurfaceCandidates.Reset();
    SurfaceBuildAttempts = 0;
    SurfaceBuildRetryTime = 0.0f;
    for (FCMChimeraIdleTentacleRuntime& Runtime : RuntimeTentacles)
    {
        DeactivateTentacle(Runtime);
        Runtime.RespawnDelay = 0.0f;
    }

    SetComponentTickEnabled(bEffectActive
        && HasUsableSourceMesh()
        && TentacleMesh);
}

void UCMChimeraIdleTentacleComponent::SetEffectActive(
    const bool bInActive)
{
    const AActor* Owner = GetOwner();
    const bool bShouldBeActive = bInActive
        && Owner
        && Owner->GetNetMode() != NM_DedicatedServer;
    if (bEffectActive == bShouldBeActive)
    {
        return;
    }
    bEffectActive = bShouldBeActive;

    if (!bEffectActive)
    {
        SetComponentTickEnabled(false);
        for (FCMChimeraIdleTentacleRuntime& Runtime : RuntimeTentacles)
        {
            DeactivateTentacle(Runtime);
        }
        return;
    }

    NotifySourceMeshChanged();
}

int32 UCMChimeraIdleTentacleComponent::GetActiveTentacleCount() const
{
    int32 Count = 0;
    for (const FCMChimeraIdleTentacleRuntime& Runtime
        : RuntimeTentacles)
    {
        Count += Runtime.bActive ? 1 : 0;
    }
    return Count;
}

void UCMChimeraIdleTentacleComponent::OnComponentDestroyed(
    const bool bDestroyingHierarchy)
{
    DestroyPool();
    Super::OnComponentDestroyed(bDestroyingHierarchy);
}

bool UCMChimeraIdleTentacleComponent::BuildSurfaceCandidates()
{
    if (UStaticMeshComponent* StaticSource =
        Cast<UStaticMeshComponent>(SourceMesh))
    {
        return BuildStaticSurfaceCandidates(*StaticSource);
    }
    if (USkeletalMeshComponent* SkeletalSource =
        Cast<USkeletalMeshComponent>(SourceMesh))
    {
        return BuildSkeletalSurfaceCandidates(*SkeletalSource);
    }
    return false;
}

bool UCMChimeraIdleTentacleComponent::BuildSkeletalSurfaceCandidates(
    USkeletalMeshComponent& SkeletalSource)
{
    if (!SkeletalSource.GetMeshObject())
    {
        return false;
    }

    USkeletalMesh* Mesh = SkeletalSource.GetSkeletalMeshAsset();
    if (!Mesh)
    {
        return false;
    }

    const FSkeletalMeshSamplingInfo& SamplingInfo =
        Mesh->GetSamplingInfo();
    const int32 RegionIndex = SamplingInfo.IndexOfRegion(
        SamplingRegionName);
    if (RegionIndex == INDEX_NONE
        || !SamplingInfo.GetBuiltData().RegionBuiltData.IsValidIndex(
            RegionIndex))
    {
        return false;
    }

    const FSkeletalMeshSamplingRegion& Region =
        SamplingInfo.GetRegion(RegionIndex);
    const int32 LODIndex = Region.LODIndex == INDEX_NONE
        ? Mesh->GetLODNum() - 1
        : Region.LODIndex;
    FSkeletalMeshRenderData* RenderData =
        Mesh->GetResourceForRendering();
    if (!RenderData
        || !RenderData->LODRenderData.IsValidIndex(LODIndex))
    {
        return false;
    }

    const FSkeletalMeshSamplingRegionBuiltData& BuiltData =
        SamplingInfo.GetRegionBuiltData(RegionIndex);
    if (BuiltData.TriangleIndices.IsEmpty())
    {
        return false;
    }

    TArray<FFinalSkinVertex> SkinnedVertices;
    SkeletalSource.GetCPUSkinnedVertices(SkinnedVertices, LODIndex);
    if (SkinnedVertices.IsEmpty())
    {
        return false;
    }

    TArray<uint32> MeshIndices;
    RenderData->LODRenderData[LODIndex]
        .MultiSizeIndexContainer.GetIndexBuffer(MeshIndices);
    if (MeshIndices.IsEmpty())
    {
        return false;
    }

    const int32 CandidateCount = FMath::Max(
        TentacleCount * SurfaceCandidateMultiplier,
        TentacleCount);
    SurfaceCandidates.Reserve(CandidateCount);
    for (int32 CandidateIndex = 0;
        CandidateIndex < CandidateCount;
        ++CandidateIndex)
    {
        int32 RegionTriangleIndex = RandomStream.RandRange(
            0,
            BuiltData.TriangleIndices.Num() - 1);
        if (BuiltData.AreaWeightedSampler.GetNumEntries()
            == BuiltData.TriangleIndices.Num())
        {
            RegionTriangleIndex =
                BuiltData.AreaWeightedSampler.GetEntryIndex(
                    RandomStream.GetFraction(),
                    RandomStream.GetFraction());
        }

        const int32 TriangleBase =
            BuiltData.TriangleIndices[RegionTriangleIndex];
        if (!MeshIndices.IsValidIndex(TriangleBase + 2))
        {
            continue;
        }

        const int32 VertexIndices[] = {
            static_cast<int32>(MeshIndices[TriangleBase]),
            static_cast<int32>(MeshIndices[TriangleBase + 1]),
            static_cast<int32>(MeshIndices[TriangleBase + 2])
        };
        if (!SkinnedVertices.IsValidIndex(VertexIndices[0])
            || !SkinnedVertices.IsValidIndex(VertexIndices[1])
            || !SkinnedVertices.IsValidIndex(VertexIndices[2]))
        {
            continue;
        }

        const float Root = FMath::Sqrt(RandomStream.GetFraction());
        const float Split = RandomStream.GetFraction();
        const float Weights[] = {
            1.0f - Root,
            Root * (1.0f - Split),
            Root * Split
        };

        FVector SourcePosition = FVector::ZeroVector;
        FVector SourceNormal = FVector::ZeroVector;
        for (int32 Corner = 0; Corner < 3; ++Corner)
        {
            const FFinalSkinVertex& Vertex =
                SkinnedVertices[VertexIndices[Corner]];
            SourcePosition += FVector(Vertex.Position)
                * Weights[Corner];
            SourceNormal += Vertex.TangentZ.ToFVector()
                * Weights[Corner];
        }
        SourceNormal.Normalize();
        if (SourceNormal.IsNearlyZero())
        {
            const FVector EdgeA = FVector(
                SkinnedVertices[VertexIndices[1]].Position
                - SkinnedVertices[VertexIndices[0]].Position);
            const FVector EdgeB = FVector(
                SkinnedVertices[VertexIndices[2]].Position
                - SkinnedVertices[VertexIndices[0]].Position);
            SourceNormal = FVector::CrossProduct(
                EdgeA,
                EdgeB).GetSafeNormal();
        }
        AddSurfaceCandidate(SourcePosition, SourceNormal);
    }

    return !SurfaceCandidates.IsEmpty();
}

bool UCMChimeraIdleTentacleComponent::BuildStaticSurfaceCandidates(
    UStaticMeshComponent& StaticSource)
{
    UStaticMesh* Mesh = StaticSource.GetStaticMesh();
    const FStaticMeshRenderData* RenderData = Mesh
        ? Mesh->GetRenderData()
        : nullptr;
    if (!RenderData
        || !RenderData->LODResources.IsValidIndex(StaticMeshLODIndex))
    {
        return false;
    }

    const FStaticMeshLODResources& LOD =
        RenderData->LODResources[StaticMeshLODIndex];
    const FPositionVertexBuffer& Positions =
        LOD.VertexBuffers.PositionVertexBuffer;
    const FStaticMeshVertexBuffer& Vertices =
        LOD.VertexBuffers.StaticMeshVertexBuffer;
    const FIndexArrayView Indices = LOD.IndexBuffer.GetArrayView();
    if (Positions.GetNumVertices() == 0 || Indices.Num() < 3)
    {
        return false;
    }

    const FBox LocalMeshBounds = Mesh->GetBoundingBox();
    const float HeightThreshold = FMath::Lerp(
        LocalMeshBounds.Min.Z,
        LocalMeshBounds.Max.Z,
        FMath::Clamp(StaticMeshMinimumRelativeHeight, 0.0f, 1.0f));
    TArray<int32> EligibleTriangleBases;
    TArray<float> CumulativeAreas;
    float TotalArea = 0.0f;
    for (int32 TriangleBase = 0;
        TriangleBase + 2 < Indices.Num();
        TriangleBase += 3)
    {
        const uint32 VertexIndices[] = {
            Indices[TriangleBase],
            Indices[TriangleBase + 1],
            Indices[TriangleBase + 2]
        };
        if (VertexIndices[0] >= Positions.GetNumVertices()
            || VertexIndices[1] >= Positions.GetNumVertices()
            || VertexIndices[2] >= Positions.GetNumVertices())
        {
            continue;
        }

        const FVector A(Positions.VertexPosition(VertexIndices[0]));
        const FVector B(Positions.VertexPosition(VertexIndices[1]));
        const FVector C(Positions.VertexPosition(VertexIndices[2]));
        const FVector Centroid = (A + B + C) / 3.0f;
        const FVector4f TangentZ0 =
            Vertices.VertexTangentZ(VertexIndices[0]);
        const FVector4f TangentZ1 =
            Vertices.VertexTangentZ(VertexIndices[1]);
        const FVector4f TangentZ2 =
            Vertices.VertexTangentZ(VertexIndices[2]);
        FVector Normal = (
            FVector(TangentZ0.X, TangentZ0.Y, TangentZ0.Z)
            + FVector(TangentZ1.X, TangentZ1.Y, TangentZ1.Z)
            + FVector(TangentZ2.X, TangentZ2.Y, TangentZ2.Z))
            .GetSafeNormal();
        if (Normal.IsNearlyZero())
        {
            Normal = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
        }
        if (Centroid.Z < HeightThreshold
            || FVector::DotProduct(Normal, FVector::UpVector)
                < StaticMeshMinimumUpNormal)
        {
            continue;
        }

        const float Area = FVector::CrossProduct(B - A, C - A).Length()
            * 0.5f;
        if (Area <= UE_SMALL_NUMBER)
        {
            continue;
        }
        TotalArea += Area;
        EligibleTriangleBases.Add(TriangleBase);
        CumulativeAreas.Add(TotalArea);
    }
    if (EligibleTriangleBases.IsEmpty() || TotalArea <= UE_SMALL_NUMBER)
    {
        return false;
    }

    const int32 CandidateCount = FMath::Max(
        TentacleCount * SurfaceCandidateMultiplier,
        TentacleCount);
    SurfaceCandidates.Reserve(CandidateCount);
    for (int32 CandidateIndex = 0;
        CandidateIndex < CandidateCount;
        ++CandidateIndex)
    {
        const float AreaPick = RandomStream.FRandRange(0.0f, TotalArea);
        int32 EligibleIndex = 0;
        while (EligibleIndex + 1 < CumulativeAreas.Num()
            && AreaPick > CumulativeAreas[EligibleIndex])
        {
            ++EligibleIndex;
        }

        const int32 TriangleBase = EligibleTriangleBases[EligibleIndex];
        const uint32 VertexIndices[] = {
            Indices[TriangleBase],
            Indices[TriangleBase + 1],
            Indices[TriangleBase + 2]
        };
        const float Root = FMath::Sqrt(RandomStream.GetFraction());
        const float Split = RandomStream.GetFraction();
        const float Weights[] = {
            1.0f - Root,
            Root * (1.0f - Split),
            Root * Split
        };
        FVector SourcePosition = FVector::ZeroVector;
        FVector SourceNormal = FVector::ZeroVector;
        for (int32 Corner = 0; Corner < 3; ++Corner)
        {
            SourcePosition += FVector(
                Positions.VertexPosition(VertexIndices[Corner]))
                * Weights[Corner];
            const FVector4f TangentZ =
                Vertices.VertexTangentZ(VertexIndices[Corner]);
            SourceNormal += FVector(TangentZ.X, TangentZ.Y, TangentZ.Z)
                * Weights[Corner];
        }
        AddSurfaceCandidate(SourcePosition, SourceNormal.GetSafeNormal());
    }
    return !SurfaceCandidates.IsEmpty();
}

bool UCMChimeraIdleTentacleComponent::HasUsableSourceMesh() const
{
    if (const USkeletalMeshComponent* SkeletalSource =
        Cast<USkeletalMeshComponent>(SourceMesh))
    {
        return SkeletalSource->GetSkeletalMeshAsset() != nullptr;
    }
    if (const UStaticMeshComponent* StaticSource =
        Cast<UStaticMeshComponent>(SourceMesh))
    {
        return StaticSource->GetStaticMesh() != nullptr;
    }
    return false;
}

void UCMChimeraIdleTentacleComponent::AddSurfaceCandidate(
    const FVector& SourceLocalPosition,
    const FVector& SourceLocalNormal)
{
    if (!SourceMesh)
    {
        return;
    }

    const FVector WorldPosition = SourceMesh->GetComponentTransform()
        .TransformPosition(SourceLocalPosition);
    const FVector WorldNormal = SourceMesh->GetComponentTransform()
        .TransformVectorNoScale(SourceLocalNormal).GetSafeNormal();
    FSurfaceCandidate& Candidate =
        SurfaceCandidates.AddDefaulted_GetRef();
    Candidate.Position = GetComponentTransform()
        .InverseTransformPosition(WorldPosition);
    Candidate.Normal = GetComponentTransform()
        .InverseTransformVectorNoScale(WorldNormal).GetSafeNormal(
            UE_SMALL_NUMBER,
            FVector::UpVector);
}

void UCMChimeraIdleTentacleComponent::EnsurePool()
{
    if (!TentacleMesh || !GetOwner())
    {
        return;
    }

    const int32 PointCount = FMath::Max(SplinePointCount, 2);
    while (RuntimeTentacles.Num() < TentacleCount)
    {
        FCMChimeraIdleTentacleRuntime& Runtime =
            RuntimeTentacles.AddDefaulted_GetRef();
        Runtime.Spline = NewObject<USplineComponent>(
            GetOwner(),
            MakeUniqueObjectName(
                GetOwner(),
                USplineComponent::StaticClass(),
                TEXT("IdleTentacleSpline")));
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
                ESplinePointType::Curve,
                false);
        }
        Runtime.Spline->UpdateSpline();

        for (int32 SpanIndex = 0;
            SpanIndex < PointCount - 1;
            ++SpanIndex)
        {
            USplineMeshComponent* MeshSegment =
                NewObject<USplineMeshComponent>(
                    GetOwner(),
                    MakeUniqueObjectName(
                        GetOwner(),
                        USplineMeshComponent::StaticClass(),
                        TEXT("IdleTentacleMesh")));
            MeshSegment->SetMobility(EComponentMobility::Movable);
            MeshSegment->SetupAttachment(this);
            MeshSegment->SetCollisionEnabled(
                ECollisionEnabled::NoCollision);
            MeshSegment->SetGenerateOverlapEvents(false);
            MeshSegment->SetCanEverAffectNavigation(false);
            MeshSegment->SetCastShadow(false);
            MeshSegment->SetOwnerNoSee(false);
            MeshSegment->SetOnlyOwnerSee(false);
            MeshSegment->bUseAttachParentBound = false;
            MeshSegment->SetForwardAxis(ESplineMeshAxis::X, false);
            MeshSegment->SetStaticMesh(TentacleMesh);
            if (TentacleMaterial)
            {
                MeshSegment->SetMaterial(0, TentacleMaterial);
                if (UMaterialInstanceDynamic* DynamicMaterial =
                    MeshSegment->CreateDynamicMaterialInstance(
                        0,
                        TentacleMaterial))
                {
                    DynamicMaterial->SetScalarParameterValue(
                        TEXT("WPO Collapse Lerp"),
                        0.0f);
                }
            }
            MeshSegment->SetStartScale(
                FVector2D(TentacleWidth),
                false);
            MeshSegment->SetEndScale(
                FVector2D(TentacleWidth * 0.3f),
                false);
            MeshSegment->SetVisibility(false, true);
            MeshSegment->SetHiddenInGame(true, true);
            GetOwner()->AddInstanceComponent(MeshSegment);
            MeshSegment->RegisterComponent();
            Runtime.MeshSegments.Add(MeshSegment);
        }
    }
}

void UCMChimeraIdleTentacleComponent::DestroyPool()
{
    for (FCMChimeraIdleTentacleRuntime& Runtime : RuntimeTentacles)
    {
        for (USplineMeshComponent* MeshSegment : Runtime.MeshSegments)
        {
            if (MeshSegment)
            {
                MeshSegment->DestroyComponent();
            }
        }
        if (Runtime.Spline)
        {
            Runtime.Spline->DestroyComponent();
        }
    }
    RuntimeTentacles.Reset();
}

bool UCMChimeraIdleTentacleComponent::TryActivateTentacle(
    FCMChimeraIdleTentacleRuntime& Runtime)
{
    if (SurfaceCandidates.IsEmpty())
    {
        return false;
    }

    TArray<FVector, TInlineAllocator<16>> ActiveAnchors;
    for (const FCMChimeraIdleTentacleRuntime& Other
        : RuntimeTentacles)
    {
        if (Other.bActive && &Other != &Runtime)
        {
            ActiveAnchors.Add(Other.Anchor);
        }
    }

    const int32 StartIndex = RandomStream.RandRange(
        0,
        SurfaceCandidates.Num() - 1);
    const FSurfaceCandidate* SelectedCandidate = nullptr;
    for (int32 Offset = 0;
        Offset < SurfaceCandidates.Num();
        ++Offset)
    {
        const FSurfaceCandidate& Candidate = SurfaceCandidates[
            (StartIndex + Offset) % SurfaceCandidates.Num()];
        if (CMChimeraIdleTentacle::IsSpacedFromActiveAnchors(
            Candidate.Position,
            ActiveAnchors,
            MinimumSampleDistance))
        {
            SelectedCandidate = &Candidate;
            break;
        }
    }
    if (!SelectedCandidate)
    {
        Runtime.RespawnDelay = 0.1f;
        return false;
    }

    Runtime.Anchor = SelectedCandidate->Position;
    Runtime.Normal = SelectedCandidate->Normal.GetSafeNormal(
        UE_SMALL_NUMBER,
        FVector::UpVector);
    Runtime.Normal.FindBestAxisVectors(Runtime.AxisX, Runtime.AxisY);
    const float AxisAngle = RandomStream.FRandRange(0.0f, 2.0f * PI);
    Runtime.AxisX = Runtime.AxisX.RotateAngleAxis(
        FMath::RadiansToDegrees(AxisAngle),
        Runtime.Normal);
    Runtime.AxisY = FVector::CrossProduct(
        Runtime.Normal,
        Runtime.AxisX).GetSafeNormal();
    Runtime.WavePhase = RandomStream.FRandRange(0.0f, 2.0f * PI);
    Runtime.Age = 0.0f;
    Runtime.bActive = true;
    UpdateTentacle(Runtime, 0.0f);
    return true;
}

void UCMChimeraIdleTentacleComponent::DeactivateTentacle(
    FCMChimeraIdleTentacleRuntime& Runtime)
{
    Runtime.bActive = false;
    for (USplineMeshComponent* MeshSegment : Runtime.MeshSegments)
    {
        if (MeshSegment)
        {
            MeshSegment->SetVisibility(false, true);
            MeshSegment->SetHiddenInGame(true, true);
        }
    }
}

void UCMChimeraIdleTentacleComponent::UpdateTentacle(
    FCMChimeraIdleTentacleRuntime& Runtime,
    const float DeltaTime)
{
    if (!Runtime.Spline)
    {
        return;
    }

    Runtime.Age += DeltaTime;
    if (Runtime.Age
        >= GrowthDuration + IdleDuration + RetractionDuration)
    {
        DeactivateTentacle(Runtime);
        const float MinimumDelay = FMath::Min(
            RespawnDelayRange.X,
            RespawnDelayRange.Y);
        const float MaximumDelay = FMath::Max(
            RespawnDelayRange.X,
            RespawnDelayRange.Y);
        Runtime.RespawnDelay = RandomStream.FRandRange(
            MinimumDelay,
            MaximumDelay);
        return;
    }

    const float LengthAlpha =
        CMChimeraIdleTentacle::ResolveLifecycleLengthAlpha(
            Runtime.Age,
            GrowthDuration,
            IdleDuration,
            RetractionDuration);
    if (LengthAlpha <= MinimumRenderableLengthAlpha)
    {
        for (USplineMeshComponent* MeshSegment : Runtime.MeshSegments)
        {
            if (MeshSegment)
            {
                MeshSegment->SetVisibility(false, true);
                MeshSegment->SetHiddenInGame(true, true);
            }
        }
        return;
    }

    const int32 PointCount = Runtime.Spline->GetNumberOfSplinePoints();
    for (int32 PointIndex = 0;
        PointIndex < PointCount;
        ++PointIndex)
    {
        const float DistanceAlpha = static_cast<float>(PointIndex)
            / static_cast<float>(FMath::Max(PointCount - 1, 1));
        Runtime.Spline->SetLocationAtSplinePoint(
            PointIndex,
            CMChimeraIdleTentacle::EvaluateSplinePoint(
                Runtime.Anchor,
                Runtime.Normal,
                Runtime.AxisX,
                Runtime.AxisY,
                DistanceAlpha,
                TentacleLength,
                WaveAmplitude,
                Runtime.WavePhase + Runtime.Age * WaveSpeed,
                WaveCycles,
                LengthAlpha),
            ESplineCoordinateSpace::Local,
            false);
    }
    Runtime.Spline->UpdateSpline();

    for (int32 SpanIndex = 0;
        SpanIndex < Runtime.MeshSegments.Num();
        ++SpanIndex)
    {
        USplineMeshComponent* MeshSegment =
            Runtime.MeshSegments[SpanIndex];
        if (!MeshSegment)
        {
            continue;
        }

        const FVector StartPosition =
            Runtime.Spline->GetLocationAtSplinePoint(
                SpanIndex,
                ESplineCoordinateSpace::Local);
        const FVector EndPosition =
            Runtime.Spline->GetLocationAtSplinePoint(
                SpanIndex + 1,
                ESplineCoordinateSpace::Local);
        const FVector SpanDelta = EndPosition - StartPosition;
        FVector StartTangent = SpanDelta;
        if (SpanIndex > 0)
        {
            const FVector PreviousPosition =
                Runtime.Spline->GetLocationAtSplinePoint(
                    SpanIndex - 1,
                    ESplineCoordinateSpace::Local);
            StartTangent = (EndPosition - PreviousPosition) * 0.5f;
        }
        FVector EndTangent = SpanDelta;
        if (SpanIndex + 2 < PointCount)
        {
            const FVector NextPosition =
                Runtime.Spline->GetLocationAtSplinePoint(
                    SpanIndex + 2,
                    ESplineCoordinateSpace::Local);
            EndTangent = (NextPosition - StartPosition) * 0.5f;
        }
        MeshSegment->SetStartAndEnd(
            StartPosition,
            StartTangent,
            EndPosition,
            EndTangent,
            false);
        const float StartAlpha = static_cast<float>(SpanIndex)
            / static_cast<float>(FMath::Max(PointCount - 1, 1));
        const float EndAlpha = static_cast<float>(SpanIndex + 1)
            / static_cast<float>(FMath::Max(PointCount - 1, 1));
        MeshSegment->SetStartScale(
            FVector2D(TentacleWidth * FMath::Lerp(1.0f, 0.3f, StartAlpha)),
            false);
        MeshSegment->SetEndScale(
            FVector2D(TentacleWidth * FMath::Lerp(1.0f, 0.3f, EndAlpha)),
            false);
        MeshSegment->UpdateMesh();
        MeshSegment->UpdateBounds();
        MeshSegment->MarkRenderTransformDirty();
        MeshSegment->SetHiddenInGame(false, true);
        MeshSegment->SetVisibility(true, true);
    }

}
