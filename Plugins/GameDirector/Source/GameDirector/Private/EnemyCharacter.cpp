#include "EnemyCharacter.h"

#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogEnemyCharacter, Log, All);

namespace
{
    constexpr float kMinPeekInterval = 0.1f;
    constexpr float kTraceDistance = 10000.f;
}

AEnemyCharacter::AEnemyCharacter()
{
    PrimaryActorTick.bCanEverTick = false;

    ReactionDelay = 1.5f;
    AimSpread = 5.0f;
    FireRate = 1.0f;
    BurstCount = 3;
    PeekInterval = 3.0f;
    bIsPeeking = false;
    bIsShooting = false;
    ShotsFiredInBurst = 0;
}

void AEnemyCharacter::BeginPlay()
{
    Super::BeginPlay();

    RefreshPeekTimer();
}

void AEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    GetWorldTimerManager().ClearTimer(FireTimerHandle);
    GetWorldTimerManager().ClearTimer(PeekTimerHandle);
}

void AEnemyCharacter::StartShooting()
{
    if (bIsShooting)
    {
        return;
    }

    if (!ensure(FireRate > KINDA_SMALL_NUMBER))
    {
        FireRate = 1.0f;
    }

    bIsShooting = true;
    ShotsFiredInBurst = 0;

    const float FireInterval = 1.0f / FireRate;
    GetWorldTimerManager().SetTimer(FireTimerHandle, this, &AEnemyCharacter::FireOneShot, FireInterval, true, ReactionDelay);
}

void AEnemyCharacter::StopShooting()
{
    if (!bIsShooting)
    {
        return;
    }

    bIsShooting = false;
    ShotsFiredInBurst = 0;

    GetWorldTimerManager().ClearTimer(FireTimerHandle);
}

void AEnemyCharacter::FireOneShot()
{
    if (!bIsShooting)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const FVector Start = GetActorLocation() + GetActorForwardVector() * 50.f;
    const float AimSpreadRadians = FMath::DegreesToRadians(AimSpread);
    const FVector ShootDirection = FMath::VRandCone(GetActorForwardVector(), AimSpreadRadians);
    const FVector End = Start + ShootDirection * kTraceDistance;

    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyCharacterFire), false, this);

    const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
    const FColor DebugColor = bHit ? FColor::Red : FColor::Yellow;

    DrawDebugLine(World, Start, bHit ? Hit.ImpactPoint : End, DebugColor, false, 1.0f, 0, 1.5f);

    ++ShotsFiredInBurst;
    if (ShotsFiredInBurst >= BurstCount)
    {
        ShotsFiredInBurst = 0;

        const float FireInterval = (FireRate > KINDA_SMALL_NUMBER) ? (1.0f / FireRate) : 1.0f;
        GetWorldTimerManager().ClearTimer(FireTimerHandle);
        GetWorldTimerManager().SetTimer(FireTimerHandle, this, &AEnemyCharacter::FireOneShot, FireInterval, true, ReactionDelay);
    }
}

void AEnemyCharacter::ApplyDifficulty(const FAIDifficulty& Diff)
{
    CurrentDifficulty = Diff;

    ReactionDelay = FMath::Clamp(1.5f - Diff.ReactionLevel * 0.25f, 0.2f, 1.5f);
    AimSpread = FMath::Clamp(5.0f - Diff.AimSpreadLevel * 0.8f + Diff.AimSpreadFine, 0.5f, 8.0f);
    FireRate = FMath::Clamp(1.0f + Diff.AggressionLevel * 0.5f, 1.0f, 5.0f);
    BurstCount = FMath::Clamp(2 + Diff.AggressionLevel, 2, 8);
    PeekInterval = FMath::Clamp(3.0f - Diff.PeekLevel * 0.4f, 0.5f, 3.0f);

    UE_LOG(LogEnemyCharacter, Log, TEXT("ApplyDifficulty: ReactionDelay=%.2f AimSpread=%.2f FireRate=%.2f BurstCount=%d PeekInterval=%.2f"), ReactionDelay, AimSpread, FireRate, BurstCount, PeekInterval);

    if (GEngine)
    {
        const FString Message = FString::Printf(TEXT("AI Diff -> Reaction: %.2fs | Spread: %.2f | FireRate: %.2f | Burst: %d"), ReactionDelay, AimSpread, FireRate, BurstCount);
        const uint64 DebugKey = reinterpret_cast<uint64>(this);
        GEngine->AddOnScreenDebugMessage(DebugKey, 2.0f, FColor::Green, Message);
    }

    RefreshPeekTimer();

    if (bIsShooting)
    {
        StopShooting();
        if (bIsPeeking)
        {
            StartShooting();
        }
    }
}

void AEnemyCharacter::TogglePeek()
{
    bIsPeeking = !bIsPeeking;

    if (bIsPeeking)
    {
        ExitCover();
    }
    else
    {
        EnterCover();
    }
}

void AEnemyCharacter::EnterCover()
{
    StopShooting();
}

void AEnemyCharacter::ExitCover()
{
    if (!bIsShooting)
    {
        StartShooting();
    }
}

void AEnemyCharacter::RefreshPeekTimer()
{
    const float Interval = FMath::Max(PeekInterval, kMinPeekInterval);

    GetWorldTimerManager().ClearTimer(PeekTimerHandle);
    GetWorldTimerManager().SetTimer(PeekTimerHandle, this, &AEnemyCharacter::TogglePeek, Interval, true, Interval);
}
