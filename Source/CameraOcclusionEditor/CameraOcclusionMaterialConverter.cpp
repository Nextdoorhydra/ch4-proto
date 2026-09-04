#include "CameraOcclusionMaterialConverter.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionDistance.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMin.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionPerInstanceCustomData.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionScreenPosition.h"
#include "Materials/MaterialExpressionSmoothStep.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialFunction.h"
#include "MaterialEditingLibrary.h"
#include "UObject/UnrealType.h"

namespace
{
		const TCHAR* CameraOcclusionDitherFunctionPath = TEXT("/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA.DitherTemporalAA");
		const FName CameraOcclusionFadeParameter(TEXT("CM_OcclusionFade"));
		const FName CameraOcclusionUseInstanceFadeParameter(TEXT("CM_OcclusionUseInstanceFade"));
		const FName CameraOcclusionCenterParameter(TEXT("CM_OcclusionCenter"));
		const FName CameraOcclusionRadiusParameter(TEXT("CM_OcclusionRadius"));
		const FName CameraOcclusionMinOpacityParameter(TEXT("CM_OcclusionMinOpacity"));
		const FName CameraOcclusionEdgeSoftnessParameter(TEXT("CM_OcclusionEdgeSoftness"));
	
		template <typename T>
		T* CreateCameraOcclusionExpression(UMaterial* Material)
		{
			return Cast<T>(UMaterialEditingLibrary::CreateMaterialExpression(Material, T::StaticClass()));
		}
	
		bool HasCameraOcclusionSupport(const UMaterial& Material)
		{
			const UMaterialEditorOnlyData* EditorOnlyData = Material.GetEditorOnlyData();
			if (!EditorOnlyData)
			{
				return false;
			}
			bool bHasFadeParameter = false;
			bool bHasInstanceFadeParameter = false;
			for (const UMaterialExpression* Expression : EditorOnlyData->ExpressionCollection.Expressions)
			{
				const UMaterialExpressionParameter* Parameter = Cast<UMaterialExpressionParameter>(Expression);
				if (!Parameter)
				{
					continue;
				}
				if (Parameter->ParameterName == CameraOcclusionFadeParameter)
				{
					bHasFadeParameter = true;
				}
				else if (Parameter->ParameterName == CameraOcclusionUseInstanceFadeParameter)
				{
					bHasInstanceFadeParameter = true;
				}
			}
			return bHasFadeParameter && bHasInstanceFadeParameter;
		}
	
		UMaterialExpressionScalarParameter* CreateScalarParameter(UMaterial* Material, FName Name, float DefaultValue)
		{
			UMaterialExpressionScalarParameter* Parameter = CreateCameraOcclusionExpression<UMaterialExpressionScalarParameter>(Material);
			if (Parameter)
			{
				Parameter->ParameterName = Name;
				Parameter->DefaultValue = DefaultValue;
			}
			return Parameter;
		}
	
		UMaterialExpressionVectorParameter* CreateVectorParameter(UMaterial* Material, FName Name, const FLinearColor& DefaultValue)
		{
			UMaterialExpressionVectorParameter* Parameter = CreateCameraOcclusionExpression<UMaterialExpressionVectorParameter>(Material);
			if (Parameter)
			{
				Parameter->ParameterName = Name;
				Parameter->DefaultValue = DefaultValue;
			}
			return Parameter;
		}
	
