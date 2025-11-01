#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "GameDirectorService.generated.h"

class UGameDirectorSubsystem;

DECLARE_LOG_CATEGORY_EXTERN(LogGameDirectorService, Log, All);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDirectorEvaluated, const FString&, ScenarioJSON);

UCLASS()
class GAMEDIRECTOR_API UGameDirectorService : public UWorldSubsystem, public FTickableGameObject
{
    GENERATED_BODY()

public:
    // --- Subsystem interface ---
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    void OnPostWorldInit(UWorld* World, const UWorld::InitializationValues IVS);
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

    // --- Tickable interface ---
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override { return true; }
    virtual TStatId GetStatId() const override
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(UGameDirectorService, STATGROUP_Tickables);
    }

    // --- Director logic ---
    UFUNCTION(BlueprintCallable, Category = "GameDirector")
    void EvaluateDifficulty();

    UFUNCTION(BlueprintCallable, Category = "GameDirector")
    FString BuildScenarioJSON() const;

    // --- Settings ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameDirector")
    float EvaluationInterval = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameDirector")
    bool bEnableAutoEvaluation = true;

    // --- Delegate ---
    UPROPERTY(BlueprintAssignable, Category = "GameDirector")
    FOnDirectorEvaluated OnDirectorEvaluated;

private:
    void RefreshCachedDirector();

    UWorld* GetWorldSafe() const;

    float TimeSinceLastEval = 0.0f;
    TWeakObjectPtr<UGameDirectorSubsystem> CachedDirector;
};
