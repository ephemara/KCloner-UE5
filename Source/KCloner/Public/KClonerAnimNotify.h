// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "KClonerAnimationEffectPreset.h"
#include "KClonerAnimNotify.generated.h"

class UAnimSequenceBase;
class USkeletalMeshComponent;

/** One-shot animation event that triggers a K-Cloner effect preset. */
UCLASS(editinlinenew, Blueprintable, const, hidecategories = Object,
	collapsecategories,
	meta = (DisplayName = "KCloner Event"))
class KCLONER_API UAnimNotify_KClonerEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_KClonerEvent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KCloner Event")
	FName EventTag = TEXT("KCloner.Event");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KCloner Event")
	TObjectPtr<UKClonerAnimationEffectPreset> EffectPreset = nullptr;

	/** -1 uses the preset weight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KCloner Event",
		meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float WeightOverride = -1.0f;

	/** Copy the modifier stack into this notify for per-animation iteration. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite,
		Category = "KCloner Event|Inline")
	TArray<UKClonerModifier*> InlineModifiers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KCloner Event|Inline")
	bool bUseInlineModifiers = false;

	/** Optional distribution override authored directly in the animation editor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		Category = "KCloner Event|Distribution")
	TArray<FKClonerDistributionLayer> InlineDistributionLayers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		Category = "KCloner Event|Distribution")
	bool bUseInlineDistribution = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		Category = "KCloner Event|Distribution")
	bool bSpawnDistributedCloner = false;

	virtual void Notify(USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};

/** Duration-based animation event for modifier layers and reaction windows. */
UCLASS(editinlinenew, Blueprintable, const, hidecategories = Object,
	collapsecategories,
	meta = (DisplayName = "KCloner Event State"))
class KCLONER_API UAnimNotifyState_KClonerEvent : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_KClonerEvent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KCloner Event")
	FName EventTag = TEXT("KCloner.Window");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KCloner Event")
	TObjectPtr<UKClonerAnimationEffectPreset> EffectPreset = nullptr;

	/** -1 uses the preset weight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KCloner Event",
		meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float WeightOverride = -1.0f;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite,
		Category = "KCloner Event|Inline")
	TArray<UKClonerModifier*> InlineModifiers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KCloner Event|Inline")
	bool bUseInlineModifiers = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		Category = "KCloner Event|Distribution")
	TArray<FKClonerDistributionLayer> InlineDistributionLayers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		Category = "KCloner Event|Distribution")
	bool bUseInlineDistribution = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		Category = "KCloner Event|Distribution")
	bool bSpawnDistributedCloner = false;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation, float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation, float FrameDeltaTime,
		const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};

/** Explicit modifier-stack notify shown separately in the animation notify picker. */
UCLASS(editinlinenew, Blueprintable, const, hidecategories = Object,
	collapsecategories,
	meta = (DisplayName = "KCloner Modifier"))
class KCLONER_API UAnimNotify_KClonerModifier : public UAnimNotify_KClonerEvent
{
	GENERATED_BODY()

public:
	UAnimNotify_KClonerModifier();
	virtual FString GetNotifyName_Implementation() const override;
};

/** Explicit distribution burst notify shown separately in the notify picker. */
UCLASS(editinlinenew, Blueprintable, const, hidecategories = Object,
	collapsecategories,
	meta = (DisplayName = "KCloner Distribution"))
class KCLONER_API UAnimNotify_KClonerDistribution : public UAnimNotify_KClonerEvent
{
	GENERATED_BODY()

public:
	UAnimNotify_KClonerDistribution();
	virtual FString GetNotifyName_Implementation() const override;
};

/** Duration-based modifier-stack notify. */
UCLASS(editinlinenew, Blueprintable, const, hidecategories = Object,
	collapsecategories,
	meta = (DisplayName = "KCloner Modifier State"))
class KCLONER_API UAnimNotifyState_KClonerModifier
	: public UAnimNotifyState_KClonerEvent
{
	GENERATED_BODY()

public:
	UAnimNotifyState_KClonerModifier();
	virtual FString GetNotifyName_Implementation() const override;
};

/** Duration-based distribution notify. */
UCLASS(editinlinenew, Blueprintable, const, hidecategories = Object,
	collapsecategories,
	meta = (DisplayName = "KCloner Distribution State"))
class KCLONER_API UAnimNotifyState_KClonerDistribution
	: public UAnimNotifyState_KClonerEvent
{
	GENERATED_BODY()

public:
	UAnimNotifyState_KClonerDistribution();
	virtual FString GetNotifyName_Implementation() const override;
};
