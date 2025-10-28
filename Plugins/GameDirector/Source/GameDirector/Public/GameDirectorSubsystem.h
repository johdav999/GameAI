#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "GameDirectorSubsystem.generated.h"

class FLlamaRunner;

/**
 * GameInstance subsystem responsible for hosting the llama.cpp runtime.
 */
UCLASS()
class GAMEDIRECTOR_API UGameDirectorSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /**
     * Routes a JSON encoded request to the llama model and returns the generated output text.
     */
    UFUNCTION(BlueprintCallable, Category = "GameDirector|AI")
    FString QueryModel(const FString& InputJSON);

private:
    /** Attempts to find a GGUF model under Content/AIModels. */
    FString ResolveModelPath() const;

private:
    TSharedPtr<FLlamaRunner> LlamaRunner;
};
