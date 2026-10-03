// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "KClonerModifier.h"
#include "KClonerTypes.h"
#include "KClonerAnimationEffectPreset.generated.h"

class UStaticMesh;
class USkeletalMesh;

/**
 * Reusable animation-driven K-Cloner reaction.
 *
 * Effect presets can be assigned directly to K-Cloner animation notifies or
 * imported from *.keffect.json source files. They contain the same modifier
 * objects used by AKClonerActor and optional distribution layers for transient
 * cloner bursts.
 */
UCLASS(BlueprintType)
class KCLONER_API UKClonerAnimationEffectPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	UKClonerAnimationEffectPreset();

	// ======= METADATA =======

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect|Metadata")
	FString DisplayName = TEXT("Animation Effect");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect|Metadata")
	FString Category = TEXT("Animation");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect|Metadata",
		meta = (MultiLine = true))
	FString Description;

	// ======= MODIFIERS =======

	/** Modifier stack applied to the owning KClonerModifierComponent. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite,
		Category = "Effect|Modifiers")
	TArray<UKClonerModifier*> Modifiers;

	/** Default layer used when the notify does not override the layer name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect|Modifiers")
	FName DefaultLayer = TEXT("Animation");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect|Modifiers")
	int32 Priority = 0;

	/** Blend weight applied when the effect is active. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp,
		Category = "Effect|Modifiers", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Weight = 1.0f;

	/** Replace an active effect using the same layer instead of stacking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect|Modifiers")
	bool bReplaceSameLayer = false;

	// ======= TIMING =======

	/** Lifetime for a one-shot notify. State notifies override this with their window duration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect|Timing",
		meta = (ClampMin = "0.0"))
	float Duration = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Effect|Timing",
		meta = (ClampMin = "0.0"))
	float BlendIn = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Effect|Timing",
		meta = (ClampMin = "0.0"))
	float BlendOut = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Effect|Timing")
	float TimeScale = 1.0f;

	// ======= DISTRIBUTION =======

	/** If enabled, also spawn a transient K-Cloner using DistributionLayers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect|Distribution")
	bool bSpawnDistributedCloner = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect|Distribution")
	TArray<FKClonerDistributionLayer> DistributionLayers;

	/** Use the animation component's mesh as the transient cloner source. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect|Distribution")
	bool bUseOwnerMesh = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect|Distribution")
	TObjectPtr<UStaticMesh> SourceMeshOverride = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect|Distribution")
	TObjectPtr<USkeletalMesh> SourceSkeletalMeshOverride = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect|Distribution")
	EKClonerSkeletalMode SkeletalMode = EKClonerSkeletalMode::PhysicsIK;

	// ======= EXTERNAL SOURCE =======

	UPROPERTY(VisibleAnywhere, Category = "Effect|Source")
	int32 SourceRevision = 1;

	UPROPERTY(VisibleAnywhere, Category = "Effect|Source")
	bool bImportedFromSource = false;

	UPROPERTY(VisibleAnywhere, Category = "Effect|Source")
	FString SourceFile;

	UPROPERTY(VisibleAnywhere, Category = "Effect|Source")
	FString SourceId;

	UPROPERTY(VisibleAnywhere, Category = "Effect|Source")
	int64 SourceTimestamp = 0;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(
		FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
