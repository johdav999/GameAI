#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameDirectorTypes.h"

#include "EnemyCharacter.generated.h"

/**
 * Character pawn that reacts to GameDirector difficulty updates and simulates
 * cover peeking and burst firing behaviour.
 */
UCLASS(BlueprintType, Blueprintable)
class GAMEDIRECTOR_API AEnemyCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AEnemyCharacter();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Starts the burst firing behaviour after the configured reaction delay. */
    UFUNCTION(BlueprintCallable, Category = "GameDirector|Combat")
    void StartShooting();

    /** Stops any active burst firing behaviour and clears timers. */
    UFUNCTION(BlueprintCallable, Category = "GameDirector|Combat")
    void StopShooting();

    /** Fires a single simulated shot using the currently configured aim spread. */
    UFUNCTION(BlueprintCallable, Category = "GameDirector|Combat")
    void FireOneShot();

    /** Applies the provided difficulty settings to the character. */
    UFUNCTION(BlueprintCallable, Category = "GameDirector|Difficulty")
    void ApplyDifficulty(const FAIDifficulty& Diff);

    /** Toggles between peeking and hiding behaviour. */
    UFUNCTION(BlueprintCallable, Category = "GameDirector|Cover")
    void TogglePeek();

    /** Called when entering cover (hiding). */
    UFUNCTION(BlueprintCallable, Category = "GameDirector|Cover")
    void EnterCover();

    /** Called when exiting cover (peeking out). */
    UFUNCTION(BlueprintCallable, Category = "GameDirector|Cover")
    void ExitCover();

protected:
    /** Updates the peek timer with the latest interval values. */
    void RefreshPeekTimer();

    /** Configured reaction delay before firing begins. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameDirector|Difficulty", meta = (AllowPrivateAccess = "true"))
    float ReactionDelay;

    /** Configured aim spread measured in degrees. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameDirector|Difficulty", meta = (AllowPrivateAccess = "true"))
    float AimSpread;

    /** Configured shots per second. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameDirector|Difficulty", meta = (AllowPrivateAccess = "true"))
    float FireRate;

    /** Configured number of shots per burst. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameDirector|Difficulty", meta = (AllowPrivateAccess = "true"))
    int32 BurstCount;

    /** Configured interval controlling peek/hide cycles. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameDirector|Difficulty", meta = (AllowPrivateAccess = "true"))
    float PeekInterval;

    /** Tracks whether the character is currently peeking out of cover. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameDirector|State", meta = (AllowPrivateAccess = "true"))
    bool bIsPeeking;

    /** Tracks whether the character is actively shooting. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameDirector|State", meta = (AllowPrivateAccess = "true"))
    bool bIsShooting;

    /** Active timer handle for firing bursts. */
    FTimerHandle FireTimerHandle;

    /** Active timer handle for peeking in and out of cover. */
    FTimerHandle PeekTimerHandle;

    /** Number of shots fired in the current burst. */
    int32 ShotsFiredInBurst;

    /** Cached difficulty for reference. */
    FAIDifficulty CurrentDifficulty;
};
