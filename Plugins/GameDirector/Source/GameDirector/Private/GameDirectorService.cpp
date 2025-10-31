// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameDirectorService.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnemyCharacter.h"
#include "GameDirectorSubsystem.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY(LogGameDirectorService);

void UGameDirectorService::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    RefreshCachedDirector();
    TimeSinceLastEval = 0.0f;
}

void UGameDirectorService::Deinitialize()
{
    CachedDirector.Reset();
    TimeSinceLastEval = 0.0f;
    Super::Deinitialize();
}

void UGameDirectorService::Tick(float DeltaSeconds)
{
    if (!bEnableAutoEvaluation)
    {
        return;
    }

    TimeSinceLastEval += DeltaSeconds;

    if (TimeSinceLastEval >= EvaluationInterval)
    {
        EvaluateDifficulty();
        TimeSinceLastEval = 0.0f;
    }
}

void UGameDirectorService::EvaluateDifficulty()
{
    TimeSinceLastEval = 0.0f;

    if (!CachedDirector.IsValid())
    {
        RefreshCachedDirector();
    }

    const FString Scenario = BuildScenarioJSON();

    UE_LOG(LogGameDirectorService, Log, TEXT("[GameDirectorService] Scenario: %s"), *Scenario);

    if (UGameDirectorSubsystem* Director = CachedDirector.Get())
    {
        Director->RequestDifficultyUpdate(Scenario);
    }
    else
    {
        UE_LOG(LogGameDirectorService, Warning, TEXT("[GameDirectorService] GameDirectorSubsystem not available."));
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("GameDirectorService triggered difficulty update"));
    }

    OnDirectorEvaluated.Broadcast(Scenario);
}

FString UGameDirectorService::BuildScenarioJSON() const
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return TEXT("{}");
    }

    const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
    float PlayerHealth = 1.0f;

    if (PlayerPawn)
    {
        if (const FFloatProperty* HealthProperty = FindFProperty<FFloatProperty>(PlayerPawn->GetClass(), TEXT("Health")))
        {
            const void* PropertyPtr = HealthProperty->ContainerPtrToValuePtr<void>(PlayerPawn);
            PlayerHealth = HealthProperty->GetFloatingPointPropertyValue(PropertyPtr);
        }
    }

    int32 EnemyCount = 0;
    double DistanceAccumulator = 0.0;

    if (PlayerPawn)
    {
        const FVector PlayerLocation = PlayerPawn->GetActorLocation();

        for (TActorIterator<AEnemyCharacter> It(World); It; ++It)
        {
            const AEnemyCharacter* Enemy = *It;
            if (!IsValid(Enemy))
            {
                continue;
            }

            ++EnemyCount;
            DistanceAccumulator += FVector::Dist(PlayerLocation, Enemy->GetActorLocation());
        }
    }
    else
    {
        for (TActorIterator<AEnemyCharacter> It(World); It; ++It)
        {
            if (IsValid(*It))
            {
                ++EnemyCount;
            }
        }
    }

    const double AvgDistance = (EnemyCount > 0) ? DistanceAccumulator / static_cast<double>(EnemyCount) : 0.0;

    return FString::Printf(TEXT("{\"schema\":\"gda.fps.input.v1\",\"player\":{\"hp\":%.3f},\"world\":{\"enemy_count\":%d,\"avg_enemy_distance\":%.1f}}"),
        PlayerHealth,
        EnemyCount,
        AvgDistance);
}

void UGameDirectorService::RefreshCachedDirector()
{
    UGameDirectorSubsystem* Director = nullptr;

    if (const UWorld* World = GetWorld())
    {
        if (UGameInstance* GameInstance = World->GetGameInstance())
        {
            Director = GameInstance->GetSubsystem<UGameDirectorSubsystem>();
        }
    }

    CachedDirector = Director;
}
