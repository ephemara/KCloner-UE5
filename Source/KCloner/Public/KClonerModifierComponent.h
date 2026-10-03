// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "KClonerAnimationEffectPreset.h"
#include "KClonerModifier.h"
#include "KClonerModifierComponent.generated.h"

class UMeshComponent;

/** Coordinate space used when evaluating a modifier component's stack. */
UENUM(BlueprintType)
enum class EKClonerModifierTransformSpace : uint8
{
	/** Evaluate modifiers relative to the owning actor. This is the best default for moving gameplay actors. */
	ActorLocal,
	/** Evaluate modifiers directly against the mesh's world transform. */
	World
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnKClonerModifierUpdated, FTransform, ModifiedTransform, float, DeltaTime,
	float, ModifierTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnKClonerModifierReset);

/**
 * Gives any mesh-owning actor the same modifier stack used by AKClonerActor.
 *
 * Add this component to an actor, leave TargetMesh empty to use the attached or
 * first mesh component, and add inline UKClonerModifier objects to Modifiers.
 * The target is evaluated from its captured base transform every frame, so
 * effects do not accumulate or drift. Blueprint modifier subclasses, presets,
 * Sequencer-interpolated properties, and the built-in Bounce/Float/etc. effects
 * all work through the same UKClonerModifier::ApplyModifier path.
 */
UCLASS(ClassGroup = (KStudio),
       meta = (BlueprintSpawnableComponent, DisplayName = "KClonerModifier"))
