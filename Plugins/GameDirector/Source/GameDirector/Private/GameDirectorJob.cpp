#include "GameDirectorJob.h"

FGameDirectorJob::FGameDirectorJob()
    : ComponentId(NAME_None)
    , ScenarioJSON()
    , ResultJSON()
    , Priority(EPriority::Normal)
    , EnqueueTime(FDateTime::UtcNow())
{
}

FGameDirectorJob::FGameDirectorJob(FName InComponentId, const FString& InScenarioJSON, EPriority InPriority)
    : ComponentId(InComponentId)
    , ScenarioJSON(InScenarioJSON)
    , ResultJSON()
    , Priority(InPriority)
    , EnqueueTime(FDateTime::UtcNow())
{
}

bool FGameDirectorJobSorter::operator()(const TSharedPtr<FGameDirectorJob>& Lhs, const TSharedPtr<FGameDirectorJob>& Rhs) const
{
    if (!Lhs.IsValid() || !Rhs.IsValid())
    {
        return Lhs.IsValid();
    }

    if (Lhs->Priority != Rhs->Priority)
    {
        return static_cast<uint8>(Lhs->Priority) > static_cast<uint8>(Rhs->Priority);
    }

    return Lhs->EnqueueTime < Rhs->EnqueueTime;
}
