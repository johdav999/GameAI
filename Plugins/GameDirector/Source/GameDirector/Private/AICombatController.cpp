#include "AICombatController.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "GameDirectorSubsystem.h"

AAICombatController::AAICombatController()
{

    //{"schema": "gda.fps.output.v1", "intent": "tune_difficulty", "reason": "Easing pressure due to fast player deaths.", "tool_calls": [{"name": "AdjustAIDifficulty", "args": {"aim_spread_level": 2, "aim_spread_fine": 0.05, "reaction_level": 1, "aggression_level": 1, "peek_level": 1, "duration_s": 60}}]}
    aggression_level= TEXT("AggressionLevel");
    reaction_level= TEXT("ReactionLevel");
    peek_level = TEXT("PeekLevel");
	aim_spread_fine = TEXT("AimSpreadFine");
	aim_spread_level = TEXT("AimSpreadLevel");
	duration_s = TEXT("DurationS");

    DefaultDifficulty.AimSpreadLevel = 2;
    DefaultDifficulty.AimSpreadFine = 0.0f;
    DefaultDifficulty.ReactionLevel =2;
    DefaultDifficulty.AggressionLevel = 2;
    DefaultDifficulty.PeekLevel =2;
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
        BlackboardComp->SetValueAsInt(aggression_level, (CachedDifficulty.AggressionLevel));
        BlackboardComp->SetValueAsInt(reaction_level, (CachedDifficulty.ReactionLevel));
        BlackboardComp->SetValueAsInt(peek_level, (CachedDifficulty.PeekLevel));
		BlackboardComp->SetValueAsFloat(aim_spread_fine, (CachedDifficulty.AimSpreadFine));
		BlackboardComp->SetValueAsInt(aim_spread_level, (CachedDifficulty.AimSpreadLevel));
		BlackboardComp->SetValueAsInt(duration_s, (CachedDifficulty.DurationS));

    }
}

void AAICombatController::OnSubsystemDifficultyChanged(const FAIDifficulty& InDiff)
{
    UE_LOG(LogGameDirector, Log, TEXT("[%s] Received difficulty update: %s"),
        *GetName(),
        *InDiff.ToString());
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

