# NKM Sound

`NKMSound` is a loader-independent runtime audio plugin. It maps `GameplayTag` values to sound rules and centralizes SFX, UI sound, BGM, volume, fading, ducking, filtering, and runtime concurrency policy.

The plugin intentionally does not depend on a project-specific asset loader. The host project owns catalog creation, asynchronous loading, registration, lifetime, and unloading.

## Public API

- `UNKMSoundSubsystem`: SFX, UI sound, BGM, Crowd, Voice, volume, and runtime mix control
- `FNKMSoundEntry`: the reusable per-sound data shape
- `INKMSoundCatalogProvider`: the loader-neutral catalog contract
- `UNKMSoundDataAsset`: a built-in `UPrimaryDataAsset` provider for projects using the standard AssetManager path
- `FNKMSoundHelper`: world-context lookup helper for `UNKMSoundSubsystem`

## Required architecture

Every registered catalog object must implement `INKMSoundCatalogProvider`:

```cpp
class NKMSOUNDRUNTIME_API INKMSoundCatalogProvider
{
public:
    virtual const TArray<FNKMSoundEntry>& GetSoundEntries() const = 0;
    virtual FPrimaryAssetId GetSoundCatalogId() const = 0;
};
```

The host must complete these operations in order:

1. Create a Primary Data Asset class that implements `INKMSoundCatalogProvider`.
2. Author `FNKMSoundEntry` values in catalog assets.
3. Load the catalog and its `Gameplay` bundle asynchronously.
4. Keep the loaded catalog and bundled sound assets alive.
5. Pass the loaded object to `UNKMSoundSubsystem::RegisterSoundCatalog()`.
6. Call playback APIs only after registration succeeds.
7. Call `ClearSoundDataAssets()` before unloading all registered catalogs.

`RegisterSoundDataAsset()` remains as a convenience overload for the built-in `UNKMSoundDataAsset`. Custom project data assets must use `RegisterSoundCatalog()`.

## Choose one integration path

### Path A: standard AssetManager project

Use the built-in `UNKMSoundDataAsset` when the project can load ordinary `UPrimaryDataAsset` objects.

1. Create `UNKMSoundDataAsset` assets.
2. Fill their `SoundEntries` arrays.
3. Configure the `NKMSoundDataAsset` Primary Asset Type.
4. Load each catalog with the `Gameplay` bundle.
5. Register the loaded asset with `RegisterSoundDataAsset()` or `RegisterSoundCatalog()`.

This path requires no project-specific data-asset class.

### Path B: project-specific loader base

Use this path when the project loader only accepts assets derived from its own base class. Do not make `NKMSound` depend on that loader and do not change `UNKMSoundDataAsset` to inherit from the loader base.

Instead, define a small adapter class inside the host project:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "MyProjectPrimaryDataAssetBase.h"
#include "Sound/NKMSoundCatalogProvider.h"
#include "MyGameSoundDataAsset.generated.h"

