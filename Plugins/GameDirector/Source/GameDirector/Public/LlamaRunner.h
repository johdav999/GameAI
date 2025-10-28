#pragma once

#include "CoreMinimal.h"

struct llama_model;
struct llama_context;
/**
 * Thin wrapper that manages llama.cpp lifecycle for the GameDirector plugin.
 */
class GAMEDIRECTOR_API FLlamaRunner
{
public:
    FLlamaRunner();
    ~FLlamaRunner();

    /** Loads a GGUF model located on disk. */
    bool LoadModel(const FString& ModelPath);

    /** Executes a synchronous inference call using the provided prompt. */
    FString RunInference(const FString& Prompt);

private:
    void Release();

private:
    FString LoadedModelPath;
    llama_model* Model;
    llama_context* Context;
    bool bIsLoaded;
};
