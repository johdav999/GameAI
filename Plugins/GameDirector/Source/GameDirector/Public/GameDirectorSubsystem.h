#pragma once

#include "CoreMinimal.h"
#include "GameDirectorTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"

#include "GameDirectorSubsystem.generated.h"

class FLlamaRunner;
class FJsonObject;
class FGameDirectorJobQueue;

DECLARE_LOG_CATEGORY_EXTERN(LogGameDirector, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDifficultyChanged, const FAIDifficulty&, Difficulty);

/**
 * GameInstance subsystem that bridges gameplay telemetry with the local llama.cpp runner.
 */
UCLASS()
class GAMEDIRECTOR_API UGameDirectorSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /**
     * Submits a generic inference job and invokes the provided callback on completion (game thread).
     */
    void RequestInference(FName ComponentId, const FString& ScenarioJSON, TFunction<void(const FString&)> OnResult);

    /**
     * Convenience helper that performs a difficulty update request and applies the returned configuration.
     */
    UFUNCTION(BlueprintCallable, Category = "GameDirector|AI")
    void RequestDifficultyUpdate(const FString& Scenario);

    /** Broadcast whenever the model selects a new difficulty configuration. */
    UPROPERTY(BlueprintAssignable, Category = "GameDirector|AI")
    FOnDifficultyChanged OnDifficultyChanged;

    /** Returns the baseline configuration used when timers expire. */
    const FAIDifficulty& GetBaselineDifficulty() const { return BaselineDifficulty; }

    /** Returns the latest configuration pushed to listeners. */
    const FAIDifficulty& GetCurrentDifficulty() const { return CurrentDifficulty; }

    /** True if the subsystem currently has work in flight. */
    bool IsBusy() const;

    /** Maximum number of concurrent inference jobs allowed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameDirector|AI")
    int32 MaxConcurrentJobs = 2;

private:
    void HandleModelResponse(const FString& Response);
    bool TryParseDifficulty(const TSharedPtr<FJsonObject>& RootObject, FAIDifficulty& OutDifficulty, FString& OutReason) const;
    void RestoreBaseline();
    FString ResolveModelPath() const;
    bool PumpJobQueue(float DeltaTime);

private:
    TSharedPtr<FLlamaRunner> LlamaRunner;
    TSharedPtr<FGameDirectorJobQueue> JobQueue;
    FTSTicker::FDelegateHandle JobQueueTickerHandle;

    FAIDifficulty BaselineDifficulty;
    FAIDifficulty CurrentDifficulty;
    FTimerHandle RestoreTimerHandle;
};
