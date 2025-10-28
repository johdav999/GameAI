#include "LlamaRunner.h"

#include "Misc/Paths.h"

#include <llama.h>
#include <string>
#include <vector>

DEFINE_LOG_CATEGORY_STATIC(LogLlamaRunner, Log, All);

FLlamaRunner::FLlamaRunner()
    : Model(nullptr)
    , Context(nullptr)
    , bIsLoaded(false)
{
}

FLlamaRunner::~FLlamaRunner()
{
    Release();
}

bool FLlamaRunner::LoadModel(const FString& ModelPath)
{
    Release();

    if (!FPaths::FileExists(ModelPath))
    {
        UE_LOG(LogLlamaRunner, Error, TEXT("Unable to locate llama model at path: %s"), *ModelPath);
        return false;
    }

    const std::string ModelPathUtf8 = TCHAR_TO_UTF8(*ModelPath);

    llama_model_params ModelParams = llama_model_default_params();
    Model = llama_load_model_from_file(ModelPathUtf8.c_str(), ModelParams);

    if (!Model)
    {
        UE_LOG(LogLlamaRunner, Error, TEXT("Failed to load llama model from %s"), *ModelPath);
        return false;
    }

    llama_context_params ContextParams = llama_context_default_params();
    Context = llama_new_context_with_model(Model, ContextParams);

    if (!Context)
    {
        UE_LOG(LogLlamaRunner, Error, TEXT("Failed to create llama context for %s"), *ModelPath);
        Release();
        return false;
    }

    LoadedModelPath = ModelPath;
    bIsLoaded = true;

    UE_LOG(LogLlamaRunner, Log, TEXT("Loaded llama model from %s"), *ModelPath);
    return true;
}

FString FLlamaRunner::RunInference(const FString& Prompt)
{
    if (!bIsLoaded || Context == nullptr)
    {
        UE_LOG(LogLlamaRunner, Warning, TEXT("RunInference called before model was loaded."));
        return FString();
    }

    const std::string PromptUtf8 = TCHAR_TO_UTF8(*Prompt);

    if (PromptUtf8.empty())
    {
        return FString();
    }

    std::vector<llama_token> Tokens;
    Tokens.reserve(PromptUtf8.size());
    for (uint8 Char : PromptUtf8)
    {
        Tokens.push_back(static_cast<llama_token>(Char));
    }

    const int32 Result = llama_eval(Context, Tokens.data(), static_cast<int>(Tokens.size()), 0, 0);
    if (Result != 0)
    {
        UE_LOG(LogLlamaRunner, Error, TEXT("llama_eval failed for prompt: %s"), *Prompt);
        return FString();
    }

    // Placeholder: Without sampling hooks we simply echo the prompt.
    // Future iterations can read logits from the llama context and perform sampling here.
    return Prompt;
}

void FLlamaRunner::Release()
{
    if (Context)
    {
        llama_free(Context);
        Context = nullptr;
    }

    if (Model)
    {
        llama_free_model(Model);
        Model = nullptr;
    }

    if (!LoadedModelPath.IsEmpty())
    {
        UE_LOG(LogLlamaRunner, Log, TEXT("Unloaded llama model %s"), *LoadedModelPath);
    }

    LoadedModelPath.Reset();
    bIsLoaded = false;
}
