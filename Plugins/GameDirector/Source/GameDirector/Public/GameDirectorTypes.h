#pragma once

#include "CoreMinimal.h"

#include "GameDirectorTypes.generated.h"

/**
 * Struct describing an AI difficulty configuration emitted by the llama policy.
 */
USTRUCT(BlueprintType)
struct GAMEDIRECTOR_API FAIDifficulty
{
    GENERATED_BODY()

    FAIDifficulty()
        : AimSpreadLevel(0)
        , AimSpreadFine(0.f)
        , ReactionLevel(0)
        , AggressionLevel(0)
        , PeekLevel(0)
        , DurationS(0)
    {
    }

    /** Coarse aim spread level provided by the AI assistant. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameDirector|Difficulty")
    int32 AimSpreadLevel;

    /** Fine grained aim spread delta applied on top of AimSpreadLevel. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameDirector|Difficulty")
    float AimSpreadFine;

    /** Reaction time bucket assigned to the enemy AI. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameDirector|Difficulty")
    int32 ReactionLevel;

    /** Aggression level used by behavior tree selectors. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameDirector|Difficulty")
    int32 AggressionLevel;

    /** Peek or corner checking behavior level. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameDirector|Difficulty")
    int32 PeekLevel;

    /** Duration in seconds before the subsystem restores baseline difficulty. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameDirector|Difficulty")
    int32 DurationS;

    /** Creates a human readable representation suitable for logging. */
    FString ToString() const;
};

