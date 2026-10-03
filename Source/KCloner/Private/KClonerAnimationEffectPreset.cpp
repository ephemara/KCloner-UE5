// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerAnimationEffectPreset.h"

UKClonerAnimationEffectPreset::UKClonerAnimationEffectPreset()
{
}

#if WITH_EDITOR
void UKClonerAnimationEffectPreset::PostEditChangeProperty(
	FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.GetPropertyName() !=
		GET_MEMBER_NAME_CHECKED(UKClonerAnimationEffectPreset, SourceRevision))
	{
		SourceRevision = FMath::Max(1, SourceRevision + 1);
	}
}
#endif
