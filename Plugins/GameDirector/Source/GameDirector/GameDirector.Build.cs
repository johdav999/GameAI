// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameDirector : ModuleRules
{
	public GameDirector(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
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
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
            }
        );

        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
            }
        );
        }
}