UCLASS(BlueprintType)
class MYGAME_API UMyGameSoundDataAsset
    : public UMyProjectPrimaryDataAssetBase
    , public INKMSoundCatalogProvider
{
    GENERATED_BODY()

public:
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(
            FPrimaryAssetType(TEXT("NKMSoundDataAsset")),
            GetFName());
    }

    virtual const TArray<FNKMSoundEntry>& GetSoundEntries() const override
    {
        return SoundEntries;
    }

    virtual FPrimaryAssetId GetSoundCatalogId() const override
    {
        return GetPrimaryAssetId();
    }

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
    TArray<FNKMSoundEntry> SoundEntries;
};
```

Replace `UMyProjectPrimaryDataAssetBase` and `MYGAME_API` with the host project's equivalents. The adapter belongs in the project module, not in `NKMSound`.

Using the fixed `NKMSoundDataAsset` Primary Asset Type allows settings and schedules to keep stable IDs such as:

```text
NKMSoundDataAsset:DA_MyGameSound_PreLoad
```

Configure AssetManager to scan the project class, not the built-in plugin class:

```ini
[/Script/Engine.AssetManagerSettings]
+PrimaryAssetTypesToScan=(PrimaryAssetType="NKMSoundDataAsset",AssetBaseClass="/Script/MyGame.MyGameSoundDataAsset",bHasBlueprintClasses=False,bIsEditorOnly=False,Directories=((Path="/Game/MyGame/Data/Sound")),SpecificAssets=,Rules=(Priority=-1,ChunkId=-1,bApplyRecursively=True,CookRule=AlwaysCook))
```

Only one scan rule should own the `NKMSoundDataAsset` type in a host project. Do not scan both the built-in and project adapter classes under the same Primary Asset Type.

## Bundle requirements

`FNKMSoundEntry` marks these soft references with `AssetBundles="Gameplay"`:

- `Sound`
- `Attenuation`
- `Concurrency`

The loader must request the catalog's `Gameplay` bundle. Loading only the catalog object is insufficient: the subsystem intentionally avoids synchronous fallback loads during playback.

If a referenced sound is not already loaded, playback is skipped and an error is logged. This is a violated preload invariant, not a request to load synchronously.

## Loader completion bridge

The host project should adapt its loader completion callback to the sound subsystem. The essential flow is:

```cpp
void UMyGameSoundBridgeSubsystem::HandleCatalogLoaded(
    const FPrimaryAssetId& AssetId)
{
    UObject* LoadedCatalog = MyProjectLoader->FindCachedAsset(AssetId);
    if (!LoadedCatalog || !SoundSubsystem)
    {
        return;
    }

    SoundSubsystem->RegisterSoundCatalog(LoadedCatalog);
}
```

`RegisterSoundCatalog()` rejects null objects, objects that do not implement `INKMSoundCatalogProvider`, and duplicate object registration.

When restoring a session from an already-populated loader cache:

1. Call `ClearSoundDataAssets()`.
2. Enumerate the configured sound catalog IDs.
3. Retrieve each cached provider object.
4. Register each provider again.

## Lifetime and asynchronous safety

The host loader and bridge own catalog lifetime. Follow these rules:

- Keep each registered provider alive while its entries may be used.
- Keep the requested `Gameplay` bundle loaded for the same period.
- Cancel outstanding requests when the owning GameInstance or bridge is torn down.
- Ignore stale callbacks from an older loading session or generation.
- Do not register an object until its catalog and bundle load has completed successfully.
- Do not unload a registered provider before calling `ClearSoundDataAssets()` or rebuilding registration.
- Treat duplicate completion messages as harmless; duplicate object registration returns `false`.

The sound subsystem keeps registered provider objects referenced, but it does not own the loader's bundle handles or cache policy.

## Why wrapper assets are not recommended

Avoid creating one loader asset that only points to a second `UNKMSoundDataAsset` unless the loader explicitly supports recursive bundle expansion.

A simple wrapper introduces two assets per catalog and does not guarantee that the inner catalog's `Gameplay` bundle will be loaded. The loader may report the wrapper as complete while the actual `USoundBase`, attenuation, and concurrency assets remain unloaded.

Embedding `FNKMSoundEntry` directly in the project adapter avoids this ambiguity. One project catalog then represents one loader request, one Primary Asset ID, one bundle lifetime, and one registration event.

## Settings and routing

`UNKMSoundSettings` contains:

- `SoundDataAssetIds`: all catalogs the host expects to use
- `SoundDataAssetIdByTagRoot`: the expected catalog for each sound-tag root
- runtime mix values such as BGM crossfade, crowd fade, time-dilation pitch, and low-pass settings

The host project may expose these settings directly or forward them through a project-specific settings adapter. A loader with requirement validation should expose the configured sound IDs from a project-owned requirement source; the plugin itself must not implement the loader's interface.

Routing uses the most specific matching gameplay-tag root. Every authored `SoundTag` should resolve to the catalog that actually contains it. Route mismatches are logged as errors.

## Volume persistence

Runtime volume values live in `UNKMSoundSubsystem`. A host that needs persistence should bind to `OnVolumeSaveRequested()` and store the Master, BGM, and SFX values in its own user-settings system.

The plugin does not depend on the host's `GameUserSettings` class or options UI.

## NetKarma reference implementation

NetKarma follows Path B:

- `UNKMGameSoundDataAsset` derives from the project-local `UPrimaryDataAssetBase` and implements `INKMSoundCatalogProvider`.
- `AsyncPDALoader` loads the project catalog and its `Gameplay` bundle according to the existing stage schedule.
- `GameplayMessageRouter` carries the loader completion message.
- `UNKMGameSoundBridgeSubsystem` retrieves the cached provider and calls `RegisterSoundCatalog()`.
- `UNKMGameSoundBridgeSubsystem` implements `IAsyncLoadRequirementSource` so project validation can verify that configured sound catalogs are reachable from the schedule.
- The project adapter, not `NKMSound`, owns dependencies on `AsyncPDALoader` and `GameplayMessageRouter`.

Relevant files:

```text
Source/NetKarmaGame/Sound/NKMGameSoundDataAsset.h
Source/NetKarmaGame/Sound/NKMGameSoundBridgeSubsystem.h
Source/NetKarmaGame/Sound/NKMGameSoundBridgeSubsystem.cpp
Config/DefaultGame.ini
Content/NetKarma/Data/SoundAsync/
```

## Integration checklist

Before considering a host integration complete, verify all of the following:

- `NKMSound` builds without the project loader plugin.
- The consuming module depends on `NKMSoundRuntime`.
- The project adapter implements both provider functions.
- AssetManager scans the project adapter class under the intended Primary Asset Type.
- Catalog IDs referenced by settings and schedules resolve to the new project assets.
- The loader requests the `Gameplay` bundle.
- Successful completion registers the cached provider.
- Cancellation and teardown cannot register stale objects.
- Registered providers remain alive until cleared.
- Every sound tag resolves to the catalog that contains it.
- Playback performs no synchronous asset load.

## Policy boundary

The plugin owns reusable audio mechanics:

- gameplay-audio fade and low-pass transition
- temporary layer ducking
- source isolation by a caller-provided gameplay-tag root
- normalized crowd-loop intensity
- tag-to-entry lookup and playback limits

The host project owns the reason and timing for those mechanics. Concepts such as death, sirens, enemy types, wave counts, cutscene state, loader schedules, and user-settings persistence remain in the host project.

## NetKarma footstep authoring

For new or edited animations, place `UNKMAnimNotify_Footstep` on the frame where the foot contacts the ground.

- `SoundTag` selects the catalog entry. The player default is `NKM.Sound.Player.Footstep`.
- `FootSide` selects `foot_l` or `foot_r`.
- `bUseFootSocket` plays the sound attached to the selected foot socket.
- If the socket does not exist, playback falls back to the owning actor location.
- Dash and configured mute-ordeal states suppress playback in the project adapter.
- During the prep/selection intermission, player footsteps use the intermission footstep tag.

Some existing animations still contain serialized `UNKMAnimNotifyContextEffects` compatibility notifies migrated from the previous implementation. If an animation already contains `UNKMAnimNotify_Footstep`, the compatibility notify is suppressed to prevent duplicate playback.

Do not add new `UNKMAnimNotifyContextEffects` entries. Use `UNKMAnimNotify_Footstep` for new work and keep sound selection in the project's sound catalog provider assets.
