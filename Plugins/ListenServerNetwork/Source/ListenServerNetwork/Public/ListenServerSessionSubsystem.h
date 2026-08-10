#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "ListenServerNetworkTypes.h"
#include "ListenServerSessionSubsystem.generated.h"

struct FListenServerSessionSubsystemImpl;

UCLASS()
class LISTENSERVERNETWORK_API UListenServerSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UListenServerSessionSubsystem();
	virtual ~UListenServerSessionSubsystem() override;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	bool HostSession(const FListenServerHostRequest& Request);
	bool FindSessions(const FListenServerSearchRequest& Request);

	UFUNCTION(BlueprintCallable, Category="Listen Server Network")
	bool JoinSession(const FListenServerSearchResultHandle& ResultHandle);

	bool StartQuickMatch(const FListenServerQuickMatchRequest& Request);
	bool LeaveSession();
	bool HostTravelToMap(const FSoftObjectPath& Map);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Listen Server Network")
	bool UpdateHostedSessionState(EListenServerAdvertisedSessionState NewState, bool bAllowNewParticipants);

	UFUNCTION(BlueprintCallable, Category="Listen Server Network")
	bool ShowInviteUI();

	bool CancelCurrentOperation();

	UFUNCTION(BlueprintCallable, Category="Listen Server Network")
	bool HostDefaultSession();

	UFUNCTION(BlueprintCallable, Category="Listen Server Network")
	bool FindDefaultSessions();

	UFUNCTION(BlueprintCallable, Category="Listen Server Network")
	bool QuickMatchDefault();

	UFUNCTION(BlueprintCallable, Category="Listen Server Network")
	bool LeaveAndReturnToMenu();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Listen Server Network")
	bool HostTravelToDefaultGameMap();

	UFUNCTION(BlueprintPure, Category="Listen Server Network")
	EListenServerRole GetCurrentRole() const;

	UFUNCTION(BlueprintPure, Category="Listen Server Network")
	EListenServerConnectionState GetConnectionState() const;

	UFUNCTION(BlueprintPure, Category="Listen Server Network")
	EListenServerOperation GetCurrentOperation() const;

	UFUNCTION(BlueprintPure, Category="Listen Server Network")
	FListenServerOperationResult GetLastOperationResult() const;

	UFUNCTION(BlueprintPure, Category="Listen Server Network")
	int32 GetSearchResultCount() const;

	UFUNCTION(BlueprintPure, Category="Listen Server Network")
	bool GetSearchResultByIndex(int32 Index, FListenServerSearchResult& OutResult) const;

	bool GetSearchResultByHandle(const FListenServerSearchResultHandle& Handle, FListenServerSearchResult& OutResult) const;

	UFUNCTION(BlueprintPure, Category="Listen Server Network")
	int32 GetParticipantCount() const;

	UFUNCTION(BlueprintPure, Category="Listen Server Network")
	bool GetParticipantByIndex(int32 Index, FListenServerParticipant& OutParticipant) const;

	UFUNCTION(BlueprintPure, Category="Listen Server Network")
	TArray<FListenServerParticipant> GetParticipants() const;

	UFUNCTION(BlueprintPure, Category="Listen Server Network")
	FListenServerDebugSnapshot GetDebugSnapshot() const;

	FListenServerConfigurationReport ValidateConfiguration() const;
	void LogConfigurationReport() const;
	TConstArrayView<FListenServerSearchResult> GetSearchResultsView() const;
	TConstArrayView<FListenServerParticipant> GetParticipantsView() const;

	UPROPERTY(BlueprintAssignable, Category="Listen Server Network")
	FListenServerStateChanged OnStateChanged;

	UPROPERTY(BlueprintAssignable, Category="Listen Server Network")
	FListenServerOperationCompleted OnOperationCompleted;

	UPROPERTY(BlueprintAssignable, Category="Listen Server Network")
	FListenServerSearchResultsChanged OnSearchResultsChanged;

	UPROPERTY(BlueprintAssignable, Category="Listen Server Network")
	FListenServerParticipantsChanged OnParticipantsChanged;

	UPROPERTY(BlueprintAssignable, Category="Listen Server Network")
	FListenServerNetworkFailure OnNetworkFailure;

private:
	FListenServerSessionSubsystemImpl* Impl = nullptr;

	friend struct FListenServerSessionSubsystemImpl;
};
