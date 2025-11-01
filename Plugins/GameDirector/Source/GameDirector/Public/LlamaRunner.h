#pragma once

#include "CoreMinimal.h"
#include <random>
#include <numeric>
#include <cmath>
#include <cfloat>
#include <llama.h>

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
    llama_context_params ContextParams;
    /** Loads a GGUF model located on disk. */
    bool LoadModel(const FString& ModelPath);

    /** Executes a synchronous inference call using the provided scenario prompt and returns the raw JSON string. */
    FString RunInference(const FString& Prompt);
private:
    void Release();

private:
    FString LoadedModelPath;
    llama_model* Model;
    llama_context* Context;
    bool bIsLoaded;
    mutable FCriticalSection DecodeMutex;
};
