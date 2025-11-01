#pragma once

#include "CoreMinimal.h"

/** Lightweight description of a single inference request handled by the job queue. */
class GAMEDIRECTOR_API FGameDirectorJob
{
public:
    enum class EPriority : uint8
    {
        Low,
        Normal,
        High
    };

    FGameDirectorJob();

    FGameDirectorJob(FName InComponentId, const FString& InScenarioJSON, EPriority InPriority = EPriority::Normal);

    /** Identifier for the component requesting inference (e.g. "Difficulty"). */
    FName ComponentId;

    /** Scenario payload that will be sent to the llama runner. */
    FString ScenarioJSON;

    /** Result payload produced by the inference worker. */
    FString ResultJSON;

    /** Priority of the job; higher priority jobs are started first when the queue is busy. */
    EPriority Priority = EPriority::Normal;

    /** Timestamp when the job was enqueued. */
    FDateTime EnqueueTime;

    /** Callback invoked on the game thread once the job completes. */
    TFunction<void(const FString&)> OnComplete;
};

/** Utility to sort jobs by priority (higher first) then by enqueue time. */
struct FGameDirectorJobSorter
{
    bool operator()(const TSharedPtr<FGameDirectorJob>& Lhs, const TSharedPtr<FGameDirectorJob>& Rhs) const;
};
