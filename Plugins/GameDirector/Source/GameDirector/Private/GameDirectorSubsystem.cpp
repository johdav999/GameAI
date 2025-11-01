#include "GameDirectorSubsystem.h"

#include "GameDirectorJob.h"
#include "GameDirectorJobQueue.h"
#include "GameDirectorTypes.h"
#include "LlamaRunner.h"

#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/CondensedJsonPrintPolicy.h"

#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogGameDirector);

void UGameDirectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    BaselineDifficulty = FAIDifficulty();
    BaselineDifficulty.AimSpreadLevel = 1;
    BaselineDifficulty.AimSpreadFine = 0.0f;
    BaselineDifficulty.ReactionLevel = 1;
    BaselineDifficulty.AggressionLevel = 1;
    BaselineDifficulty.PeekLevel = 1;
    BaselineDifficulty.DurationS = 0;
    CurrentDifficulty = BaselineDifficulty;

    LlamaRunner = MakeShared<FLlamaRunner>();

    const FString ModelPath = ResolveModelPath();
    if (ModelPath.IsEmpty())
    {
        UE_LOG(LogGameDirector, Warning, TEXT("No GGUF model found under Content/AIModels/. GameDirector subsystem will be inactive."));
        LlamaRunner.Reset();
        OnDifficultyChanged.Broadcast(CurrentDifficulty);
        return;
    }

    if (!LlamaRunner->LoadModel(ModelPath))
    {
        UE_LOG(LogGameDirector, Error, TEXT("Failed to load llama model at %s"), *ModelPath);
        LlamaRunner.Reset();
        OnDifficultyChanged.Broadcast(CurrentDifficulty);
        return;
    }

    UE_LOG(LogGameDirector, Log, TEXT("Loaded llama model from %s"), *ModelPath);

    JobQueue = MakeShared<FGameDirectorJobQueue>(LlamaRunner, MaxConcurrentJobs);
    if (!JobQueueTickerHandle.IsValid())
    {
        JobQueueTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UGameDirectorSubsystem::PumpJobQueue));
    }

    OnDifficultyChanged.Broadcast(CurrentDifficulty);
}

void UGameDirectorSubsystem::Deinitialize()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RestoreTimerHandle);
    }

    if (JobQueueTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(JobQueueTickerHandle);
        JobQueueTickerHandle.Reset();
    }

    JobQueue.Reset();
    LlamaRunner.Reset();

    Super::Deinitialize();
}

void UGameDirectorSubsystem::RequestInference(FName ComponentId, const FString& ScenarioJSON, TFunction<void(const FString&)> OnResult)
{
    if (!LlamaRunner.IsValid())
    {
        UE_LOG(LogGameDirector, Warning, TEXT("RequestInference called but no llama model is loaded."));
        return;
    }

    if (!JobQueue.IsValid())
    {
        JobQueue = MakeShared<FGameDirectorJobQueue>(LlamaRunner, MaxConcurrentJobs);

        if (!JobQueueTickerHandle.IsValid())
        {
            JobQueueTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateUObject(this, &UGameDirectorSubsystem::PumpJobQueue));
        }
    }

    const TSharedPtr<FGameDirectorJob> Job = MakeShared<FGameDirectorJob>(ComponentId, ScenarioJSON, FGameDirectorJob::EPriority::Normal);
    Job->OnComplete = MoveTemp(OnResult);

    UE_LOG(LogGameDirector, Log, TEXT("[GameDirectorSubsystem] Queuing inference job for %s."), *ComponentId.ToString());

    JobQueue->EnqueueJob(Job);
}

void UGameDirectorSubsystem::RequestDifficultyUpdate(const FString& Scenario)
{
    const TWeakObjectPtr<UGameDirectorSubsystem> WeakThis(this);

    RequestInference(TEXT("Difficulty"), Scenario, [WeakThis](const FString& ResultJSON)
    {
        if (!WeakThis.IsValid())
        {
            return;
        }

        UGameDirectorSubsystem* StrongSubsystem = WeakThis.Get();
        if (!StrongSubsystem)
        {
            return;
        }

        if (ResultJSON.IsEmpty())
        {
            UE_LOG(LogGameDirector, Warning, TEXT("Difficulty inference returned an empty response."));
            return;
        }

        StrongSubsystem->HandleModelResponse(ResultJSON);
    });
}

bool UGameDirectorSubsystem::IsBusy() const
{
    return JobQueue.IsValid() && JobQueue->IsBusy();
}

