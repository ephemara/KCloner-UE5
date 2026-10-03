// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "KClonerAnimationEffectPreset.h"
#include "KClonerAnimationComponent.generated.h"

class AKClonerActor;
class UKClonerModifierComponent;
class UAnimInstance;
class UAnimMontage;
class UAnimationAsset;
class UAnimNotifyState;
class UAnimSequenceBase;
class USkeletalMeshComponent;
class UMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_SixParams(
	FOnKClonerAnimationEvent, FName, EventTag, bool, bStarted, float,
	WindowDuration, float, Weight, USkeletalMeshComponent*, MeshComponent,
	UKClonerAnimationEffectPreset*, EffectPreset);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnKClonerMontageStarted,
	UAnimMontage*, Montage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnKClonerMontageEnded,
	UAnimMontage*, Montage, bool, bInterrupted);

/**
 * Routes animation notifies into K-Cloner modifier layers and optional
 * distributed cloner bursts. Add it beside KClonerModifierComponent on an
 * actor with a skeletal mesh.
 */
UCLASS(ClassGroup = (KStudio),
	meta = (BlueprintSpawnableComponent, DisplayName = "KClonerAnimation"))
class KCLONER_API UKClonerAnimationComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UKClonerAnimationComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// ======= TARGET =======

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation|Target")
	TObjectPtr<USkeletalMeshComponent> TargetMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation|Target")
	bool bAutoResolveTarget = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation|Target")
	TObjectPtr<UKClonerModifierComponent> ModifierComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation|Playback")
	bool bAllowEditorPreview = true;

	// ======= EVENTS =======

	UPROPERTY(BlueprintAssignable, Category = "Animation|Events")
	FOnKClonerAnimationEvent OnAnimationEvent;

	UPROPERTY(BlueprintAssignable, Category = "Animation|Montage")
	FOnKClonerMontageStarted OnMontageStarted;

	UPROPERTY(BlueprintAssignable, Category = "Animation|Montage")
	FOnKClonerMontageEnded OnMontageEnded;

	// ======= MONTAGE PIPELINE =======

	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	float PlayMontage(UAnimMontage* Montage, float PlayRate = 1.0f,
		FName StartSection = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	void StopMontage(float BlendOutTime = 0.2f,
		UAnimMontage* Montage = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	void PauseMontage(UAnimMontage* Montage = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	void ResumeMontage(UAnimMontage* Montage);

	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	void SetMontagePlayRate(UAnimMontage* Montage, float PlayRate);

	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	void JumpToMontageSection(FName SectionName,
		UAnimMontage* Montage = nullptr);

	UFUNCTION(BlueprintPure, Category = "Animation|Montage")
	UAnimMontage* GetCurrentMontage() const;

	/** Trigger a one-shot effect from gameplay or Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "Animation|Effects")
	int32 TriggerEffect(UKClonerAnimationEffectPreset* EffectPreset,
		FName EventTag = NAME_None, float WeightOverride = -1.0f);

	/** Start a state-style effect and return a handle for EndEffect. */
	UFUNCTION(BlueprintCallable, Category = "Animation|Effects")
	int32 BeginEffect(UKClonerAnimationEffectPreset* EffectPreset,
		FName EventTag = NAME_None, float WindowDuration = -1.0f,
		float WeightOverride = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Animation|Effects")
	void EndEffect(int32 EffectHandle);

	UFUNCTION(BlueprintCallable, Category = "Animation|Effects")
	void EndAllEffects();

	UFUNCTION(BlueprintPure, Category = "Animation|Target")
	USkeletalMeshComponent* GetTargetMesh() const;

	UFUNCTION(BlueprintPure, Category = "Animation|Target")
	UKClonerModifierComponent* GetModifierComponent() const;

	// ======= NOTIFY ROUTING =======

	/** Called by UAnimNotify_KClonerEvent. */
	int32 BeginNotifyEvent(const UAnimNotifyState* NotifyState,
		USkeletalMeshComponent* InMesh, UAnimSequenceBase* Animation,
		FName EventTag, UKClonerAnimationEffectPreset* EffectPreset,
		const TArray<UKClonerModifier*>& InlineModifiers,
		const TArray<FKClonerDistributionLayer>& InlineDistributionLayers,
		bool bUseInlineModifiers, bool bUseInlineDistribution,
		bool bSpawnDistributedCloner, float TotalDuration,
		float WeightOverride, float InitialAge = 0.0f);

	/** Called by UAnimNotifyState_KClonerEvent. */
	void EndNotifyEvent(const UAnimNotifyState* NotifyState,
		USkeletalMeshComponent* InMesh, UAnimSequenceBase* Animation,
		FName EventTag, UKClonerAnimationEffectPreset* EffectPreset);

	/** Called by UAnimNotify_KClonerEvent. */
	int32 TriggerNotifyEvent(USkeletalMeshComponent* InMesh,
		UAnimSequenceBase* Animation, FName EventTag,
		UKClonerAnimationEffectPreset* EffectPreset,
		const TArray<UKClonerModifier*>& InlineModifiers,
		const TArray<FKClonerDistributionLayer>& InlineDistributionLayers,
		bool bUseInlineModifiers, bool bUseInlineDistribution,
		bool bSpawnDistributedCloner, float WeightOverride);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FActiveAnimationEffect
	{
		int32 Handle = INDEX_NONE;
		int32 ModifierHandle = INDEX_NONE;
		TWeakObjectPtr<UKClonerAnimationEffectPreset> EffectPreset;
		TArray<TWeakObjectPtr<AKClonerActor>> DistributedCloners;
		TWeakObjectPtr<USkeletalMeshComponent> SourceMesh;
		TWeakObjectPtr<UAnimSequenceBase> Animation;
		FName EventTag = NAME_None;
		float Age = 0.0f;
		float Duration = 0.0f;
		bool bStateEffect = false;
	};

	UMeshComponent* ResolveAnyTargetMesh() const;
	UAnimInstance* ResolveAnimInstance() const;
	void RefreshAnimInstanceBinding();
	void UnbindAnimInstance();
	void ResetPreviewEffectsIfSourceChanged();
	void CleanupDistributedCloners(FActiveAnimationEffect& Effect);
	AKClonerActor* SpawnDistributedEffect(
		UKClonerAnimationEffectPreset* EffectPreset, float Lifetime);
	UKClonerAnimationEffectPreset* BuildNotifyEffectPreset(
		UKClonerAnimationEffectPreset* EffectPreset,
		const TArray<UKClonerModifier*>& InlineModifiers,
		const TArray<FKClonerDistributionLayer>& InlineDistributionLayers,
		bool bUseInlineModifiers, bool bUseInlineDistribution,
		bool bSpawnDistributedCloner);
	int32 BeginEffectInternal(UKClonerAnimationEffectPreset* EffectPreset,
		FName EventTag, float WindowDuration, float WeightOverride,
		bool bStateEffect, USkeletalMeshComponent* SourceMesh,
		UAnimSequenceBase* Animation, float InitialAge = 0.0f);

	UFUNCTION()
	void HandleMontageStarted(UAnimMontage* Montage);

	UFUNCTION()
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	TArray<FActiveAnimationEffect> ActiveEffects;
	TMap<const UAnimNotifyState*, TArray<int32>> ActiveNotifyHandles;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UKClonerAnimationEffectPreset>> RuntimeNotifyPresets;
	TWeakObjectPtr<UAnimInstance> BoundAnimInstance;
	TWeakObjectPtr<UAnimationAsset> LastPreviewAnimationAsset;
	TWeakObjectPtr<UAnimMontage> LastPreviewMontage;
	int32 NextEffectHandle = 1;
};