		bool AddCameraOcclusionSupport(UMaterial& Material, FString& OutReason)
		{
		if (HasCameraOcclusionSupport(Material))
		{
			bool bChanged = false;
			if (Material.DitherOpacityMask)
			{
				Material.DitherOpacityMask = false;
				bChanged = true;
			}
			if (Material.bUsedWithStaticLighting)
			{
				Material.bUsedWithStaticLighting = false;
				bChanged = true;
			}
			if (bChanged)
			{
				Material.MarkPackageDirty();
				OutReason = TEXT("updated camera occlusion material settings");
				return true;
			}
			OutReason = TEXT("already supported");
			return false;
			}
			if (Material.MaterialDomain != MD_Surface)
			{
				OutReason = TEXT("not a Surface material");
				return false;
			}
			if (Material.bUsedWithParticleSprites
				|| Material.bUsedWithMeshParticles
				|| Material.bUsedWithNiagaraSprites
				|| Material.bUsedWithNiagaraRibbons
				|| Material.bUsedWithNiagaraMeshParticles)
			{
				OutReason = TEXT("particle or Niagara material");
				return false;
			}
			if (Material.GetBlendMode() != BLEND_Opaque && Material.GetBlendMode() != BLEND_Masked)
			{
				OutReason = TEXT("blend mode is not Opaque or Masked");
				return false;
			}
	
			UMaterialFunction* DitherFunction = LoadObject<UMaterialFunction>(nullptr, CameraOcclusionDitherFunctionPath);
			if (!DitherFunction)
			{
				OutReason = TEXT("DitherTemporalAA function could not be loaded");
				return false;
			}
	
			FExpressionInput OriginalMask = Material.GetEditorOnlyData()->OpacityMask;
			UMaterialExpressionConstant* One = CreateCameraOcclusionExpression<UMaterialExpressionConstant>(&Material);
			UMaterialExpressionScalarParameter* Fade = CreateScalarParameter(&Material, CameraOcclusionFadeParameter, 0.0f);
			UMaterialExpressionScalarParameter* UseInstanceFade = CreateScalarParameter(&Material, CameraOcclusionUseInstanceFadeParameter, 0.0f);
			UMaterialExpressionPerInstanceCustomData* InstanceFade =
				CreateCameraOcclusionExpression<UMaterialExpressionPerInstanceCustomData>(&Material);
			UMaterialExpressionScalarParameter* Radius = CreateScalarParameter(&Material, CameraOcclusionRadiusParameter, 0.15f);
			UMaterialExpressionScalarParameter* MinOpacity = CreateScalarParameter(&Material, CameraOcclusionMinOpacityParameter, 0.1f);
			UMaterialExpressionScalarParameter* EdgeSoftness = CreateScalarParameter(&Material, CameraOcclusionEdgeSoftnessParameter, 0.03f);
			UMaterialExpressionScreenPosition* ScreenPosition = CreateCameraOcclusionExpression<UMaterialExpressionScreenPosition>(&Material);
			UMaterialExpressionComponentMask* ScreenUV = CreateCameraOcclusionExpression<UMaterialExpressionComponentMask>(&Material);
			if (!One || !Fade || !UseInstanceFade || !InstanceFade || !Radius || !MinOpacity || !EdgeSoftness || !ScreenPosition || !ScreenUV)
			{
				OutReason = TEXT("failed to create material expressions");
				return false;
			}
			One->R = 1.0f;
			InstanceFade->DataIndex = 0;
			InstanceFade->ConstDefaultValue = 0.0f;
			ScreenUV->R = true;
			ScreenUV->G = true;
			ScreenUV->Input.Expression = ScreenPosition;
	
			UMaterialExpression* MinimumDistance = nullptr;
			for (int32 Index = 0; Index < 8; ++Index)
			{
				const FName CenterName = Index == 0
					? CameraOcclusionCenterParameter
					: FName(*FString::Printf(TEXT("CM_OcclusionCenter%d"), Index));
				UMaterialExpressionVectorParameter* Center = CreateVectorParameter(&Material, CenterName, FLinearColor(10.0f, 10.0f, 0.0f, 0.0f));
				UMaterialExpressionComponentMask* CenterRG = CreateCameraOcclusionExpression<UMaterialExpressionComponentMask>(&Material);
				UMaterialExpressionDistance* Distance = CreateCameraOcclusionExpression<UMaterialExpressionDistance>(&Material);
				if (!Center || !CenterRG || !Distance)
				{
					OutReason = TEXT("failed to create screen-center expressions");
					return false;
				}
				CenterRG->R = true;
				CenterRG->G = true;
				CenterRG->Input.Expression = Center;
				Distance->A.Expression = ScreenUV;
				Distance->B.Expression = CenterRG;
				if (!MinimumDistance)
				{
					MinimumDistance = Distance;
				}
				else
				{
					UMaterialExpressionMin* Min = CreateCameraOcclusionExpression<UMaterialExpressionMin>(&Material);
					if (!Min)
					{
						OutReason = TEXT("failed to create minimum-distance expression");
						return false;
					}
					Min->A.Expression = MinimumDistance;
					Min->B.Expression = Distance;
					MinimumDistance = Min;
				}
			}
	
			UMaterialExpressionAdd* RadiusWithEdge = CreateCameraOcclusionExpression<UMaterialExpressionAdd>(&Material);
			UMaterialExpressionSmoothStep* SoftCircle = CreateCameraOcclusionExpression<UMaterialExpressionSmoothStep>(&Material);
			UMaterialExpressionLinearInterpolate* LocalOpacity = CreateCameraOcclusionExpression<UMaterialExpressionLinearInterpolate>(&Material);
			UMaterialExpressionLinearInterpolate* FadeSource = CreateCameraOcclusionExpression<UMaterialExpressionLinearInterpolate>(&Material);
			UMaterialExpressionLinearInterpolate* FadedOpacity = CreateCameraOcclusionExpression<UMaterialExpressionLinearInterpolate>(&Material);
			UMaterialExpressionMaterialFunctionCall* Dither = CreateCameraOcclusionExpression<UMaterialExpressionMaterialFunctionCall>(&Material);
			UMaterialExpressionMultiply* CombinedMask = CreateCameraOcclusionExpression<UMaterialExpressionMultiply>(&Material);
			if (!RadiusWithEdge || !SoftCircle || !LocalOpacity || !FadeSource || !FadedOpacity || !Dither || !CombinedMask)
			{
				OutReason = TEXT("failed to create fade expressions");
				return false;
			}
			RadiusWithEdge->A.Expression = Radius;
			RadiusWithEdge->B.Expression = EdgeSoftness;
			SoftCircle->Min.Expression = Radius;
			SoftCircle->Max.Expression = RadiusWithEdge;
			SoftCircle->Value.Expression = MinimumDistance;
			LocalOpacity->A.Expression = MinOpacity;
			LocalOpacity->B.Expression = One;
			LocalOpacity->Alpha.Expression = SoftCircle;
			FadeSource->A.Expression = Fade;
			FadeSource->B.Expression = InstanceFade;
			FadeSource->Alpha.Expression = UseInstanceFade;
			FadedOpacity->A.Expression = One;
			FadedOpacity->B.Expression = LocalOpacity;
			FadedOpacity->Alpha.Expression = FadeSource;
			Dither->SetMaterialFunction(DitherFunction);
			Dither->UpdateFromFunctionResource();
			if (Dither->FunctionInputs.IsEmpty())
			{
				OutReason = TEXT("DitherTemporalAA has no input pin");
				return false;
			}
			Dither->FunctionInputs[0].Input.Expression = FadedOpacity;
			if (OriginalMask.Expression)
			{
				CombinedMask->A = OriginalMask;
			}
			else
			{
				CombinedMask->A.Expression = One;
			}
			// Keep the graph pending for the developer to Apply and Save in the material editor.
			CombinedMask->B.Expression = FadedOpacity;
			Material.GetEditorOnlyData()->OpacityMask.Expression = CombinedMask;
			Material.BlendMode = BLEND_Masked;
			Material.DitherOpacityMask = false;
			Material.bUsedWithStaticLighting = false;
			UMaterialEditingLibrary::LayoutMaterialExpressions(&Material);
			Material.MarkPackageDirty();
			return true;
		}
}

