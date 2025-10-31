#include "GameDirectorTypes.h"

FString FAIDifficulty::ToString() const
{
    return FString::Printf(
        TEXT("AimSpreadLevel=%d AimSpreadFine=%.3f ReactionLevel=%d AggressionLevel=%d PeekLevel=%d Duration=%ds"),
        AimSpreadLevel,
        AimSpreadFine,
        ReactionLevel,
        AggressionLevel,
        PeekLevel,
        DurationS);
}

