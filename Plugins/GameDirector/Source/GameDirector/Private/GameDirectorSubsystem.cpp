#include "GameDirectorSubsystem.h"

#include "LlamaRunner.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogGameDirectorSubsystem, Log, All);

void UGameDirectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    LlamaRunner = MakeShared<FLlamaRunner>();

    const FString ModelPath = ResolveModelPath();
    if (ModelPath.IsEmpty())
    {
        UE_LOG(LogGameDirectorSubsystem, Warning, TEXT("No GGUF model found under Content/AIModels/. GameDirector subsystem will be inactive."));
        LlamaRunner.Reset();
        return;
    }

    if (!LlamaRunner->LoadModel(ModelPath))
    {
        UE_LOG(LogGameDirectorSubsystem, Error, TEXT("Failed to load llama model at %s"), *ModelPath);
        LlamaRunner.Reset();
        return;
    }
}

void UGameDirectorSubsystem::Deinitialize()
{
    LlamaRunner.Reset();
    Super::Deinitialize();
}

FString UGameDirectorSubsystem::QueryModel(const FString& InputJSON)
{
    if (!LlamaRunner.IsValid())
    {
        UE_LOG(LogGameDirectorSubsystem, Warning, TEXT("QueryModel called but no llama model is loaded."));
        return FString();
    }

    return LlamaRunner->RunInference(InputJSON);
}

FString UGameDirectorSubsystem::ResolveModelPath() const
{
    const FString ModelsDirectory = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("AIModels"));

    if (!FPaths::DirectoryExists(ModelsDirectory))
    {
        return FString();
    }

    TArray<FString> FoundModels;
    IFileManager::Get().FindFiles(FoundModels, *(ModelsDirectory / TEXT("*.gguf")), true, false);

    if (FoundModels.Num() == 0)
    {
        return FString();
    }

    // Prefer the first model discovered in the directory.
    return FPaths::Combine(ModelsDirectory, FoundModels[0]);
}
