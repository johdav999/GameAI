// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class GameDirector : ModuleRules
{
	public GameDirector(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;


        string ThirdPartyPath = Path.Combine(ModuleDirectory, "..", "ThirdParty", "llama");
        PrivateIncludePaths.Add(Path.GetFullPath(Path.Combine(ThirdPartyPath, "include")));
        PrivateIncludePaths.Add(Path.Combine(ThirdPartyPath, "include", "ggml"));

        PublicIncludePaths.AddRange(
            new string[]
            {
                System.IO.Path.Combine(ModuleDirectory, "..", "..", "ThirdParty", "llama.cpp", "include"),
            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Projects",
                "AIModule",
                "GameplayTasks",
            }
        );
        string LlamaLibPath = Path.Combine(ThirdPartyPath, "win64", "lib");
        PublicAdditionalLibraries.Add(Path.Combine(LlamaLibPath, "llama.lib"));  // exact name needed
        RuntimeDependencies.Add(Path.Combine(LlamaLibPath, "llama.dll")); // if DLL

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Json",
                "JsonUtilities",
            }
        );

        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
            }
        );
        }
}
