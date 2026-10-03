// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerAnimNotify.h"

#include "Animation/AnimNotifyLibrary.h"
#include "KClonerAnimationComponent.h"
#include "KClonerModifierComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectGlobals.h"

#if WITH_EDITOR
#include "Animation/DebugSkelMeshComponent.h"
#include "UObject/StrongObjectPtr.h"
#endif

namespace
{
#if WITH_EDITOR
struct FStandalonePreviewBinding
{
	TWeakObjectPtr<USkeletalMeshComponent> Mesh;
	TStrongObjectPtr<UKClonerModifierComponent> Modifier;
	TStrongObjectPtr<UKClonerAnimationComponent> Animation;
};

TArray<FStandalonePreviewBinding> GStandalonePreviewBindings;

void PrunePreviewBindings()
{
	for (int32 Index = GStandalonePreviewBindings.Num() - 1; Index >= 0; --Index)
	{
		if (!GStandalonePreviewBindings[Index].Mesh.IsValid())
		{
			GStandalonePreviewBindings.RemoveAt(Index);
		}
	}
}
#endif

UKClonerAnimationComponent* FindAnimationComponent(
	USkeletalMeshComponent* MeshComp)
{
	if (!MeshComp)
	{
		return nullptr;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (Owner)
	{
		TArray<UKClonerAnimationComponent*> AnimationComponents;
		Owner->GetComponents<UKClonerAnimationComponent>(AnimationComponents);
		for (UKClonerAnimationComponent* Component : AnimationComponents)
		{
			if (Component && Component->GetTargetMesh() == MeshComp)
			{
				return Component;
			}
		}
	}

#if WITH_EDITOR
	if (!MeshComp->IsA<UDebugSkelMeshComponent>())
	{
		return nullptr;
	}

	// Some Persona preview platforms mark the debug mesh static. Modifier
	// effects are component-transform animation, so the preview target must be
	// movable; this branch is editor-only and never changes gameplay assets.
	MeshComp->SetMobility(EComponentMobility::Movable);

	// Persona's main preview owns the debug mesh through an editor-only actor;
	// the compact sequence/montage preview widgets often own it directly with
	// no actor. Both cases need a transient, per-mesh bridge. A single global
	// component is incorrect when the preview scene contains extra meshes.
	PrunePreviewBindings();
	for (FStandalonePreviewBinding& Binding : GStandalonePreviewBindings)
	{
		if (Binding.Mesh.Get() == MeshComp)
		{
			return Binding.Animation.Get();
		}
	}

	UKClonerModifierComponent* Modifier = nullptr;
	if (Owner)
	{
		Modifier = NewObject<UKClonerModifierComponent>(
			Owner, MakeUniqueObjectName(Owner,
				UKClonerModifierComponent::StaticClass(),
				TEXT("KClonerPreviewModifier")), RF_Transient);
	}
	else
	{
		Modifier = NewObject<UKClonerModifierComponent>(
			MeshComp, MakeUniqueObjectName(MeshComp,
				UKClonerModifierComponent::StaticClass(),
				TEXT("KClonerPreviewModifier")), RF_Transient);
	}
	if (!Modifier)
	{
		return nullptr;
	}
	Modifier->TargetMesh = MeshComp;
	Modifier->bAutoResolveTarget = false;
	Modifier->bPreviewInEditor = true;

	UKClonerAnimationComponent* Animation = nullptr;
	if (Owner)
	{
		Animation = NewObject<UKClonerAnimationComponent>(
			Owner, MakeUniqueObjectName(Owner,
				UKClonerAnimationComponent::StaticClass(),
				TEXT("KClonerPreviewAnimation")), RF_Transient);
	}
	else
	{
		Animation = NewObject<UKClonerAnimationComponent>(
			MeshComp, MakeUniqueObjectName(MeshComp,
				UKClonerAnimationComponent::StaticClass(),
				TEXT("KClonerPreviewAnimation")), RF_Transient);
	}
	if (!Animation)
	{
		return nullptr;
	}
	Animation->TargetMesh = MeshComp;
	Animation->bAutoResolveTarget = false;
	Animation->ModifierComponent = Modifier;
	Animation->bAllowEditorPreview = true;

	if (Owner)
	{
		Owner->AddInstanceComponent(Modifier);
		Owner->AddInstanceComponent(Animation);
		Modifier->RegisterComponent();
		Animation->RegisterComponent();
	}
	else if (UWorld* World = MeshComp->GetWorld())
	{
		Modifier->RegisterComponentWithWorld(World);
		Animation->RegisterComponentWithWorld(World);
	}
	else
	{
		return nullptr;
	}

	FStandalonePreviewBinding Binding;
	Binding.Mesh = MeshComp;
	Binding.Modifier.Reset(Modifier);
	Binding.Animation.Reset(Animation);
	GStandalonePreviewBindings.Add(MoveTemp(Binding));
	return Animation;
#else
	return nullptr;
#endif
}

FString MakeNotifyName(const FString& Prefix, const FName EventTag,
	const UKClonerAnimationEffectPreset* EffectPreset)
{
	if (!EventTag.IsNone())
	{
		return Prefix + TEXT(": ") + EventTag.ToString();
	}
	if (EffectPreset)
	{
		return Prefix + TEXT(": ") + EffectPreset->DisplayName;
	}
	return Prefix;
}
}

UAnimNotify_KClonerEvent::UAnimNotify_KClonerEvent()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(80, 180, 255, 255);
	bShouldFireInEditor = true;
#endif
}

