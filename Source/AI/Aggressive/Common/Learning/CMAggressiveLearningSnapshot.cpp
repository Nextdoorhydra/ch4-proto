#include "Aggressive/Common/Learning/CMAggressiveLearningSnapshot.h"

#include "HAL/FileManager.h"
#include "LearningAgentsCritic.h"
#include "LearningAgentsNeuralNetwork.h"
#include "LearningAgentsPolicy.h"
#include "LearningNeuralNetwork.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogCMAggressiveLearningSnapshot, Log, All);

namespace
{
    struct FCMLearningSnapshotProfile
    {
        const TCHAR* RootRelativePath;
        const TCHAR* FilePrefix;
        const TCHAR* DisplayName;
        const TCHAR* BootstrapRelativePath;
    };

    const FCMLearningSnapshotProfile& GetSnapshotProfile(ECMAggressiveLearningSnapshotProfile Profile)
    {
        static const FCMLearningSnapshotProfile TetraProfile = {
            TEXT("LearningAgents/AggressiveAI/Type01_Acceleration/V2"),
            TEXT("Type01"),
            TEXT("Tetra"),
            TEXT("")
        };
        static const FCMLearningSnapshotProfile RipperProfile = {
            TEXT("LearningAgents/AggressiveAI/Type02_ThreeBodyThreeLeg/V6"),
            TEXT("Type02"),
            TEXT("Ripper"),
            TEXT("LearningAgents/AggressiveAI/Type02_ThreeBodyThreeLeg/V5/TrainingLatest")
        };
        static const FCMLearningSnapshotProfile CentipedeProfile = {
            TEXT("LearningAgents/AggressiveAI/Type03_FourSegmentEightLeg/V1"),
            TEXT("Type03"),
            TEXT("Centipede"),
            TEXT("")
        };

        switch (Profile)
        {
        case ECMAggressiveLearningSnapshotProfile::Tetra:
            return TetraProfile;
        case ECMAggressiveLearningSnapshotProfile::Ripper:
            return RipperProfile;
        case ECMAggressiveLearningSnapshotProfile::Centipede:
        default:
            return CentipedeProfile;
        }
    }

    FString GetNetworkFilePath(ECMAggressiveLearningSnapshotProfile Profile, const FString& Directory, const TCHAR* NetworkName)
    {
        return FPaths::Combine(Directory, FString::Printf(TEXT("%s%s.bin"), GetSnapshotProfile(Profile).FilePrefix, NetworkName));
    }

    FString GetPolicyFilePath(ECMAggressiveLearningSnapshotProfile Profile, const FString& Directory)
    {
        return GetNetworkFilePath(Profile, Directory, TEXT("Policy"));
    }

    FString GetEncoderFilePath(ECMAggressiveLearningSnapshotProfile Profile, const FString& Directory)
    {
        return GetNetworkFilePath(Profile, Directory, TEXT("Encoder"));
    }

    FString GetDecoderFilePath(ECMAggressiveLearningSnapshotProfile Profile, const FString& Directory)
    {
        return GetNetworkFilePath(Profile, Directory, TEXT("Decoder"));
    }

    FString GetCriticFilePath(ECMAggressiveLearningSnapshotProfile Profile, const FString& Directory)
    {
        return GetNetworkFilePath(Profile, Directory, TEXT("Critic"));
    }

    bool HasSnapshotFile(const FString& FilePath)
    {
        return IFileManager::Get().FileSize(*FilePath) > 0;
    }

    bool SaveNetworkSnapshot(ULearningAgentsNeuralNetwork* Network, const FString& FilePath)
    {
        if (!Network)
            return false;

        FFilePath SnapshotFile;
        SnapshotFile.FilePath = FilePath;
        Network->SaveNetworkToSnapshot(SnapshotFile);
        return HasSnapshotFile(FilePath);
    }

    bool LoadCompatibleNetworkSnapshot(ULearningAgentsNeuralNetwork* Network, const FString& FilePath)
    {
        if (!Network || !Network->NeuralNetworkData)
            return false;

        TArray<uint8> SnapshotBytes;
        if (!FFileHelper::LoadFileToArray(SnapshotBytes, *FilePath))
            return false;

        ULearningNeuralNetworkData* SnapshotData = NewObject<ULearningNeuralNetworkData>(GetTransientPackage());
        if (!SnapshotData || !SnapshotData->LoadFromSnapshot(SnapshotBytes))
            return false;

        const ULearningNeuralNetworkData* CurrentData = Network->NeuralNetworkData;
        if (SnapshotData->GetInputSize() != CurrentData->GetInputSize()
            || SnapshotData->GetOutputSize() != CurrentData->GetOutputSize()
            || SnapshotData->GetCompatibilityHash() != CurrentData->GetCompatibilityHash())
        {
            return false;
        }

        FFilePath SnapshotFile;
        SnapshotFile.FilePath = FilePath;
        Network->LoadNetworkFromSnapshot(SnapshotFile);
        return Network->NeuralNetworkData->GetContentHash() == SnapshotData->GetContentHash();
    }
}

FString CMAggressiveLearningSnapshot::GetRootDirectory(ECMAggressiveLearningSnapshotProfile Profile)
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), GetSnapshotProfile(Profile).RootRelativePath));
}

FString CMAggressiveLearningSnapshot::GetLatestDirectory(ECMAggressiveLearningSnapshotProfile Profile)
{
    return FPaths::Combine(GetRootDirectory(Profile), TEXT("TrainingLatest"));
}

