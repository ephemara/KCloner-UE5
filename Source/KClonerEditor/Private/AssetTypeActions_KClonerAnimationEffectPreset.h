// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"
#include "CoreMinimal.h"
#include "KClonerAnimationEffectPreset.h"

class FAssetTypeActions_KClonerAnimationEffectPreset
	: public FAssetTypeActions_Base
{
public:
	explicit FAssetTypeActions_KClonerAnimationEffectPreset(
		EAssetTypeCategories::Type InAssetCategory)
		: MyAssetCategory(InAssetCategory)
	{
	}

	virtual FText GetName() const override
	{
		return NSLOCTEXT("AssetTypeActions",
			"AssetTypeActions_KClonerAnimationEffectPreset",
			"K-Cloner Animation Effect");
	}

	virtual FColor GetTypeColor() const override
	{
		return FColor(80, 180, 255);
	}

	virtual UClass* GetSupportedClass() const override
	{
		return UKClonerAnimationEffectPreset::StaticClass();
	}

	virtual uint32 GetCategories() override
	{
		return MyAssetCategory;
	}

private:
	EAssetTypeCategories::Type MyAssetCategory;
};