void UAnimNotify_KClonerEvent::Notify(
	USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (UKClonerAnimationComponent* AnimationComponent =
		FindAnimationComponent(MeshComp))
	{
		AnimationComponent->TriggerNotifyEvent(MeshComp, Animation, EventTag,
			EffectPreset, InlineModifiers, InlineDistributionLayers,
			bUseInlineModifiers, bUseInlineDistribution,
			bSpawnDistributedCloner, WeightOverride);
	}
}

FString UAnimNotify_KClonerEvent::GetNotifyName_Implementation() const
{
	return MakeNotifyName(TEXT("KCloner Event"), EventTag, EffectPreset);
}

UAnimNotify_KClonerModifier::UAnimNotify_KClonerModifier()
{
	bUseInlineModifiers = false;
}

UAnimNotifyState_KClonerEvent::UAnimNotifyState_KClonerEvent()
{
	NotifyStateBehaviorFlags = static_cast<uint8>(
		EAnimNotifyStateBehaviorFlags::NoMergeOnConcurrentPlay);
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(255, 170, 60, 255);
	bShouldFireInEditor = true;
#endif
}

void UAnimNotifyState_KClonerEvent::NotifyBegin(
	USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	if (UKClonerAnimationComponent* AnimationComponent =
		FindAnimationComponent(MeshComp))
	{
		const float InitialAge = FMath::Clamp(
			UAnimNotifyLibrary::GetCurrentAnimationNotifyStateTime(
				EventReference),
			0.0f, FMath::Max(0.0f, TotalDuration));
		AnimationComponent->BeginNotifyEvent(this, MeshComp, Animation, EventTag,
			EffectPreset, InlineModifiers, InlineDistributionLayers,
			bUseInlineModifiers, bUseInlineDistribution,
			bSpawnDistributedCloner, TotalDuration, WeightOverride, InitialAge);
	}
}

void UAnimNotifyState_KClonerEvent::NotifyTick(
	USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	// The animation component owns the effect tick. Keeping this callback empty
	// avoids evaluating modifier stacks once per notify state in addition to the
	// component's normal update.
}

void UAnimNotifyState_KClonerEvent::NotifyEnd(
	USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (UKClonerAnimationComponent* AnimationComponent =
		FindAnimationComponent(MeshComp))
	{
		AnimationComponent->EndNotifyEvent(this, MeshComp, Animation, EventTag,
			EffectPreset);
	}
}

FString UAnimNotifyState_KClonerEvent::GetNotifyName_Implementation() const
{
	return MakeNotifyName(TEXT("KCloner Event State"), EventTag, EffectPreset);
}

FString UAnimNotify_KClonerModifier::GetNotifyName_Implementation() const
{
	return MakeNotifyName(TEXT("KCloner Modifier"), EventTag, EffectPreset);
}

UAnimNotify_KClonerDistribution::UAnimNotify_KClonerDistribution()
{
	bUseInlineDistribution = false;
	bSpawnDistributedCloner = true;
}

FString UAnimNotify_KClonerDistribution::GetNotifyName_Implementation() const
{
	return MakeNotifyName(TEXT("KCloner Distribution"), EventTag, EffectPreset);
}

UAnimNotifyState_KClonerModifier::UAnimNotifyState_KClonerModifier()
{
	bUseInlineModifiers = false;
}

FString UAnimNotifyState_KClonerModifier::GetNotifyName_Implementation() const
{
	return MakeNotifyName(TEXT("KCloner Modifier State"), EventTag, EffectPreset);
}

UAnimNotifyState_KClonerDistribution::UAnimNotifyState_KClonerDistribution()
{
	bUseInlineDistribution = false;
	bSpawnDistributedCloner = true;
}

FString UAnimNotifyState_KClonerDistribution::GetNotifyName_Implementation() const
{
	return MakeNotifyName(TEXT("KCloner Distribution State"), EventTag,
		EffectPreset);
}
