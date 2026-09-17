#include "DataForgeFolderSource.h"

void FDataForgeAssetLayoutRecipeLibrary::ConfigureChimeraCharacter(UDataForgeAssetLayoutRecipe& Recipe)
{
	Recipe.RecipeId = TEXT("ChimeraCharacter");
	Recipe.Domain = TEXT("Character");
	Recipe.SubjectSource = EDataForgeLayoutSubjectSource::FolderSegment;
	Recipe.SubjectFolderIndex = 0;
	Recipe.FixedSubject.Reset();
	Recipe.KindFolderIndex = 1;
	Recipe.bRequireKindFolderMatch = true;
}

void FDataForgeAssetLayoutRecipeLibrary::ConfigureChimeraObstacle(UDataForgeAssetLayoutRecipe& Recipe)
{
	Recipe.RecipeId = TEXT("ChimeraObstacle");
	Recipe.Domain = TEXT("Environment");
	Recipe.SubjectSource = EDataForgeLayoutSubjectSource::Fixed;
	Recipe.FixedSubject = TEXT("Obstacle");
	Recipe.KindFolderIndex = 0;
	Recipe.bRequireKindFolderMatch = true;
}

void FDataForgeAssetLayoutRecipeLibrary::ConfigureChimeraAbility(UDataForgeAssetLayoutRecipe& Recipe)
{
	Recipe.RecipeId = TEXT("ChimeraAbility");
	Recipe.Domain = TEXT("Ability");
	Recipe.SubjectSource = EDataForgeLayoutSubjectSource::AssetName;
	Recipe.FixedSubject.Reset();
	Recipe.KindFolderIndex = 0;
	Recipe.bRequireKindFolderMatch = true;
}

void FDataForgeAssetLayoutRecipeLibrary::ConfigureChimeraUI(UDataForgeAssetLayoutRecipe& Recipe)
{
	Recipe.RecipeId = TEXT("ChimeraUI");
	Recipe.Domain = TEXT("UI");
	Recipe.SubjectSource = EDataForgeLayoutSubjectSource::AssetName;
	Recipe.FixedSubject.Reset();
	Recipe.KindFolderIndex = INDEX_NONE;
	Recipe.bRequireKindFolderMatch = false;
}
