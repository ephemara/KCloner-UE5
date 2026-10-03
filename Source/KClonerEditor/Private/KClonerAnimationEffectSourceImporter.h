// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FMenuBuilder;
class UKClonerAnimationEffectPreset;

/** Imports external *.keffect.json animation reactions into DataAssets. */
class FKClonerAnimationEffectSourceImporter
{
public:
	static void Startup();
	static void Shutdown();
	static void Reload();

	static void BuildEffectMenu(
		FMenuBuilder& MenuBuilder,
		TFunction<void(UKClonerAnimationEffectPreset*)> OnEffectSelected);

private:
	static void ImportAll(bool bForce);
	static void OnEngineLoopInitComplete();
	static FDelegateHandle EngineInitDelegateHandle;
	static TArray<TWeakObjectPtr<UKClonerAnimationEffectPreset>> ImportedEffects;
};