void UGameDirectorSubsystem::HandleModelResponse(const FString& Response)
{
    TSharedPtr<FJsonObject> RootObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response);
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        UE_LOG(LogGameDirector, Error, TEXT("Failed to parse llama response as JSON: %s"), *Response);
        return;
    }

    FAIDifficulty ParsedDifficulty;
    FString Reason;
    if (!TryParseDifficulty(RootObject, ParsedDifficulty, Reason))
    {
        UE_LOG(LogGameDirector, Warning, TEXT("No valid AdjustAIDifficulty tool call found in response: %s"), *Response);
        return;
    }

    FString Intent;
    RootObject->TryGetStringField(TEXT("intent"), Intent);

    CurrentDifficulty = ParsedDifficulty;

    UE_LOG(LogGameDirector, Log, TEXT("AI difficulty adjusted (%s). Intent=%s Reason: %s"), *CurrentDifficulty.ToString(), *Intent, *Reason);
    OnDifficultyChanged.Broadcast(CurrentDifficulty);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RestoreTimerHandle);
        if (CurrentDifficulty.DurationS > 0)
        {
            World->GetTimerManager().SetTimer(RestoreTimerHandle, this, &UGameDirectorSubsystem::RestoreBaseline, CurrentDifficulty.DurationS, false);
        }
    }
}

bool UGameDirectorSubsystem::TryParseDifficulty(const TSharedPtr<FJsonObject>& RootObject, FAIDifficulty& OutDifficulty, FString& OutReason) const
{
    if (!RootObject.IsValid())
    {
        return false;
    }

    OutDifficulty = CurrentDifficulty;

    RootObject->TryGetStringField(TEXT("reason"), OutReason);
    if (OutReason.IsEmpty())
    {
        OutReason = TEXT("No reason provided");
    }

    const TArray<TSharedPtr<FJsonValue>>* ToolCalls = nullptr;
    if (!RootObject->TryGetArrayField(TEXT("tool_calls"), ToolCalls) || ToolCalls == nullptr)
    {
        return false;
    }

    for (const TSharedPtr<FJsonValue>& ToolValue : *ToolCalls)
    {
        const TSharedPtr<FJsonObject> ToolObject = ToolValue.IsValid() ? ToolValue->AsObject() : nullptr;
        if (!ToolObject.IsValid())
        {
            continue;
        }

        FString ToolName;
        if (!ToolObject->TryGetStringField(TEXT("name"), ToolName) || !ToolName.Equals(TEXT("AdjustAIDifficulty"), ESearchCase::IgnoreCase))
        {
            continue;
        }

        TSharedPtr<FJsonObject> ArgsObject;
        const TSharedPtr<FJsonObject>* ArgsPtr = nullptr;

        if (ToolObject->TryGetObjectField(TEXT("args"), ArgsPtr) && ArgsPtr && ArgsPtr->IsValid())
        {
            ArgsObject = *ArgsPtr;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[GameDirector] Missing or invalid args for %s"), *ToolName);
            continue;
        }

        double NumberValue = 0.0;

        if (ArgsObject->TryGetNumberField(TEXT("aim_spread_level"), NumberValue))
        {
            OutDifficulty.AimSpreadLevel = FMath::Clamp(static_cast<int32>(FMath::RoundToInt(NumberValue)), 0, 10);
        }

        if (ArgsObject->TryGetNumberField(TEXT("aim_spread_fine"), NumberValue))
        {
            OutDifficulty.AimSpreadFine = static_cast<float>(NumberValue);
        }

        if (ArgsObject->TryGetNumberField(TEXT("reaction_level"), NumberValue))
        {
            OutDifficulty.ReactionLevel = FMath::Clamp(static_cast<int32>(FMath::RoundToInt(NumberValue)), 0, 10);
        }

        if (ArgsObject->TryGetNumberField(TEXT("aggression_level"), NumberValue))
        {
            OutDifficulty.AggressionLevel = FMath::Clamp(static_cast<int32>(FMath::RoundToInt(NumberValue)), 0, 10);
        }

        if (ArgsObject->TryGetNumberField(TEXT("peek_level"), NumberValue))
        {
            OutDifficulty.PeekLevel = FMath::Clamp(static_cast<int32>(FMath::RoundToInt(NumberValue)), 0, 10);
        }

        if (ArgsObject->TryGetNumberField(TEXT("duration_s"), NumberValue))
        {
            OutDifficulty.DurationS = FMath::Max(0, static_cast<int32>(FMath::RoundToInt(NumberValue)));
        }

        return true;
    }

    return false;
}

void UGameDirectorSubsystem::RestoreBaseline()
{
    CurrentDifficulty = BaselineDifficulty;
    UE_LOG(LogGameDirector, Log, TEXT("Restoring baseline difficulty: %s"), *BaselineDifficulty.ToString());
    OnDifficultyChanged.Broadcast(CurrentDifficulty);
}

FString UGameDirectorSubsystem::ResolveModelPath() const
{
    const FString ModelsDirectory = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("AIModels"));

    if (!FPaths::DirectoryExists(ModelsDirectory))
    {
        return FString();
    }

    TArray<FString> FoundModels;
    IFileManager::Get().FindFiles(FoundModels, *(ModelsDirectory / TEXT("*.gguf")), true, false);

    if (FoundModels.Num() == 0)
    {
        return FString();
    }

    return FPaths::Combine(ModelsDirectory, FoundModels[0]);
}

bool UGameDirectorSubsystem::PumpJobQueue(float DeltaTime)
{
    if (JobQueue.IsValid())
    {
        JobQueue->Tick();
    }

    return true;
}
