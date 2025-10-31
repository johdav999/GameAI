#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameDirectorTypes.h"

#include "AICombatController.generated.h"

class UGameDirectorSubsystem;
class UBlackboardComponent;

/**
 * Base AI controller that subscribes to the GameDirector difficulty feed and pushes values to the blackboard.
 */
UCLASS()
class GAMEDIRECTOR_API AAICombatController : public AAIController
{
    GENERATED_BODY()

public:
    AAICombatController();

    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Apply difficulty adjustments provided by the subsystem. */
    UFUNCTION(BlueprintCallable, Category = "GameDirector|AI")
    virtual void AdjustDifficulty(const FAIDifficulty& InDiff);

    /** Restore the default baseline configuration. */
    UFUNCTION(BlueprintCallable, Category = "GameDirector|AI")
    virtual void ResetDifficulty();

protected:
    /** Pushes the cached difficulty to the blackboard for consumption by behavior tree services. */
    virtual void PushToBlackboard();

    /** Handles broadcast from the subsystem. */
    UFUNCTION()
    void OnSubsystemDifficultyChanged(const FAIDifficulty& InDiff);

    void RegisterWithSubsystem();
    void UnregisterFromSubsystem();

protected:
    //{"schema": "gda.fps.output.v1", "intent": "tune_difficulty", "reason": "Easing pressure due to fast player deaths.", "tool_calls": [{"name": "AdjustAIDifficulty", "args": {"aim_spread_level": 2, "aim_spread_fine": 0.05, "reaction_level": 1, "aggression_level": 1, "peek_level": 1, "duration_s": 60}}]}
    UPROPERTY(EditDefaultsOnly, Category = "GameDirector|AI|Blackboard")
    FName peek_level;

    UPROPERTY(EditDefaultsOnly, Category = "GameDirector|AI|Blackboard")
    FName reaction_level;

    UPROPERTY(EditDefaultsOnly, Category = "GameDirector|AI|Blackboard")
    FName aim_spread_fine;

    UPROPERTY(EditDefaultsOnly, Category = "GameDirector|AI|Blackboard")
    FName aim_spread_level;

    UPROPERTY(EditDefaultsOnly, Category = "GameDirector|AI|Blackboard")
    FName duration_s;

    UPROPERTY(EditDefaultsOnly, Category = "GameDirector|AI|Blackboard")
    FName aggression_level;


    /** Default values that will be restored whenever timers expire. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameDirector|AI")
    FAIDifficulty DefaultDifficulty;

    /** Current values mirrored in the blackboard. */
    UPROPERTY(VisibleInstanceOnly, Category = "GameDirector|AI")
    FAIDifficulty CachedDifficulty;

private:
    TWeakObjectPtr<UGameDirectorSubsystem> CachedSubsystem;
};

/** Lightweight variant for grunt enemies. */
UCLASS()
class GAMEDIRECTOR_API AAIEnemyController : public AAICombatController
{
    GENERATED_BODY()

public:
    AAIEnemyController();
};

/** Specialized controller for bosses with more aggressive defaults. */
UCLASS()
class GAMEDIRECTOR_API ABossAIController : public AAICombatController
{
    GENERATED_BODY()

public:
    ABossAIController();
};

