#include "GameDirectorJobQueue.h"

#include "Algo/Sort.h"
#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "LlamaRunner.h"

DEFINE_LOG_CATEGORY_STATIC(LogGameDirectorJobs, Log, All);

FGameDirectorJobQueue::FGameDirectorJobQueue(const TSharedPtr<FLlamaRunner>& InRunner, int32 InMaxConcurrentJobs)
    : LlamaRunner(InRunner)
    , MaxConcurrentJobs(FMath::Max(1, InMaxConcurrentJobs))
{
}

void FGameDirectorJobQueue::EnqueueJob(const TSharedPtr<FGameDirectorJob>& Job)
{
    if (!Job.IsValid())
    {
        UE_LOG(LogGameDirectorJobs, Warning, TEXT("[GameDirectorJobQueue] Attempted to enqueue invalid job."));
        return;
    }

    Job->EnqueueTime = FDateTime::UtcNow();

    {
        FScopeLock ScopeLock(&PendingMutex);
        PendingJobs.Add(Job);
        Algo::StableSort(PendingJobs, FGameDirectorJobSorter());
    }

    UE_LOG(LogGameDirectorJobs, Log, TEXT("[GameDirectorJobQueue] Enqueued job for component %s (Priority=%d)."),
        *Job->ComponentId.ToString(), static_cast<int32>(Job->Priority));

    TryStartJobs();
}

void FGameDirectorJobQueue::Tick()
{
    TryStartJobs();

    TArray<TSharedPtr<FGameDirectorJob>> JobsToDispatch;
    TSharedPtr<FGameDirectorJob> Job;
    while (CompletedJobs.Dequeue(Job))
    {
        JobsToDispatch.Add(Job);
    }

    for (const TSharedPtr<FGameDirectorJob>& Job : JobsToDispatch)
    {
        if (!Job.IsValid())
        {
            continue;
        }

        UE_LOG(LogGameDirectorJobs, Log, TEXT("[GameDirectorJobQueue] Dispatching completed job for %s."),
            *Job->ComponentId.ToString());

        AsyncTask(ENamedThreads::GameThread, [Job]()
        {
            if (Job->OnComplete)
            {
                Job->OnComplete(Job->ResultJSON);
            }
        });
    }
}

bool FGameDirectorJobQueue::IsBusy() const
{
    {
        FScopeLock ActiveLock(&ActiveMutex);
        if (ActiveJobCount > 0)
        {
            return true;
        }
    }

    FScopeLock PendingLock(&PendingMutex);
    return PendingJobs.Num() > 0;
}

void FGameDirectorJobQueue::TryStartJobs()
{
    while (CanStartJob())
    {
        TSharedPtr<FGameDirectorJob> NextJob;
        {
            FScopeLock ScopeLock(&PendingMutex);
            if (PendingJobs.Num() == 0)
            {
                break;
            }

            NextJob = PendingJobs[0];
            PendingJobs.RemoveAt(0);
        }

        if (!NextJob.IsValid())
        {
            continue;
        }

        StartJob(NextJob);
    }
}

bool FGameDirectorJobQueue::CanStartJob() const
{
    FScopeLock ScopeLock(&ActiveMutex);
    return ActiveJobCount < MaxConcurrentJobs;
}

void FGameDirectorJobQueue::StartJob(const TSharedPtr<FGameDirectorJob>& Job)
{
    if (!Job.IsValid())
    {
        return;
    }

    {
        FScopeLock ScopeLock(&ActiveMutex);
        ++ActiveJobCount;
    }

    UE_LOG(LogGameDirectorJobs, Log, TEXT("[GameDirectorJobQueue] Starting job for %s."), *Job->ComponentId.ToString());

    TSharedPtr<FGameDirectorJobQueue> ThisPtr = AsShared();

    Async(EAsyncExecution::ThreadPool, [ThisPtr, Job]()
    {
        const TSharedPtr<FLlamaRunner> Runner = ThisPtr->LlamaRunner.Pin();
        if (!Runner.IsValid())
        {
            UE_LOG(LogGameDirectorJobs, Warning,
                TEXT("[GameDirectorJobQueue] LlamaRunner invalid while processing %s."),
                *Job->ComponentId.ToString());
            ThisPtr->CompleteJob(Job);
            return;
        }

        Job->ResultJSON = Runner->RunInference(Job->ScenarioJSON);

        UE_LOG(LogGameDirectorJobs, Log, TEXT("[GameDirectorJobQueue] Job completed for %s."), *Job->ComponentId.ToString());

        ThisPtr->CompleteJob(Job);
    });
}

void FGameDirectorJobQueue::CompleteJob(const TSharedPtr<FGameDirectorJob>& Job)
{
    {
        FScopeLock ScopeLock(&ActiveMutex);
        ActiveJobCount = FMath::Max(0, ActiveJobCount - 1);
    }

    CompletedJobs.Enqueue(Job);
}