class KCLONER_API UKClonerModifierComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UKClonerModifierComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	// ======= TARGET =======

	/** Explicit mesh to drive. Leave empty to auto-resolve the attached/first mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier|Target")
	TObjectPtr<UMeshComponent> TargetMesh = nullptr;

	/** Resolve the attached mesh, root mesh, or first mesh when TargetMesh is empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier|Target")
	bool bAutoResolveTarget = true;

	/** Space in which the modifier stack is evaluated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier|Target")
	EKClonerModifierTransformSpace TransformSpace =
		EKClonerModifierTransformSpace::ActorLocal;

	/** Allow modifiers to change each transform channel independently. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier|Target")
	bool bAffectLocation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier|Target")
	bool bAffectRotation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier|Target")
	bool bAffectScale = true;

	// ======= MODIFIER STACK =======

	/** Inline modifier stack. Add Bounce, Float, Custom, Preset, or any subclass. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite,
		Category = "Modifier|Stack")
	TArray<UKClonerModifier*> Modifiers;

	UFUNCTION(BlueprintPure, Category = "Modifier|Stack")
	UKClonerModifier* GetModifierByIndex(int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Modifier|Stack")
	int32 GetModifierCount() const { return Modifiers.Num(); }

	UFUNCTION(BlueprintCallable, Category = "Modifier|Stack",
		meta = (DeterminesOutputType = "ModifierClass"))
	UKClonerModifier* AddModifierOfClass(
		TSubclassOf<UKClonerModifier> ModifierClass);

	UFUNCTION(BlueprintCallable, Category = "Modifier|Stack")
	bool RemoveModifier(UKClonerModifier* Modifier);

	UFUNCTION(BlueprintCallable, Category = "Modifier|Stack")
	void ClearAllModifiers();

	UFUNCTION(BlueprintCallable, Category = "Modifier|Stack")
	void SetModifierEnabled(int32 Index, bool bEnabled);

	// ======= ANIMATION EFFECT LAYERS =======

	/** Begin a state-style effect. EndModifierEffect removes it with BlendOut. */
	UFUNCTION(BlueprintCallable, Category = "Modifier|Effects")
	int32 BeginModifierEffect(UKClonerAnimationEffectPreset* EffectPreset,
		FName LayerName = NAME_None, float DurationOverride = -1.0f,
		float WeightOverride = -1.0f, float InitialAge = 0.0f);

	/** Trigger a timed effect and return its runtime handle. */
	UFUNCTION(BlueprintCallable, Category = "Modifier|Effects")
	int32 TriggerModifierEffect(UKClonerAnimationEffectPreset* EffectPreset,
		FName LayerName = NAME_None, float WeightOverride = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Modifier|Effects")
	void EndModifierEffect(int32 EffectHandle);

	UFUNCTION(BlueprintCallable, Category = "Modifier|Effects")
	void EndAllModifierEffects();

	UFUNCTION(BlueprintPure, Category = "Modifier|Effects")
	bool IsModifierEffectActive(int32 EffectHandle) const;

	// ======= PLAYBACK =======

	/** Master switch. Disabling restores the captured base transform by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier|Playback")
	bool bEnabled = true;

	/** Advance modifier time. Pausing keeps the current pose and stops the clock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier|Playback")
	bool bPlaying = true;

	/** Start the local clock at this value when play begins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp,
		Category = "Modifier|Playback")
	float StartTime = 0.0f;

	/** Multiplier applied to local time. Negative values play the stack backwards. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Modifier|Playback")
	float TimeScale = 1.0f;

	/** Constant phase offset, useful for making enemy variants less synchronized. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp,
		Category = "Modifier|Playback")
	float TimeOffset = 0.0f;

	/** Use world time instead of a per-component clock when enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier|Playback")
	bool bUseWorldTime = false;

	/** Start playback automatically in PIE/game worlds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier|Playback")
	bool bPlayOnBeginPlay = true;

	/** Evaluate this component in editor viewports as well as during play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier|Playback")
	bool bPreviewInEditor = true;

	/** Restore the base transform when the component is disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier|Playback")
	bool bRestoreWhenDisabled = true;

	/** Restore the base transform when the component leaves play/editor registration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier|Playback")
	bool bRestoreOnUnregister = true;

	/** Index passed to modifiers for per-actor phase variation (Bounce Step, Delay, etc.). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier|Playback",
		meta = (ClampMin = "0"))
	int32 InstanceIndex = 0;

	/** Count passed to modifiers. Keep at 1 for a standalone actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier|Playback",
		meta = (ClampMin = "1"))
	int32 InstanceCount = 1;

	UFUNCTION(BlueprintCallable, Category = "Modifier|Playback")
	void SetEnabled(bool bInEnabled);

	UFUNCTION(BlueprintCallable, Category = "Modifier|Playback")
	void SetPlaying(bool bInPlaying);

	UFUNCTION(BlueprintCallable, Category = "Modifier|Playback")
	void Play();

	UFUNCTION(BlueprintCallable, Category = "Modifier|Playback")
	void Pause();

	UFUNCTION(BlueprintCallable, Category = "Modifier|Playback")
	void Restart(float InStartTime = 0.0f);

	UFUNCTION(BlueprintPure, Category = "Modifier|Playback")
	float GetModifierTime() const { return CurrentTime; }

	// ======= TARGET / POSE CONTROL =======

	UFUNCTION(BlueprintCallable, Category = "Modifier|Target")
	void SetTargetMesh(UMeshComponent* InTargetMesh);

	UFUNCTION(BlueprintPure, Category = "Modifier|Target")
	UMeshComponent* GetTargetMesh() const;

	/** Capture the target's current unmodified pose as the new base pose. */
	UFUNCTION(BlueprintCallable, Category = "Modifier|Target")
	void CaptureBaseTransform();

	/** Restore the target to its captured pose and stop applying the current pose. */
	UFUNCTION(BlueprintCallable, Category = "Modifier|Target")
	void ResetToBaseTransform();

	/** Re-evaluate the stack immediately without waiting for the next tick. */
	UFUNCTION(BlueprintCallable, Category = "Modifier|Target")
	void ApplyNow();

	UFUNCTION(BlueprintPure, Category = "Modifier|Target")
	FTransform GetBaseTransform() const { return BaseTransform; }

	UFUNCTION(BlueprintPure, Category = "Modifier|Target")
	FTransform GetModifiedTransform() const { return ModifiedTransform; }

	// ======= EVENTS =======

	UPROPERTY(BlueprintAssignable, Category = "Modifier|Events")
	FOnKClonerModifierUpdated OnModifierUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Modifier|Events")
	FOnKClonerModifierReset OnModifierReset;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(
		FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	struct FActiveModifierEffect
	{
		int32 Handle = INDEX_NONE;
		FName LayerName = NAME_None;
		UKClonerAnimationEffectPreset* EffectPreset = nullptr;
		TArray<UKClonerModifier*> RuntimeModifiers;
		float Age = 0.0f;
		float Duration = -1.0f;
		float Weight = 1.0f;
		int32 Priority = 0;
		int32 SourceRevision = INDEX_NONE;
		bool bAutoExpire = false;
		bool bEndRequested = false;
		float EndAge = 0.0f;
	};

	UMeshComponent* ResolveTargetMesh() const;

	FTransform GetTransformInSpace(
		const UMeshComponent* InTarget,
		EKClonerModifierTransformSpace InSpace) const;

	void SetTransformInSpace(
		UMeshComponent* InTarget, const FTransform& InTransform,
		EKClonerModifierTransformSpace InSpace) const;

	void SyncBaseWithExternalTransform(UMeshComponent* InTarget);
	void RestoreBaseTransformInternal();
	void ApplyModifierStack(float DeltaTime);
	void RefreshTargetAndBase();
	void ResetClock();
	void UpdateModifierEffects(float DeltaTime);
	void ApplyActiveModifierEffects(FTransform& InOutTransform, float BaseTime,
		float DeltaTime, int32 SafeIndex, int32 SafeCount);
	void RemoveModifierEffectAt(int32 ArrayIndex);
	void BuildRuntimeEffectModifiers(FActiveModifierEffect& Effect);
	float GetEffectWeight(const FActiveModifierEffect& Effect) const;

	TWeakObjectPtr<UMeshComponent> CachedTargetMesh;
	FTransform BaseTransform = FTransform::Identity;
	FTransform BaseWorldTransform = FTransform::Identity;
	FTransform ModifiedTransform = FTransform::Identity;
	FTransform LastAppliedTransform = FTransform::Identity;
	FTransform LastAppliedWorldTransform = FTransform::Identity;
	EKClonerModifierTransformSpace CachedTransformSpace =
		EKClonerModifierTransformSpace::ActorLocal;

	float ElapsedTime = 0.0f;
	float CurrentTime = 0.0f;
	TArray<float> ModifierCustomData;
	TArray<FActiveModifierEffect> ActiveModifierEffects;

	/** Reflected references keep duplicated effect modifiers and presets alive. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UKClonerModifier>> ActiveEffectModifierObjects;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UKClonerAnimationEffectPreset>> ActiveEffectPresets;

	int32 NextModifierEffectHandle = 1;

	bool bHasCapturedBaseTransform = false;
	bool bHasAppliedTransform = false;
};
