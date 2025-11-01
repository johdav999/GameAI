#pragma once

#include "CoreMinimal.h"
#include "GameDirectorJob.h"

class FLlamaRunner;

/**
 * Threaded job queue that schedules inference work on the engine thread pool.
 */
class GAMEDIRECTOR_API FGameDirectorJobQueue : public TSharedFromThis<FGameDirectorJobQueue>
{
public:
    explicit FGameDirectorJobQueue(const TSharedPtr<FLlamaRunner>& InRunner, int32 InMaxConcurrentJobs = 2);

    /** Enqueues a new job for background execution. */
    void EnqueueJob(const TSharedPtr<FGameDirectorJob>& Job);

    /** Advances the queue state and dispatches completed jobs back to the game thread. */
    void Tick();

    /** Returns true while there is pending or running work. */
    bool IsBusy() const;

private:
    void TryStartJobs();
    void StartJob(const TSharedPtr<FGameDirectorJob>& Job);
    void CompleteJob(const TSharedPtr<FGameDirectorJob>& Job);
    bool CanStartJob() const;

private:
    TWeakPtr<FLlamaRunner> LlamaRunner;
    int32 MaxConcurrentJobs = 1;

    mutable FCriticalSection PendingMutex;
    TArray<TSharedPtr<FGameDirectorJob>> PendingJobs;

    mutable FCriticalSection CompletedMutex;
    TQueue<TSharedPtr<FGameDirectorJob>> CompletedJobs;

    mutable FCriticalSection ActiveMutex;
    int32 ActiveJobCount = 0;
};
