// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerAnimationComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "KClonerActor.h"
#include "KClonerModifierComponent.h"
#include "KClonerModifier.h"
#include "UObject/UObjectGlobals.h"

UKClonerAnimationComponent::UKClonerAnimationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bTickInEditor = true;
	bAutoActivate = true;
}

void UKClonerAnimationComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshAnimInstanceBinding();
}

void UKClonerAnimationComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	EndAllEffects();
	UnbindAnimInstance();
	Super::EndPlay(EndPlayReason);
}

void UKClonerAnimationComponent::TickComponent(
	float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshAnimInstanceBinding();
	ResetPreviewEffectsIfSourceChanged();

	for (int32 Index = ActiveEffects.Num() - 1; Index >= 0; --Index)
	{
		FActiveAnimationEffect& Effect = ActiveEffects[Index];
		if (!Effect.EffectPreset.IsValid())
		{
			CleanupDistributedCloners(Effect);
			ActiveEffects.RemoveAt(Index);
			continue;
		}

		Effect.Age += FMath::Max(0.0f, DeltaTime);
		const float EffectTime =
			Effect.Age * Effect.EffectPreset->TimeScale;

		for (const TWeakObjectPtr<AKClonerActor>& WeakCloner :
			Effect.DistributedCloners)
		{
			if (AKClonerActor* Cloner = WeakCloner.Get())
			{
				Cloner->bUseOverrideTime = true;
				Cloner->OverrideTime = EffectTime;
			}
		}

		if (!Effect.bStateEffect &&
			Effect.Age >= Effect.Duration +
				Effect.EffectPreset->BlendOut)
		{
			CleanupDistributedCloners(Effect);
			ActiveEffects.RemoveAt(Index);
		}
	}

	if (ActiveEffects.Num() == 0)
	{
		RuntimeNotifyPresets.Empty();
	}
}

