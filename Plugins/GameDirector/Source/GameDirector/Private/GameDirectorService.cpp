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

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogGameDirectorService, Verbose,
            TEXT("[GameDirectorService] Skipping initialization (no world)."));
        return;
    }

    // Skip editor / preview / transient worlds
    if (!World->IsGameWorld() || World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE)
    {
        UE_LOG(LogGameDirectorService, Verbose,
            TEXT("[GameDirectorService] Skipping non-game world: %s (Type=%d)"),
            *World->GetName(), static_cast<int32>(World->WorldType));
        return;
    }

    FWorldDelegates::OnPostWorldInitialization.AddUObject(this,
        &UGameDirectorService::OnPostWorldInit);

    UE_LOG(LogGameDirectorService, Log,
        TEXT("[GameDirectorService] Initialized for game world: %s"), *World->GetName());

    RefreshCachedDirector();
    TimeSinceLastEval = 0.0f;
}

void UGameDirectorService::OnPostWorldInit(UWorld* World, const UWorld::InitializationValues IVS)
{
    if (World && World->IsGameWorld())
    {
        RefreshCachedDirector();
        UE_LOG(LogGameDirectorService, Log, TEXT("[GameDirectorService] Connected after world init: %s"), *World->GetName());
    }
}

void UGameDirectorService::Deinitialize()
{
    FWorldDelegates::OnPostWorldInitialization.RemoveAll(this);

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
    //--- World validation -----------------------------------------------------
    UWorld* World = GetWorld();
    if (!World || !World->IsGameWorld())
    {
        UE_LOG(LogGameDirectorService, Verbose,
            TEXT("[GameDirectorService] Skipping EvaluateDifficulty for non-game world: %s"),
            *GetNameSafe(World));
        return;
    }

    //--- Skip if auto-eval disabled -------------------------------------------
    if (!bEnableAutoEvaluation)
    {
        UE_LOG(LogGameDirectorService, Verbose,
            TEXT("[GameDirectorService] Auto-evaluation disabled, skipping difficulty check."));
        return;
    }

    //--- Timer reset ----------------------------------------------------------
    TimeSinceLastEval = 0.0f;

    //--- Ensure subsystem pointer is valid ------------------------------------
    if (!CachedDirector.IsValid())
    {
        UE_LOG(LogGameDirectorService, Warning,
            TEXT("[GameDirectorService] CachedDirector invalid. World=%s GameInstance=%s"),
            *GetNameSafe(World),
            *GetNameSafe(World ? World->GetGameInstance() : nullptr));

        RefreshCachedDirector();
    }

    UGameDirectorSubsystem* Director = CachedDirector.Get();
    if (!Director)
    {
        // Try one last time in case world changed since last eval
        RefreshCachedDirector();
        Director = CachedDirector.Get();
    }

    //--- Build input JSON -----------------------------------------------------
    const FString Scenario = BuildScenarioJSON();
    UE_LOG(LogGameDirectorService, Log, TEXT("[GameDirectorService] Scenario: %s"), *Scenario);

    //--- Perform difficulty evaluation ----------------------------------------
    if (Director)
    {
        const TWeakObjectPtr<UGameDirectorService> WeakThis(this);

        Director->RequestInference(TEXT("Difficulty"), Scenario,
            [WeakThis](const FString& ResultJSON)
            {
                if (!WeakThis.IsValid())
                {
                    return;
                }

                if (UGameDirectorService* StrongService = WeakThis.Get())
                {
                    StrongService->OnDirectorEvaluated.Broadcast(ResultJSON);
                    UE_LOG(LogGameDirectorService, Log,
                        TEXT("[GameDirectorService] Inference completed: %s"), *ResultJSON);
                }
            });

        UE_LOG(LogGameDirectorService, Log, TEXT("[GameDirectorService] Sent difficulty request to %s"),
            *Director->GetName());
    }
    else
    {
        UE_LOG(LogGameDirectorService, Warning, TEXT("[GameDirectorService] GameDirectorSubsystem unavailable."));
    }

    //--- Visual feedback in PIE ----------------------------------------------
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            (uint64)this, 2.0f, Director ? FColor::Green : FColor::Red,
            Director ? TEXT("✅ Difficulty request queued") : TEXT("⚠️ Subsystem unavailable"));
    }
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
    CachedDirector.Reset();

    if (UWorld* World = GetWorldSafe())
    {
        if (UGameInstance* GameInstance = World->GetGameInstance())
        {
            if (UGameDirectorSubsystem* Director = GameInstance->GetSubsystem<UGameDirectorSubsystem>())
            {
                CachedDirector = Director;
                UE_LOG(LogGameDirectorService, Log,
                    TEXT("[GameDirectorService] (Re)connected to GameDirectorSubsystem in world %s"),
                    *World->GetName());
            }
            else
            {
                UE_LOG(LogGameDirectorService, Verbose,
                    TEXT("[GameDirectorService] GameDirectorSubsystem not yet available in world %s"),
                    *World->GetName());
            }
        }
        else
        {
            UE_LOG(LogGameDirectorService, Verbose,
                TEXT("[GameDirectorService] GameInstance still null for world %s"), *World->GetName());
        }
    }
}

UWorld* UGameDirectorService::GetWorldSafe() const
{
    if (const UWorld* SubsystemWorld = GetWorld())
    {
        return const_cast<UWorld*>(SubsystemWorld);
    }

    if (UObject* Outer = GetOuter())
    {
        if (UWorld* OuterWorld = Cast<UWorld>(Outer))
        {
            return OuterWorld;
        }
    }

    return nullptr;
}