FString CMAggressiveLearningSnapshot::GetBootstrapDirectory(ECMAggressiveLearningSnapshotProfile Profile)
{
    const TCHAR* RelativePath = GetSnapshotProfile(Profile).BootstrapRelativePath;
    return RelativePath[0] == TEXT('\0')
        ? FString()
        : FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), RelativePath));
}

FString CMAggressiveLearningSnapshot::GetCheckpointRootDirectory(ECMAggressiveLearningSnapshotProfile Profile)
{
    return FPaths::Combine(GetRootDirectory(Profile), TEXT("Checkpoints"));
}

FString CMAggressiveLearningSnapshot::GetCheckpointDirectory(ECMAggressiveLearningSnapshotProfile Profile, int64 TotalDecisionCount)
{
    return FPaths::Combine(GetCheckpointRootDirectory(Profile), FString::Printf(TEXT("Decision_%09lld"), FMath::Max(TotalDecisionCount, static_cast<int64>(0))));
}

bool CMAggressiveLearningSnapshot::SaveTrainingNetworks(ECMAggressiveLearningSnapshotProfile Profile, ULearningAgentsPolicy& Policy, ULearningAgentsCritic& Critic, const FString& Directory)
{
    if (!IFileManager::Get().MakeDirectory(*Directory, true))
        return false;

    const bool bPolicySaved = SaveNetworkSnapshot(Policy.GetPolicyNetworkAsset(), GetPolicyFilePath(Profile, Directory));
    const bool bEncoderSaved = SaveNetworkSnapshot(Policy.GetEncoderNetworkAsset(), GetEncoderFilePath(Profile, Directory));
    const bool bDecoderSaved = SaveNetworkSnapshot(Policy.GetDecoderNetworkAsset(), GetDecoderFilePath(Profile, Directory));
    const bool bCriticSaved = SaveNetworkSnapshot(Critic.GetCriticNetworkAsset(), GetCriticFilePath(Profile, Directory));
    return bPolicySaved && bEncoderSaved && bDecoderSaved && bCriticSaved;
}

bool CMAggressiveLearningSnapshot::HasAnyTrainingSnapshots(ECMAggressiveLearningSnapshotProfile Profile, const FString& Directory)
{
    return HasSnapshotFile(GetPolicyFilePath(Profile, Directory))
        || HasSnapshotFile(GetEncoderFilePath(Profile, Directory))
        || HasSnapshotFile(GetDecoderFilePath(Profile, Directory))
        || HasSnapshotFile(GetCriticFilePath(Profile, Directory));
}

bool CMAggressiveLearningSnapshot::HasCompleteTrainingSnapshots(ECMAggressiveLearningSnapshotProfile Profile, const FString& Directory)
{
    return HasSnapshotFile(GetPolicyFilePath(Profile, Directory))
        && HasSnapshotFile(GetEncoderFilePath(Profile, Directory))
        && HasSnapshotFile(GetDecoderFilePath(Profile, Directory))
        && HasSnapshotFile(GetCriticFilePath(Profile, Directory));
}

bool CMAggressiveLearningSnapshot::LoadTrainingNetworks(ECMAggressiveLearningSnapshotProfile Profile, ULearningAgentsPolicy& Policy, ULearningAgentsCritic& Critic, const FString& Directory)
{
    const bool bEncoderLoaded = LoadCompatibleNetworkSnapshot(Policy.GetEncoderNetworkAsset(), GetEncoderFilePath(Profile, Directory));
    const bool bPolicyLoaded = LoadCompatibleNetworkSnapshot(Policy.GetPolicyNetworkAsset(), GetPolicyFilePath(Profile, Directory));
    const bool bDecoderLoaded = LoadCompatibleNetworkSnapshot(Policy.GetDecoderNetworkAsset(), GetDecoderFilePath(Profile, Directory));
    const bool bCriticLoaded = LoadCompatibleNetworkSnapshot(Critic.GetCriticNetworkAsset(), GetCriticFilePath(Profile, Directory));
    if (!bEncoderLoaded || !bPolicyLoaded || !bDecoderLoaded || !bCriticLoaded)
        return false;

    UE_LOG(LogCMAggressiveLearningSnapshot, Display, TEXT("%s AI 학습 네트워크 네 개를 불러왔습니다: %s"), GetSnapshotProfile(Profile).DisplayName, *Directory);
    return true;
}

bool CMAggressiveLearningSnapshot::LoadInferenceNetworks(ECMAggressiveLearningSnapshotProfile Profile, ULearningAgentsPolicy& Policy)
{
    const FString Directory = GetLatestDirectory(Profile);
    const bool bEncoderLoaded = LoadCompatibleNetworkSnapshot(Policy.GetEncoderNetworkAsset(), GetEncoderFilePath(Profile, Directory));
    const bool bPolicyLoaded = LoadCompatibleNetworkSnapshot(Policy.GetPolicyNetworkAsset(), GetPolicyFilePath(Profile, Directory));
    const bool bDecoderLoaded = LoadCompatibleNetworkSnapshot(Policy.GetDecoderNetworkAsset(), GetDecoderFilePath(Profile, Directory));
    if (!bEncoderLoaded || !bPolicyLoaded || !bDecoderLoaded)
        return false;

    UE_LOG(LogCMAggressiveLearningSnapshot, Display, TEXT("%s AI 추론 네트워크 세 개를 불러왔습니다: %s"), GetSnapshotProfile(Profile).DisplayName, *Directory);
    return true;
}
