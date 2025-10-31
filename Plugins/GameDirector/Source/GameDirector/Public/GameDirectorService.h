// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameDirectorService.generated.h"

class UGameDirectorSubsystem;

DECLARE_LOG_CATEGORY_EXTERN(LogGameDirectorService, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDirectorEvaluated, const FString&, ScenarioJSON);

UCLASS()
class GAMEDIRECTOR_API UGameDirectorService : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // USubsystem interface
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

    // Tick interface
    virtual void Tick(float DeltaSeconds) override;
    virtual bool IsTickable() const override { return true; }
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UGameDirectorService, STATGROUP_Tickables); }

    UFUNCTION(BlueprintCallable, Category = "GameDirector")
    void EvaluateDifficulty();

    UFUNCTION(BlueprintCallable, Category = "GameDirector")
    FString BuildScenarioJSON() const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameDirector")
    float EvaluationInterval = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameDirector")
    bool bEnableAutoEvaluation = true;

    UPROPERTY(BlueprintAssignable, Category = "GameDirector")
    FOnDirectorEvaluated OnDirectorEvaluated;

private:
    void RefreshCachedDirector();

    float TimeSinceLastEval = 0.0f;
    TWeakObjectPtr<UGameDirectorSubsystem> CachedDirector;
};
