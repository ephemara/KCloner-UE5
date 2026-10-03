// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FMenuBuilder;
class UKClonerModifier;
class UKClonerModifierPreset;
class UKClonerModifier_Preset;

/**
 * External modifier source importer and editor library.
 *
 * Source files are JSON because modifier definitions are nested and the
 * runtime already has a JSON-backed expression preset asset. Both a single
 * preset object and a { "Presets": [] } pack are supported.
 */
class FKClonerModifierSourceImporter
{
public:
	/** Scan project/plugin source folders and create or update generated assets. */
	static void Startup();
	static void Shutdown();
	static void Reload();

	/** Build the categorized external-preset menu used by details customizations. */
	static void BuildPresetMenu(
		FMenuBuilder& MenuBuilder,
		TFunction<void(UKClonerModifierPreset*)> OnPresetSelected);

	/** Create a configured runtime modifier wrapper for an imported preset. */
	static UKClonerModifier_Preset* CreatePresetModifier(
		UObject* Outer, UKClonerModifierPreset* Preset);

private:
	static void ImportAll(bool bForce);
	static void OnEngineLoopInitComplete();
	static FDelegateHandle EngineInitDelegateHandle;
	static TArray<TWeakObjectPtr<UKClonerModifierPreset>> ImportedPresets;
};
