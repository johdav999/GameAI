#pragma once

#include "CoreMinimal.h"
#include "GameDirectorTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"

#include "GameDirectorSubsystem.generated.h"

class FLlamaRunner;
class FJsonObject;

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
     * Triggers a difficulty update for the supplied scenario description.
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

private:
    void HandleModelResponse(const FString& Response); 
    bool TryParseDifficulty(const TSharedPtr<FJsonObject>& RootObject, FAIDifficulty& OutDifficulty, FString& OutReason) const;
    void RestoreBaseline();
    FString ResolveModelPath() const;

private:
    TSharedPtr<FLlamaRunner> LlamaRunner;
    FAIDifficulty BaselineDifficulty;
    FAIDifficulty CurrentDifficulty;
    FTimerHandle RestoreTimerHandle;
};