void FCameraOcclusionMaterialConverter::ConvertSelectedMaterials(const TArray<FAssetData>& SelectedAssets)
{
	int32 MaterialCount = 0;
	int32 ConvertedCount = 0;
	for (const FAssetData& Asset : SelectedAssets)
	{
		if (Asset.AssetClassPath == UMaterial::StaticClass()->GetClassPathName())
		{
			++MaterialCount;
			if (UMaterial* Material = Cast<UMaterial>(Asset.GetAsset()))
			{
				FString Reason;
				ConvertedCount += AddCameraOcclusionSupport(*Material, Reason) ? 1 : 0;
				if (!Reason.IsEmpty())
				{
					UE_LOG(LogTemp, Display, TEXT("[Camera Occlusion] Skipped %s: %s"), *Material->GetPathName(), *Reason);
				}
			}
		}
	}
	UE_LOG(LogTemp, Display, TEXT("[Camera Occlusion] Converted %d/%d selected material(s)."), ConvertedCount, MaterialCount);
}

void FCameraOcclusionMaterialConverter::ConvertMaterialsUnderFolders(const TArray<FString>& SelectedPackagePaths)
{
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());
	for (const FString& Path : SelectedPackagePaths) Filter.PackagePaths.Add(FName(*Path));
	TArray<FAssetData> Materials;
	AssetRegistry.GetAssets(Filter, Materials);
	ConvertSelectedMaterials(Materials);
	UE_LOG(LogTemp, Display, TEXT("[Camera Occlusion] Folder scan completed for %d folder(s)."), SelectedPackagePaths.Num());
}
