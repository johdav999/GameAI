#include "AICombatController.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "GameDirectorSubsystem.h"

AAICombatController::AAICombatController()
{
    AggressionKeyName = TEXT("AggressionLevel");
    ReactionKeyName = TEXT("ReactionLevel");
    PeekKeyName = TEXT("PeekLevel");

    DefaultDifficulty.AimSpreadLevel = 1;
    DefaultDifficulty.AimSpreadFine = 0.0f;
    DefaultDifficulty.ReactionLevel = 1;
    DefaultDifficulty.AggressionLevel = 1;
    DefaultDifficulty.PeekLevel = 1;
    DefaultDifficulty.DurationS = 0;

    CachedDifficulty = DefaultDifficulty;
}

void AAICombatController::BeginPlay()
{
    Super::BeginPlay();
    RegisterWithSubsystem();
    PushToBlackboard();
}

void AAICombatController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    CachedDifficulty = DefaultDifficulty;
    PushToBlackboard();
    RegisterWithSubsystem();
}

void AAICombatController::OnUnPossess()
{
    ResetDifficulty();
    Super::OnUnPossess();
}

void AAICombatController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UnregisterFromSubsystem();
    Super::EndPlay(EndPlayReason);
}

void AAICombatController::AdjustDifficulty(const FAIDifficulty& InDiff)
{
    CachedDifficulty = InDiff;
    UE_LOG(LogGameDirector, Verbose, TEXT("%s applying AI difficulty: %s"), *GetName(), *CachedDifficulty.ToString());
    PushToBlackboard();
}

void AAICombatController::ResetDifficulty()
{
    CachedDifficulty = DefaultDifficulty;
    UE_LOG(LogGameDirector, Verbose, TEXT("%s resetting AI difficulty to defaults: %s"), *GetName(), *CachedDifficulty.ToString());
    PushToBlackboard();
}

void AAICombatController::PushToBlackboard()
{
    if (UBlackboardComponent* BlackboardComp = GetBlackboardComponent())
    {
        BlackboardComp->SetValueAsFloat(AggressionKeyName, static_cast<float>(CachedDifficulty.AggressionLevel));
        BlackboardComp->SetValueAsFloat(ReactionKeyName, static_cast<float>(CachedDifficulty.ReactionLevel));
        BlackboardComp->SetValueAsFloat(PeekKeyName, static_cast<float>(CachedDifficulty.PeekLevel));
    }
}

void AAICombatController::OnSubsystemDifficultyChanged(const FAIDifficulty& InDiff)
{
    AdjustDifficulty(InDiff);
}

void AAICombatController::RegisterWithSubsystem()
{
    if (CachedSubsystem.IsValid())
    {
        return;
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UGameDirectorSubsystem* Subsystem = GameInstance->GetSubsystem<UGameDirectorSubsystem>())
        {
            Subsystem->OnDifficultyChanged.AddDynamic(this, &AAICombatController::OnSubsystemDifficultyChanged);
            CachedSubsystem = Subsystem;
            AdjustDifficulty(Subsystem->GetCurrentDifficulty());
        }
    }
}

void AAICombatController::UnregisterFromSubsystem()
{
    if (UGameDirectorSubsystem* Subsystem = CachedSubsystem.Get())
    {
        Subsystem->OnDifficultyChanged.RemoveDynamic(this, &AAICombatController::OnSubsystemDifficultyChanged);
    }

    CachedSubsystem.Reset();
}

AAIEnemyController::AAIEnemyController()
{
    DefaultDifficulty.AimSpreadLevel = 1;
    DefaultDifficulty.AimSpreadFine = 0.02f;
    DefaultDifficulty.ReactionLevel = 1;
    DefaultDifficulty.AggressionLevel = 2;
    DefaultDifficulty.PeekLevel = 2;
    DefaultDifficulty.DurationS = 0;

    CachedDifficulty = DefaultDifficulty;
}

ABossAIController::ABossAIController()
{
    DefaultDifficulty.AimSpreadLevel = 3;
    DefaultDifficulty.AimSpreadFine = -0.05f;
    DefaultDifficulty.ReactionLevel = 3;
    DefaultDifficulty.AggressionLevel = 4;
    DefaultDifficulty.PeekLevel = 3;
    DefaultDifficulty.DurationS = 0;

    CachedDifficulty = DefaultDifficulty;
}

