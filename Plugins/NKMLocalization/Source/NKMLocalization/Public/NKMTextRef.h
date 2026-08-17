#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NKMTextRef.generated.h"

USTRUCT(BlueprintType)
struct NKMLOCALIZATION_API FNKMTextRef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Localization")
	FName TableId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Localization")
	FString Key;

	FNKMTextRef() = default;
	FNKMTextRef(FName InTableId, FString InKey)
		: TableId(InTableId), Key(MoveTemp(InKey))
	{
	}

	bool IsSet() const { return !TableId.IsNone() && !Key.IsEmpty(); }
	FString ToString() const;
	FText Resolve() const;

	static bool Parse(const FString& Value, FName DefaultTableId, FNKMTextRef& OutRef, FString* OutError = nullptr);
	static bool ResolveTableId(FName AliasOrObjectPath, FName& OutRuntimeTableId);

	bool ExportTextItem(FString& ValueStr, const FNKMTextRef& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const;
	bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText);

	bool operator==(const FNKMTextRef& Other) const { return TableId == Other.TableId && Key == Other.Key; }
	bool operator!=(const FNKMTextRef& Other) const { return !(*this == Other); }
};

template<>
struct TStructOpsTypeTraits<FNKMTextRef> : public TStructOpsTypeTraitsBase2<FNKMTextRef>
{
	enum
	{
		WithIdenticalViaEquality = true,
		WithExportTextItem = true,
		WithImportTextItem = true
	};
};

UCLASS()
class NKMLOCALIZATION_API UNKMLocalizationTextLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="NKM|Localization")
	static FText ResolveTextReference(const FNKMTextRef& TextReference);

	UFUNCTION(BlueprintPure, Category="NKM|Localization")
	static bool ParseTextReference(const FString& Value, FName DefaultTableId, FNKMTextRef& OutReference);

	UFUNCTION(BlueprintPure, Category="NKM|Localization")
	static bool MakeBoundTextReference(FName BindingProfile, FName RecordId, FName FieldName, FNKMTextRef& OutReference);

	/** Canonical runtime entry point for data-driven localized fields. */
	UFUNCTION(BlueprintPure, Category="NKM|Localization")
	static FText ResolveBoundText(FName BindingProfile, FName RecordId, FName FieldName);
};
