// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerAnimationEffectPresetFactory.h"

#include "KClonerAnimationEffectPreset.h"

UKClonerAnimationEffectPresetFactory::UKClonerAnimationEffectPresetFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UKClonerAnimationEffectPreset::StaticClass();
}

UObject* UKClonerAnimationEffectPresetFactory::FactoryCreateNew(
	UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UKClonerAnimationEffectPreset>(InParent, InClass, InName,
		Flags);
}
