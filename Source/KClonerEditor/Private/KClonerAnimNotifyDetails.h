// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "KClonerTypes.h"

class UAnimNotify_KClonerEvent;
class UAnimNotifyState_KClonerEvent;
class UKClonerAnimationEffectPreset;
class UKClonerModifierPreset;
class UClass;

/** Persona/animation-editor details workflow for K-Cloner notify instances. */
class FKClonerAnimNotifyDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	UObject* GetSelectedObject() const;
	UKClonerAnimationEffectPreset* GetEffectPreset() const;
	void SetEffectPreset(UKClonerAnimationEffectPreset* EffectPreset);
	void AddExternalModifier(UKClonerModifierPreset* Preset);
	void AddBuiltInModifier(UClass* ModifierClass);
	void CopyEffectToInlineStack();
	void AddDistributionLayer(EKClonerMode Mode);
	void EnableInlineDistribution();

	TWeakObjectPtr<UObject> SelectedObject;
};