UMeshComponent* UKClonerAnimationComponent::ResolveAnyTargetMesh() const
{
	if (IsValid(TargetMesh))
	{
		return TargetMesh;
	}

	if (!bAutoResolveTarget)
	{
		return nullptr;
	}

	if (USkeletalMeshComponent* AttachedMesh =
		Cast<USkeletalMeshComponent>(GetAttachParent()))
	{
		return AttachedMesh;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	if (USkeletalMeshComponent* RootMesh =
		Cast<USkeletalMeshComponent>(Owner->GetRootComponent()))
	{
		return RootMesh;
	}

	TArray<USkeletalMeshComponent*> Meshes;
	Owner->GetComponents<USkeletalMeshComponent>(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (IsValid(Mesh))
		{
			return Mesh;
		}
	}

	return nullptr;
}

USkeletalMeshComponent* UKClonerAnimationComponent::GetTargetMesh() const
{
	return Cast<USkeletalMeshComponent>(ResolveAnyTargetMesh());
}

UKClonerModifierComponent*
UKClonerAnimationComponent::GetModifierComponent() const
{
	if (IsValid(ModifierComponent))
	{
		return ModifierComponent;
	}

	AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UKClonerModifierComponent>()
		           : nullptr;
}

UAnimInstance* UKClonerAnimationComponent::ResolveAnimInstance() const
{
	if (USkeletalMeshComponent* Mesh = GetTargetMesh())
	{
		return Mesh->GetAnimInstance();
	}
	return nullptr;
}

void UKClonerAnimationComponent::UnbindAnimInstance()
{
	if (UAnimInstance* AnimInstance = BoundAnimInstance.Get())
	{
		AnimInstance->OnMontageStarted.RemoveDynamic(
			this, &UKClonerAnimationComponent::HandleMontageStarted);
		AnimInstance->OnMontageEnded.RemoveDynamic(
			this, &UKClonerAnimationComponent::HandleMontageEnded);
	}
	BoundAnimInstance.Reset();
}

void UKClonerAnimationComponent::RefreshAnimInstanceBinding()
{
	UAnimInstance* CurrentAnimInstance = ResolveAnimInstance();
	if (BoundAnimInstance.Get() == CurrentAnimInstance)
	{
		return;
	}

	UnbindAnimInstance();
	if (CurrentAnimInstance)
	{
		CurrentAnimInstance->OnMontageStarted.AddDynamic(
			this, &UKClonerAnimationComponent::HandleMontageStarted);
		CurrentAnimInstance->OnMontageEnded.AddDynamic(
			this, &UKClonerAnimationComponent::HandleMontageEnded);
		BoundAnimInstance = CurrentAnimInstance;
	}
}

void UKClonerAnimationComponent::ResetPreviewEffectsIfSourceChanged()
{
	UWorld* World = GetWorld();
	if (!World || World->IsGameWorld())
	{
		return;
	}

	UAnimationAsset* CurrentAnimationAsset = nullptr;
	UAnimMontage* CurrentMontage = nullptr;
	if (UAnimInstance* AnimInstance = ResolveAnimInstance())
	{
		CurrentMontage = AnimInstance->GetCurrentActiveMontage();
		if (!CurrentMontage)
		{
			if (UAnimSingleNodeInstance* SingleNode =
				Cast<UAnimSingleNodeInstance>(AnimInstance))
			{
				CurrentAnimationAsset = SingleNode->GetAnimationAsset();
			}
		}
	}

	const bool bHasPreviousSource = LastPreviewAnimationAsset.IsValid() ||
		LastPreviewMontage.IsValid();
	const bool bSourceChanged =
		LastPreviewAnimationAsset.Get() != CurrentAnimationAsset ||
		LastPreviewMontage.Get() != CurrentMontage;
	if (bHasPreviousSource && bSourceChanged && ActiveEffects.Num() > 0)
	{
		EndAllEffects();
	}

	LastPreviewAnimationAsset = CurrentAnimationAsset;
	LastPreviewMontage = CurrentMontage;
}

void UKClonerAnimationComponent::HandleMontageStarted(UAnimMontage* Montage)
{
	OnMontageStarted.Broadcast(Montage);
}

void UKClonerAnimationComponent::HandleMontageEnded(UAnimMontage* Montage,
	bool bInterrupted)
{
	// Notify states own their effect handles and receive NotifyEnd even when a
	// montage blends out. Do not clear every layer here: an unrelated montage
	// ending must not cancel locomotion or another concurrent reaction.
	OnMontageEnded.Broadcast(Montage, bInterrupted);
}

float UKClonerAnimationComponent::PlayMontage(UAnimMontage* Montage,
	float PlayRate, FName StartSection)
{
	UAnimInstance* AnimInstance = ResolveAnimInstance();
	if (!AnimInstance || !Montage)
	{
		return 0.0f;
	}

	const float Duration = AnimInstance->Montage_Play(Montage, PlayRate);
	if (Duration > 0.0f && !StartSection.IsNone())
	{
		AnimInstance->Montage_JumpToSection(StartSection, Montage);
	}
	return Duration;
}

void UKClonerAnimationComponent::StopMontage(float BlendOutTime,
	UAnimMontage* Montage)
{
	if (UAnimInstance* AnimInstance = ResolveAnimInstance())
	{
		AnimInstance->Montage_Stop(BlendOutTime, Montage);
	}
}

void UKClonerAnimationComponent::PauseMontage(UAnimMontage* Montage)
{
	if (UAnimInstance* AnimInstance = ResolveAnimInstance())
	{
		AnimInstance->Montage_Pause(Montage);
	}
}

void UKClonerAnimationComponent::ResumeMontage(UAnimMontage* Montage)
{
	if (UAnimInstance* AnimInstance = ResolveAnimInstance())
	{
		if (Montage)
		{
			AnimInstance->Montage_Resume(Montage);
		}
	}
}

void UKClonerAnimationComponent::SetMontagePlayRate(UAnimMontage* Montage,
	float PlayRate)
{
	if (UAnimInstance* AnimInstance = ResolveAnimInstance())
	{
		AnimInstance->Montage_SetPlayRate(Montage, PlayRate);
	}
}

void UKClonerAnimationComponent::JumpToMontageSection(FName SectionName,
	UAnimMontage* Montage)
{
	if (UAnimInstance* AnimInstance = ResolveAnimInstance())
	{
		AnimInstance->Montage_JumpToSection(SectionName, Montage);
	}
}

UAnimMontage* UKClonerAnimationComponent::GetCurrentMontage() const
{
	if (UAnimInstance* AnimInstance = ResolveAnimInstance())
	{
		return AnimInstance->GetCurrentActiveMontage();
	}
	return nullptr;
}

void UKClonerAnimationComponent::CleanupDistributedCloners(
	FActiveAnimationEffect& Effect)
{
	for (const TWeakObjectPtr<AKClonerActor>& WeakCloner :
		Effect.DistributedCloners)
	{
		if (AKClonerActor* Cloner = WeakCloner.Get())
		{
			Cloner->Destroy();
		}
	}
	Effect.DistributedCloners.Reset();
}

AKClonerActor* UKClonerAnimationComponent::SpawnDistributedEffect(
	UKClonerAnimationEffectPreset* EffectPreset, float Lifetime)
{
	if (!EffectPreset || !EffectPreset->bSpawnDistributedCloner ||
		!GetWorld())
	{
		return nullptr;
	}

	UMeshComponent* Target = ResolveAnyTargetMesh();
	if (!Target)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.Owner = GetOwner();

	AKClonerActor* Cloner = GetWorld()->SpawnActor<AKClonerActor>(
		AKClonerActor::StaticClass(), Target->GetComponentTransform(),
		SpawnParameters);
	if (!Cloner)
	{
		return nullptr;
	}

	Cloner->AddTickPrerequisiteComponent(this);
	Cloner->AttachToComponent(Target,
		FAttachmentTransformRules::KeepWorldTransform);
	Cloner->SetActorTransform(Target->GetComponentTransform());
	Cloner->DistributionLayers = EffectPreset->DistributionLayers;
	Cloner->SkeletalMode = EffectPreset->SkeletalMode;
	Cloner->Modifiers.Empty();

	UStaticMesh* StaticSource = EffectPreset->SourceMeshOverride;
	USkeletalMesh* SkeletalSource = EffectPreset->SourceSkeletalMeshOverride;
	UAnimSequence* AnimationSource = nullptr;
	UAnimMontage* ActiveMontage = nullptr;
	float ActiveMontageRate = 1.0f;

	if (USkeletalMeshComponent* SkeletalTarget =
		Cast<USkeletalMeshComponent>(Target))
	{
		if (UAnimInstance* AnimInstance = SkeletalTarget->GetAnimInstance())
		{
			ActiveMontage = AnimInstance->GetCurrentActiveMontage();
			if (ActiveMontage)
			{
				ActiveMontageRate = AnimInstance->Montage_GetPlayRate(
					ActiveMontage);
			}
			else if (UAnimSingleNodeInstance* SingleNode =
				Cast<UAnimSingleNodeInstance>(AnimInstance))
			{
				AnimationSource = Cast<UAnimSequence>(
					SingleNode->GetAnimationAsset());
			}
		}
	}

	if (EffectPreset->bUseOwnerMesh && !StaticSource && !SkeletalSource)
	{
		if (UStaticMeshComponent* StaticMesh =
			Cast<UStaticMeshComponent>(Target))
		{
			StaticSource = StaticMesh->GetStaticMesh();
		}
		else if (USkeletalMeshComponent* SkeletalMesh =
			Cast<USkeletalMeshComponent>(Target))
		{
			SkeletalSource = SkeletalMesh->GetSkeletalMeshAsset();
		}
	}

	Cloner->SourceMesh = StaticSource;
	Cloner->SourceSkeletalMesh = SkeletalSource;
	Cloner->SourceAnimSequence = AnimationSource;

	for (UKClonerModifier* SourceModifier : EffectPreset->Modifiers)
	{
		if (SourceModifier)
		{
			if (UKClonerModifier* RuntimeModifier =
				DuplicateObject<UKClonerModifier>(SourceModifier, Cloner))
			{
				Cloner->Modifiers.Add(RuntimeModifier);
			}
		}
	}

	Cloner->bUseOverrideTime = true;
	Cloner->OverrideTime = 0.0f;
	Cloner->ForceRebuild();
	if (ActiveMontage)
	{
		Cloner->PlayClonerMontage(ActiveMontage, ActiveMontageRate);
	}

	if (Lifetime > 0.0f)
	{
		Cloner->SetLifeSpan(Lifetime + EffectPreset->BlendOut + 0.05f);
	}

	return Cloner;
}

UKClonerAnimationEffectPreset* UKClonerAnimationComponent::BuildNotifyEffectPreset(
	UKClonerAnimationEffectPreset* EffectPreset,
	const TArray<UKClonerModifier*>& InlineModifiers,
	const TArray<FKClonerDistributionLayer>& InlineDistributionLayers,
	bool bUseInlineModifiers, bool bUseInlineDistribution,
	bool bSpawnDistributedCloner)
{
	const bool bNeedsRuntimePreset = bUseInlineModifiers ||
		bUseInlineDistribution || bSpawnDistributedCloner || !EffectPreset;
	if (!bNeedsRuntimePreset)
	{
		return EffectPreset;
	}

	UKClonerAnimationEffectPreset* RuntimePreset =
		NewObject<UKClonerAnimationEffectPreset>(this, NAME_None, RF_Transient);
	if (!RuntimePreset)
	{
		return EffectPreset;
	}

	if (EffectPreset)
	{
		RuntimePreset->DisplayName = EffectPreset->DisplayName;
		RuntimePreset->Category = EffectPreset->Category;
		RuntimePreset->Description = EffectPreset->Description;
		RuntimePreset->DefaultLayer = EffectPreset->DefaultLayer;
		RuntimePreset->Priority = EffectPreset->Priority;
		RuntimePreset->Weight = EffectPreset->Weight;
		RuntimePreset->bReplaceSameLayer = EffectPreset->bReplaceSameLayer;
		RuntimePreset->Duration = EffectPreset->Duration;
		RuntimePreset->BlendIn = EffectPreset->BlendIn;
		RuntimePreset->BlendOut = EffectPreset->BlendOut;
		RuntimePreset->TimeScale = EffectPreset->TimeScale;
		RuntimePreset->bSpawnDistributedCloner =
			EffectPreset->bSpawnDistributedCloner;
		RuntimePreset->DistributionLayers = EffectPreset->DistributionLayers;
		RuntimePreset->bUseOwnerMesh = EffectPreset->bUseOwnerMesh;
		RuntimePreset->SourceMeshOverride = EffectPreset->SourceMeshOverride;
		RuntimePreset->SourceSkeletalMeshOverride =
			EffectPreset->SourceSkeletalMeshOverride;
		RuntimePreset->SkeletalMode = EffectPreset->SkeletalMode;
	}

	auto CopyModifiers = [RuntimePreset](
		const TArray<UKClonerModifier*>& SourceModifiers)
	{
		for (UKClonerModifier* SourceModifier : SourceModifiers)
		{
			if (SourceModifier)
			{
				if (UKClonerModifier* RuntimeModifier =
					DuplicateObject<UKClonerModifier>(SourceModifier,
						RuntimePreset))
				{
					RuntimePreset->Modifiers.Add(RuntimeModifier);
				}
			}
		}
	};

	if (bUseInlineModifiers)
	{
		CopyModifiers(InlineModifiers);
	}
	else if (EffectPreset)
	{
		CopyModifiers(EffectPreset->Modifiers);
	}

	if (bUseInlineDistribution)
	{
		RuntimePreset->DistributionLayers = InlineDistributionLayers;
	}
	if (bSpawnDistributedCloner)
	{
		RuntimePreset->bSpawnDistributedCloner = true;
	}

	RuntimeNotifyPresets.Add(RuntimePreset);
	return RuntimePreset;
}

int32 UKClonerAnimationComponent::BeginEffectInternal(
	UKClonerAnimationEffectPreset* EffectPreset, FName EventTag,
	float WindowDuration, float WeightOverride, bool bStateEffect,
	USkeletalMeshComponent* SourceMesh, UAnimSequenceBase* Animation,
	float InitialAge)
{
	if (UWorld* World = GetWorld(); World && !World->IsGameWorld() &&
		!bAllowEditorPreview)
	{
		return INDEX_NONE;
	}

	if (!EffectPreset)
	{
		return INDEX_NONE;
	}

	UKClonerModifierComponent* Modifier = GetModifierComponent();
	const FName LayerName = EffectPreset->DefaultLayer;
	const int32 ModifierHandle = Modifier
		? (bStateEffect
			? Modifier->BeginModifierEffect(EffectPreset, LayerName,
				WindowDuration, WeightOverride, InitialAge)
			: Modifier->TriggerModifierEffect(EffectPreset, LayerName,
				WeightOverride))
		: INDEX_NONE;

	FActiveAnimationEffect ActiveEffect;
	ActiveEffect.Handle = NextEffectHandle++;
	if (NextEffectHandle == INDEX_NONE)
	{
		NextEffectHandle = 1;
	}
	ActiveEffect.ModifierHandle = ModifierHandle;
	ActiveEffect.EffectPreset = EffectPreset;
	ActiveEffect.Age = FMath::Max(0.0f, InitialAge);
	ActiveEffect.SourceMesh = SourceMesh;
	ActiveEffect.Animation = Animation;
	ActiveEffect.EventTag = EventTag;
	ActiveEffect.Duration = bStateEffect
		? FMath::Max(0.0f, WindowDuration)
		: FMath::Max(0.0f, EffectPreset->Duration);
	ActiveEffect.bStateEffect = bStateEffect;

	const float SpawnLifetime = bStateEffect
		? 0.0f
		: ActiveEffect.Duration;
	if (AKClonerActor* Distributed =
		SpawnDistributedEffect(EffectPreset, SpawnLifetime))
	{
		Distributed->bUseOverrideTime = true;
		Distributed->OverrideTime =
			ActiveEffect.Age * EffectPreset->TimeScale;
		ActiveEffect.DistributedCloners.Add(Distributed);
	}

	ActiveEffects.Add(MoveTemp(ActiveEffect));
	OnAnimationEvent.Broadcast(EventTag, true, WindowDuration,
		WeightOverride >= 0.0f ? WeightOverride : EffectPreset->Weight,
		SourceMesh, EffectPreset);

	return ActiveEffects.Last().Handle;
}

int32 UKClonerAnimationComponent::TriggerEffect(
	UKClonerAnimationEffectPreset* EffectPreset, FName EventTag,
	float WeightOverride)
{
	return BeginEffectInternal(EffectPreset, EventTag,
		EffectPreset ? EffectPreset->Duration : 0.0f, WeightOverride, false,
		GetTargetMesh(), nullptr, 0.0f);
}

int32 UKClonerAnimationComponent::BeginEffect(
	UKClonerAnimationEffectPreset* EffectPreset, FName EventTag,
	float WindowDuration, float WeightOverride)
{
	const float ResolvedDuration = WindowDuration >= 0.0f
		? WindowDuration
		: (EffectPreset ? EffectPreset->Duration : 0.0f);
	return BeginEffectInternal(EffectPreset, EventTag, ResolvedDuration,
		WeightOverride, true, GetTargetMesh(), nullptr, 0.0f);
}

void UKClonerAnimationComponent::EndEffect(int32 EffectHandle)
{
	for (int32 Index = ActiveEffects.Num() - 1; Index >= 0; --Index)
	{
		FActiveAnimationEffect& Effect = ActiveEffects[Index];
		if (Effect.Handle != EffectHandle)
		{
			continue;
		}

		if (UKClonerModifierComponent* Modifier = GetModifierComponent())
		{
			if (Effect.ModifierHandle != INDEX_NONE)
			{
				Modifier->EndModifierEffect(Effect.ModifierHandle);
			}
		}

		CleanupDistributedCloners(Effect);
		OnAnimationEvent.Broadcast(Effect.EventTag, false, Effect.Duration,
			0.0f, Effect.SourceMesh.Get(), Effect.EffectPreset.Get());
		ActiveEffects.RemoveAt(Index);
		return;
	}
}

void UKClonerAnimationComponent::EndAllEffects()
{
	for (int32 Index = ActiveEffects.Num() - 1; Index >= 0; --Index)
	{
		EndEffect(ActiveEffects[Index].Handle);
	}
	ActiveNotifyHandles.Empty();
	RuntimeNotifyPresets.Empty();
}

int32 UKClonerAnimationComponent::TriggerNotifyEvent(
	USkeletalMeshComponent* InMesh, UAnimSequenceBase* Animation,
	FName EventTag, UKClonerAnimationEffectPreset* EffectPreset,
	const TArray<UKClonerModifier*>& InlineModifiers,
	const TArray<FKClonerDistributionLayer>& InlineDistributionLayers,
	bool bUseInlineModifiers, bool bUseInlineDistribution,
	bool bSpawnDistributedCloner, float WeightOverride)
{
	UKClonerAnimationEffectPreset* EffectivePreset = BuildNotifyEffectPreset(
		EffectPreset, InlineModifiers, InlineDistributionLayers,
		bUseInlineModifiers, bUseInlineDistribution, bSpawnDistributedCloner);
	return BeginEffectInternal(EffectivePreset, EventTag,
		EffectivePreset ? EffectivePreset->Duration : 0.0f, WeightOverride, false,
		InMesh, Animation, 0.0f);
}

int32 UKClonerAnimationComponent::BeginNotifyEvent(
	const UAnimNotifyState* NotifyState, USkeletalMeshComponent* InMesh,
	UAnimSequenceBase* Animation, FName EventTag,
	UKClonerAnimationEffectPreset* EffectPreset,
	const TArray<UKClonerModifier*>& InlineModifiers,
	const TArray<FKClonerDistributionLayer>& InlineDistributionLayers,
	bool bUseInlineModifiers, bool bUseInlineDistribution,
	bool bSpawnDistributedCloner, float TotalDuration,
	float WeightOverride, float InitialAge)
{
	UKClonerAnimationEffectPreset* EffectivePreset = BuildNotifyEffectPreset(
		EffectPreset, InlineModifiers, InlineDistributionLayers,
		bUseInlineModifiers, bUseInlineDistribution, bSpawnDistributedCloner);
	const int32 Handle = BeginEffectInternal(EffectivePreset, EventTag,
		TotalDuration, WeightOverride, true, InMesh, Animation, InitialAge);
	if (Handle != INDEX_NONE && NotifyState)
	{
		ActiveNotifyHandles.FindOrAdd(NotifyState).Add(Handle);
	}
	return Handle;
}

void UKClonerAnimationComponent::EndNotifyEvent(
	const UAnimNotifyState* NotifyState, USkeletalMeshComponent* InMesh,
	UAnimSequenceBase* Animation, FName EventTag,
	UKClonerAnimationEffectPreset* EffectPreset)
{
	if (!NotifyState)
	{
		return;
	}

	TArray<int32>* Handles = ActiveNotifyHandles.Find(NotifyState);
	if (!Handles || Handles->Num() == 0)
	{
		return;
	}

	const int32 Handle = Handles->Pop();
	if (Handles->Num() == 0)
	{
		ActiveNotifyHandles.Remove(NotifyState);
	}

	EndEffect(Handle);
}
