// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerModifierComponent.h"

#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectGlobals.h"

UKClonerModifierComponent::UKClonerModifierComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bTickInEditor = true;
	bAutoActivate = true;

	ModifierCustomData.SetNum(3);
	ModifierCustomData[0] = 1.0f;
	ModifierCustomData[1] = 1.0f;
	ModifierCustomData[2] = 1.0f;
}

void UKClonerModifierComponent::OnRegister()
{
	Super::OnRegister();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		RefreshTargetAndBase();
	}
}

void UKClonerModifierComponent::OnUnregister()
{
	if (bRestoreOnUnregister && bHasAppliedTransform)
	{
		SyncBaseWithExternalTransform(CachedTargetMesh.Get());
		RestoreBaseTransformInternal();
	}

	Super::OnUnregister();
}

void UKClonerModifierComponent::BeginPlay()
{
	Super::BeginPlay();

	RefreshTargetAndBase();

	// A viewport preview may have already driven the mesh. Restore the original
	// captured pose before entering PIE, rather than baking the preview pose into
	// gameplay. If preview was disabled, capture any transform edited in the
	// level since registration.
	if (bHasAppliedTransform)
	{
		SyncBaseWithExternalTransform(CachedTargetMesh.Get());
		RestoreBaseTransformInternal();
	}
	else if (bHasCapturedBaseTransform)
	{
		CaptureBaseTransform();
	}

	ResetClock();
	bPlaying = bPlayOnBeginPlay;
}

void UKClonerModifierComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bRestoreOnUnregister && bHasAppliedTransform)
	{
		SyncBaseWithExternalTransform(CachedTargetMesh.Get());
		RestoreBaseTransformInternal();
	}

	Super::EndPlay(EndPlayReason);
}

void UKClonerModifierComponent::TickComponent(
	float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UWorld* World = GetWorld();
	if (!World || (!World->IsGameWorld() && !bPreviewInEditor))
	{
		return;
	}

	RefreshTargetAndBase();
	UMeshComponent* Target = CachedTargetMesh.Get();
	if (!Target || !bHasCapturedBaseTransform)
	{
		return;
	}

	// Detect movement made by gameplay code, animation setup, or a parent. The
	// component then carries that external change into the next modifier pose
	// instead of fighting it or accumulating its own previous result.
	if (bHasAppliedTransform)
	{
		SyncBaseWithExternalTransform(Target);
	}

	if (!bEnabled)
	{
		if (bRestoreWhenDisabled && bHasAppliedTransform)
		{
			RestoreBaseTransformInternal();
		}
		return;
	}

	if (bPlaying)
	{
		if (bUseWorldTime)
		{
			CurrentTime = World->GetTimeSeconds() * TimeScale + TimeOffset;
		}
		else
		{
			ElapsedTime += DeltaTime * TimeScale;
			CurrentTime = ElapsedTime + TimeOffset;
		}
	}

	UpdateModifierEffects(DeltaTime);
	ApplyModifierStack(DeltaTime);
}

void UKClonerModifierComponent::RefreshTargetAndBase()
{
	UMeshComponent* ResolvedTarget = ResolveTargetMesh();
	const bool bTargetChanged = CachedTargetMesh.Get() != ResolvedTarget;
	const bool bSpaceChanged =
		bHasCapturedBaseTransform &&
		CachedTransformSpace != TransformSpace;

	if (bHasCapturedBaseTransform && (bTargetChanged || bSpaceChanged))
	{
		if (bHasAppliedTransform)
		{
			SyncBaseWithExternalTransform(CachedTargetMesh.Get());
		}
		RestoreBaseTransformInternal();
	}

	if (!bHasCapturedBaseTransform || bTargetChanged || bSpaceChanged)
	{
		CachedTargetMesh = ResolvedTarget;
		bHasCapturedBaseTransform = false;

		if (ResolvedTarget)
		{
			BaseWorldTransform = ResolvedTarget->GetComponentTransform();
			BaseTransform = GetTransformInSpace(ResolvedTarget, TransformSpace);
			ModifiedTransform = BaseTransform;
			LastAppliedTransform = BaseTransform;
			LastAppliedWorldTransform = BaseWorldTransform;
			CachedTransformSpace = TransformSpace;
			bHasCapturedBaseTransform = true;
		}
		else
		{
			BaseTransform = FTransform::Identity;
			BaseWorldTransform = FTransform::Identity;
			ModifiedTransform = FTransform::Identity;
			LastAppliedTransform = FTransform::Identity;
			LastAppliedWorldTransform = FTransform::Identity;
		}

		bHasAppliedTransform = false;
	}
}

UMeshComponent* UKClonerModifierComponent::ResolveTargetMesh() const
{
	if (IsValid(TargetMesh))
	{
		return TargetMesh;
	}

	if (!bAutoResolveTarget)
	{
		return nullptr;
	}

	// The common setup is to attach this component directly to the mesh.
	if (UMeshComponent* AttachedMesh =
		Cast<UMeshComponent>(GetAttachParent()))
	{
		return AttachedMesh;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	// Prefer a mesh root so a simple StaticMeshActor works without any setup.
	if (UMeshComponent* RootMesh =
		Cast<UMeshComponent>(Owner->GetRootComponent()))
	{
		return RootMesh;
	}

	TArray<UMeshComponent*> MeshComponents;
	Owner->GetComponents<UMeshComponent>(MeshComponents);
	for (UMeshComponent* Mesh : MeshComponents)
	{
		if (IsValid(Mesh))
		{
			return Mesh;
		}
	}

	return nullptr;
}

FTransform UKClonerModifierComponent::GetTransformInSpace(
	const UMeshComponent* InTarget,
	EKClonerModifierTransformSpace InSpace) const
{
	if (!InTarget)
	{
		return FTransform::Identity;
	}

	const FTransform WorldTransform = InTarget->GetComponentTransform();
	if (InSpace == EKClonerModifierTransformSpace::World)
	{
		return WorldTransform;
	}

	const AActor* Owner = GetOwner();
	if (!Owner || Owner->GetRootComponent() == InTarget)
	{
		// The actor root has no meaningful transform relative to itself. Its
		// stable world base is handled separately by ApplyModifierStack().
		return Owner && Owner->GetRootComponent() == InTarget
			       ? FTransform::Identity
		       : WorldTransform;
	}

	return Owner->GetActorTransform().Inverse() * WorldTransform;
}

void UKClonerModifierComponent::SetTransformInSpace(
	UMeshComponent* InTarget, const FTransform& InTransform,
	EKClonerModifierTransformSpace InSpace) const
{
	if (!InTarget)
	{
		return;
	}

	FTransform WorldTransform = InTransform;
	AActor* Owner = GetOwner();
	const bool bIsActorRoot = Owner && Owner->GetRootComponent() == InTarget;

	if (InSpace == EKClonerModifierTransformSpace::ActorLocal && Owner &&
		!bIsActorRoot)
	{
		WorldTransform = Owner->GetActorTransform() * InTransform;
	}

	InTarget->SetWorldTransform(WorldTransform, false, nullptr,
		ETeleportType::TeleportPhysics);
}

void UKClonerModifierComponent::SyncBaseWithExternalTransform(
	UMeshComponent* InTarget)
{
	if (!InTarget || !bHasCapturedBaseTransform || !bHasAppliedTransform)
	{
		return;
	}

	AActor* Owner = GetOwner();
	const bool bIsActorRoot = Owner && Owner->GetRootComponent() == InTarget;
	const bool bTrackWorld =
		CachedTransformSpace == EKClonerModifierTransformSpace::World ||
		bIsActorRoot;

	if (bTrackWorld)
	{
		const FTransform CurrentWorld = InTarget->GetComponentTransform();
		if (!CurrentWorld.Equals(LastAppliedWorldTransform, 0.01f))
		{
			const FTransform ExternalDelta =
				CurrentWorld * LastAppliedWorldTransform.Inverse();
			BaseWorldTransform = ExternalDelta * BaseWorldTransform;

			if (CachedTransformSpace ==
				EKClonerModifierTransformSpace::World)
			{
				BaseTransform = BaseWorldTransform;
			}
		}
		return;
	}

	const FTransform CurrentLocal = GetTransformInSpace(
		InTarget, EKClonerModifierTransformSpace::ActorLocal);
	if (!CurrentLocal.Equals(LastAppliedTransform, 0.01f))
	{
		const FTransform ExternalDelta =
			CurrentLocal * LastAppliedTransform.Inverse();
		BaseTransform = ExternalDelta * BaseTransform;
		BaseWorldTransform = InTarget->GetComponentTransform();
	}
}

void UKClonerModifierComponent::RestoreBaseTransformInternal()
{
	UMeshComponent* Target = CachedTargetMesh.Get();
	if (!Target || !bHasCapturedBaseTransform)
	{
		bHasAppliedTransform = false;
		return;
	}

	AActor* Owner = GetOwner();
	const bool bIsActorRoot = Owner && Owner->GetRootComponent() == Target;

	if (CachedTransformSpace == EKClonerModifierTransformSpace::World ||
		bIsActorRoot)
	{
		SetTransformInSpace(Target, BaseWorldTransform,
			EKClonerModifierTransformSpace::World);
	}
	else
	{
		SetTransformInSpace(Target, BaseTransform,
			EKClonerModifierTransformSpace::ActorLocal);
	}

	ModifiedTransform = BaseTransform;
	LastAppliedTransform = BaseTransform;
	LastAppliedWorldTransform = Target->GetComponentTransform();
	bHasAppliedTransform = false;
}

void UKClonerModifierComponent::ApplyModifierStack(float DeltaTime)
{
	UMeshComponent* Target = CachedTargetMesh.Get();
	if (!Target || !bHasCapturedBaseTransform)
	{
		return;
	}

	AActor* Owner = GetOwner();
	const bool bIsActorRoot = Owner && Owner->GetRootComponent() == Target;
	const bool bEvaluateWorld =
		TransformSpace == EKClonerModifierTransformSpace::World;

	// Actor roots are evaluated from a stable world base. Setting a root mesh's
	// transform also changes the actor transform, so using the live actor as the
	// parent each frame would compound the modifier forever.
	FTransform WorkingTransform;
	if (bEvaluateWorld)
	{
		WorkingTransform = BaseWorldTransform;
	}
	else if (bIsActorRoot)
	{
		WorkingTransform = FTransform::Identity;
	}
	else
	{
		WorkingTransform = BaseTransform;
	}

	const FTransform ChannelBase = WorkingTransform;
	ModifierCustomData.SetNum(3);
	ModifierCustomData[0] = 1.0f;
	ModifierCustomData[1] = 1.0f;
	ModifierCustomData[2] = 1.0f;

	float ModifierTime = CurrentTime;
	const int32 SafeIndex = FMath::Max(0, InstanceIndex);
	const int32 SafeCount = FMath::Max(1, InstanceCount);

	for (UKClonerModifier* Modifier : Modifiers)
	{
		if (Modifier)
		{
			FKClonerExpressionContext Context;
			Context.Time = ModifierTime;
			Context.DeltaTime = DeltaTime;
			Context.Index = SafeIndex;
			Context.Count = SafeCount;
			Context.NormalizedIndex = SafeCount > 1
				? static_cast<float>(SafeIndex) /
					static_cast<float>(SafeCount - 1)
				: 0.0f;
			Modifier->ApplyModifierWithContext(WorkingTransform, SafeIndex,
				SafeCount, ModifierTime, ModifierCustomData, Context);
		}
	}

	ApplyActiveModifierEffects(WorkingTransform, CurrentTime, DeltaTime,
		SafeIndex, SafeCount);

	if (!bAffectLocation)
	{
		WorkingTransform.SetLocation(ChannelBase.GetLocation());
	}
	if (!bAffectRotation)
	{
		WorkingTransform.SetRotation(ChannelBase.GetRotation());
	}
	if (!bAffectScale)
	{
		WorkingTransform.SetScale3D(ChannelBase.GetScale3D());
	}

	FTransform DesiredWorldTransform = WorkingTransform;
	if (!bEvaluateWorld && !bIsActorRoot && Owner)
	{
		DesiredWorldTransform = Owner->GetActorTransform() * WorkingTransform;
	}
	else if (!bEvaluateWorld && bIsActorRoot)
	{
		DesiredWorldTransform = BaseWorldTransform * WorkingTransform;
	}

	Target->SetWorldTransform(DesiredWorldTransform, false, nullptr,
		ETeleportType::TeleportPhysics);

	ModifiedTransform = WorkingTransform;
	LastAppliedTransform = WorkingTransform;
	LastAppliedWorldTransform = Target->GetComponentTransform();
	bHasAppliedTransform = true;

	OnModifierUpdated.Broadcast(ModifiedTransform, DeltaTime, CurrentTime);
}

void UKClonerModifierComponent::ResetClock()
{
	ElapsedTime = StartTime;

	if (bUseWorldTime && GetWorld())
	{
		CurrentTime = GetWorld()->GetTimeSeconds() * TimeScale + TimeOffset;
	}
	else
	{
		CurrentTime = ElapsedTime + TimeOffset;
	}
}

void UKClonerModifierComponent::BuildRuntimeEffectModifiers(
	FActiveModifierEffect& Effect)
{
	for (UKClonerModifier* ExistingModifier : Effect.RuntimeModifiers)
	{
		for (int32 RefIndex = ActiveEffectModifierObjects.Num() - 1;
			RefIndex >= 0; --RefIndex)
		{
			if (ActiveEffectModifierObjects[RefIndex] == ExistingModifier)
			{
				ActiveEffectModifierObjects.RemoveAt(RefIndex);
				break;
			}
		}
	}
	Effect.RuntimeModifiers.Reset();
	if (!Effect.EffectPreset)
	{
		return;
	}

	ActiveEffectPresets.AddUnique(Effect.EffectPreset);
	Effect.SourceRevision = Effect.EffectPreset->SourceRevision;
	for (UKClonerModifier* SourceModifier : Effect.EffectPreset->Modifiers)
	{
		if (!SourceModifier)
		{
			continue;
		}

		if (UKClonerModifier* RuntimeModifier =
			DuplicateObject<UKClonerModifier>(SourceModifier, this))
		{
			Effect.RuntimeModifiers.Add(RuntimeModifier);
			ActiveEffectModifierObjects.Add(RuntimeModifier);
		}
	}
}

float UKClonerModifierComponent::GetEffectWeight(
	const FActiveModifierEffect& Effect) const
{
	if (!Effect.EffectPreset)
	{
		return 0.0f;
	}

	const UKClonerAnimationEffectPreset* Preset = Effect.EffectPreset;
	float Result = FMath::Clamp(Effect.Weight * Preset->Weight, 0.0f, 1.0f);

	if (!Effect.bEndRequested && Preset->BlendIn > KINDA_SMALL_NUMBER &&
		Effect.Age < Preset->BlendIn)
	{
		Result *= FMath::Clamp(Effect.Age / Preset->BlendIn, 0.0f, 1.0f);
	}

	if (Effect.bEndRequested)
	{
		if (Preset->BlendOut <= KINDA_SMALL_NUMBER)
		{
			return 0.0f;
		}

		const float EndAlpha =
			FMath::Clamp((Effect.Age - Effect.EndAge) / Preset->BlendOut,
				0.0f, 1.0f);
		Result *= 1.0f - EndAlpha;
	}

	return FMath::Clamp(Result, 0.0f, 1.0f);
}

void UKClonerModifierComponent::RemoveModifierEffectAt(int32 ArrayIndex)
{
	if (!ActiveModifierEffects.IsValidIndex(ArrayIndex))
	{
		return;
	}

	FActiveModifierEffect& Effect = ActiveModifierEffects[ArrayIndex];
	for (UKClonerModifier* RuntimeModifier : Effect.RuntimeModifiers)
	{
		for (int32 RefIndex = ActiveEffectModifierObjects.Num() - 1;
			RefIndex >= 0; --RefIndex)
		{
			if (ActiveEffectModifierObjects[RefIndex] == RuntimeModifier)
			{
				ActiveEffectModifierObjects.RemoveAt(RefIndex);
				break;
			}
		}
	}

	ActiveModifierEffects.RemoveAt(ArrayIndex);

	for (int32 PresetIndex = ActiveEffectPresets.Num() - 1;
		PresetIndex >= 0; --PresetIndex)
	{
		UKClonerAnimationEffectPreset* Candidate = ActiveEffectPresets[PresetIndex];
		const bool bStillUsed = ActiveModifierEffects.ContainsByPredicate(
			[Candidate](const FActiveModifierEffect& ActiveEffect)
			{
				return ActiveEffect.EffectPreset == Candidate;
			});
		if (!bStillUsed)
		{
			ActiveEffectPresets.RemoveAt(PresetIndex);
		}
	}
}

void UKClonerModifierComponent::UpdateModifierEffects(float DeltaTime)
{
	if (ActiveModifierEffects.Num() == 0)
	{
		return;
	}

	for (int32 Index = ActiveModifierEffects.Num() - 1; Index >= 0; --Index)
	{
		FActiveModifierEffect& Effect = ActiveModifierEffects[Index];
		if (!Effect.EffectPreset)
		{
			RemoveModifierEffectAt(Index);
			continue;
		}

		if (bPlaying)
		{
			Effect.Age += FMath::Max(0.0f, DeltaTime);
		}

		if (Effect.bAutoExpire && !Effect.bEndRequested &&
			Effect.Duration >= 0.0f && Effect.Age >= Effect.Duration)
		{
			Effect.bEndRequested = true;
			Effect.EndAge = Effect.Duration;
		}

		const float BlendOut = Effect.EffectPreset->BlendOut;
		if (Effect.bEndRequested &&
			(BlendOut <= KINDA_SMALL_NUMBER ||
			 Effect.Age >= Effect.EndAge + BlendOut))
		{
			RemoveModifierEffectAt(Index);
		}
	}
}

void UKClonerModifierComponent::ApplyActiveModifierEffects(
	FTransform& InOutTransform, float BaseTime, float DeltaTime,
	int32 SafeIndex, int32 SafeCount)
{
	if (ActiveModifierEffects.Num() == 0)
	{
		return;
	}

	ActiveModifierEffects.Sort(
		[](const FActiveModifierEffect& A, const FActiveModifierEffect& B)
		{
			if (A.Priority == B.Priority)
			{
				return A.Handle < B.Handle;
			}
			return A.Priority < B.Priority;
		});

	for (FActiveModifierEffect& Effect : ActiveModifierEffects)
	{
		if (Effect.EffectPreset &&
			Effect.SourceRevision != Effect.EffectPreset->SourceRevision)
		{
			BuildRuntimeEffectModifiers(Effect);
		}

		const float LayerWeight = GetEffectWeight(Effect);
		if (LayerWeight <= KINDA_SMALL_NUMBER || Effect.RuntimeModifiers.Num() == 0)
		{
			continue;
		}

		FTransform LayerTransform = InOutTransform;
		TArray<float> LayerCustomData;
		LayerCustomData.Init(1.0f, 3);
		float EffectTime = Effect.Age * Effect.EffectPreset->TimeScale;

		for (UKClonerModifier* Modifier : Effect.RuntimeModifiers)
		{
			if (Modifier)
			{
				FKClonerExpressionContext Context;
				Context.Time = EffectTime;
				Context.DeltaTime = DeltaTime;
				Context.Duration = Effect.Duration;
				Context.Index = SafeIndex;
				Context.Count = SafeCount;
				Context.NormalizedIndex = SafeCount > 1
					? static_cast<float>(SafeIndex) /
						static_cast<float>(SafeCount - 1)
					: 0.0f;
				Modifier->ApplyModifierWithContext(LayerTransform, SafeIndex,
					SafeCount, EffectTime, LayerCustomData, Context);
			}
		}

		InOutTransform.BlendWith(LayerTransform, LayerWeight);
	}
}

int32 UKClonerModifierComponent::BeginModifierEffect(
	UKClonerAnimationEffectPreset* EffectPreset, FName LayerName,
	float DurationOverride, float WeightOverride, float InitialAge)
{
	if (!EffectPreset)
	{
		return INDEX_NONE;
	}

	const FName ResolvedLayer =
		LayerName.IsNone() ? EffectPreset->DefaultLayer : LayerName;
	if (EffectPreset->bReplaceSameLayer && !ResolvedLayer.IsNone())
	{
		for (int32 Index = ActiveModifierEffects.Num() - 1; Index >= 0; --Index)
		{
			if (ActiveModifierEffects[Index].LayerName == ResolvedLayer)
			{
				ActiveModifierEffects[Index].bEndRequested = true;
				ActiveModifierEffects[Index].EndAge =
					ActiveModifierEffects[Index].Age;
			}
		}
	}

	FActiveModifierEffect Effect;
	Effect.Handle = NextModifierEffectHandle++;
	if (NextModifierEffectHandle == INDEX_NONE)
	{
		NextModifierEffectHandle = 1;
	}
	Effect.LayerName = ResolvedLayer;
	Effect.EffectPreset = EffectPreset;
	Effect.Duration = DurationOverride;
	Effect.Age = FMath::Max(0.0f, InitialAge);
	Effect.Weight = WeightOverride >= 0.0f ? WeightOverride : 1.0f;
	Effect.Priority = EffectPreset->Priority;
	BuildRuntimeEffectModifiers(Effect);
	ActiveModifierEffects.Add(MoveTemp(Effect));
	ApplyNow();
	return ActiveModifierEffects.Last().Handle;
}

int32 UKClonerModifierComponent::TriggerModifierEffect(
	UKClonerAnimationEffectPreset* EffectPreset, FName LayerName,
	float WeightOverride)
{
	const int32 Handle = BeginModifierEffect(
		EffectPreset, LayerName, EffectPreset ? EffectPreset->Duration : -1.0f,
		WeightOverride);
	if (Handle == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	for (FActiveModifierEffect& Effect : ActiveModifierEffects)
	{
		if (Effect.Handle == Handle)
		{
			Effect.bAutoExpire = true;
			break;
		}
	}
	return Handle;
}

void UKClonerModifierComponent::EndModifierEffect(int32 EffectHandle)
{
	for (int32 Index = ActiveModifierEffects.Num() - 1; Index >= 0; --Index)
	{
		FActiveModifierEffect& Effect = ActiveModifierEffects[Index];
		if (Effect.Handle != EffectHandle)
		{
			continue;
		}

		Effect.bEndRequested = true;
		Effect.EndAge = Effect.Age;
		if (!Effect.EffectPreset ||
			Effect.EffectPreset->BlendOut <= KINDA_SMALL_NUMBER)
		{
			RemoveModifierEffectAt(Index);
		}
		ApplyNow();
		return;
	}
}

void UKClonerModifierComponent::EndAllModifierEffects()
{
	for (FActiveModifierEffect& Effect : ActiveModifierEffects)
	{
		Effect.bEndRequested = true;
		Effect.EndAge = Effect.Age;
	}
	ApplyNow();
}

bool UKClonerModifierComponent::IsModifierEffectActive(int32 EffectHandle) const
{
	return ActiveModifierEffects.ContainsByPredicate(
		[EffectHandle](const FActiveModifierEffect& Effect)
		{
			return Effect.Handle == EffectHandle;
		});
}

void UKClonerModifierComponent::SetEnabled(bool bInEnabled)
{
	bEnabled = bInEnabled;

	if (!bEnabled && bRestoreWhenDisabled)
	{
		if (bHasAppliedTransform)
		{
			SyncBaseWithExternalTransform(CachedTargetMesh.Get());
			RestoreBaseTransformInternal();
		}
	}
	else if (bEnabled)
	{
		ApplyNow();
	}
}

void UKClonerModifierComponent::SetPlaying(bool bInPlaying)
{
	bPlaying = bInPlaying;
}

void UKClonerModifierComponent::Play()
{
	bPlaying = true;
}

void UKClonerModifierComponent::Pause()
{
	bPlaying = false;
}

void UKClonerModifierComponent::Restart(float InStartTime)
{
	ElapsedTime = InStartTime;

	if (bUseWorldTime && GetWorld())
	{
		TimeOffset =
			InStartTime - GetWorld()->GetTimeSeconds() * TimeScale;
		CurrentTime = InStartTime;
	}
	else
	{
		CurrentTime = ElapsedTime + TimeOffset;
	}

	if (bEnabled)
	{
		ApplyNow();
	}
}

void UKClonerModifierComponent::SetTargetMesh(UMeshComponent* InTargetMesh)
{
	if (TargetMesh == InTargetMesh)
	{
		return;
	}

	if (bHasAppliedTransform)
	{
		SyncBaseWithExternalTransform(CachedTargetMesh.Get());
		RestoreBaseTransformInternal();
	}

	TargetMesh = InTargetMesh;
	CachedTargetMesh.Reset();
	bHasCapturedBaseTransform = false;
	RefreshTargetAndBase();
}

UMeshComponent* UKClonerModifierComponent::GetTargetMesh() const
{
	return ResolveTargetMesh();
}

void UKClonerModifierComponent::CaptureBaseTransform()
{
	UMeshComponent* Target = ResolveTargetMesh();
	if (!Target)
	{
		return;
	}

	if (bHasAppliedTransform)
	{
		SyncBaseWithExternalTransform(CachedTargetMesh.Get());
		RestoreBaseTransformInternal();
	}

	CachedTargetMesh = Target;
	CachedTransformSpace = TransformSpace;
	BaseWorldTransform = Target->GetComponentTransform();
	BaseTransform = GetTransformInSpace(Target, TransformSpace);
	ModifiedTransform = BaseTransform;
	LastAppliedTransform = BaseTransform;
	LastAppliedWorldTransform = BaseWorldTransform;
	bHasCapturedBaseTransform = true;
	bHasAppliedTransform = false;
}

void UKClonerModifierComponent::ResetToBaseTransform()
{
	if (!bHasCapturedBaseTransform)
	{
		CaptureBaseTransform();
		return;
	}

	if (bHasAppliedTransform)
	{
		SyncBaseWithExternalTransform(CachedTargetMesh.Get());
	}

	RestoreBaseTransformInternal();
	OnModifierReset.Broadcast();
}

void UKClonerModifierComponent::ApplyNow()
{
	if (!GetWorld() && !GetOwner())
	{
		return;
	}

	RefreshTargetAndBase();
	if (!bEnabled || !CachedTargetMesh.IsValid())
	{
		return;
	}

	if (bHasAppliedTransform)
	{
		SyncBaseWithExternalTransform(CachedTargetMesh.Get());
	}

	ApplyModifierStack(0.0f);
}

UKClonerModifier* UKClonerModifierComponent::GetModifierByIndex(int32 Index) const
{
	return Modifiers.IsValidIndex(Index) ? Modifiers[Index] : nullptr;
}

UKClonerModifier* UKClonerModifierComponent::AddModifierOfClass(
	TSubclassOf<UKClonerModifier> ModifierClass)
{
	if (!ModifierClass)
	{
		return nullptr;
	}

	UKClonerModifier* NewModifier =
		NewObject<UKClonerModifier>(this, ModifierClass);
	if (NewModifier)
	{
		Modifiers.Add(NewModifier);
		ApplyNow();
	}

	return NewModifier;
}

bool UKClonerModifierComponent::RemoveModifier(UKClonerModifier* Modifier)
{
	if (!Modifier)
	{
		return false;
	}

	const int32 RemovedCount = Modifiers.Remove(Modifier);
	if (RemovedCount > 0)
	{
		ApplyNow();
		return true;
	}

	return false;
}

void UKClonerModifierComponent::ClearAllModifiers()
{
	Modifiers.Empty();
	ApplyNow();
}

void UKClonerModifierComponent::SetModifierEnabled(int32 Index, bool bInEnabled)
{
	if (Modifiers.IsValidIndex(Index) && Modifiers[Index])
	{
		Modifiers[Index]->bEnabled = bInEnabled;
		ApplyNow();
	}
}

#if WITH_EDITOR
void UKClonerModifierComponent::PostEditChangeProperty(
	FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName ==
			GET_MEMBER_NAME_CHECKED(UKClonerModifierComponent, TargetMesh) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(
				UKClonerModifierComponent, bAutoResolveTarget) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(
				UKClonerModifierComponent, TransformSpace))
	{
			RefreshTargetAndBase();
	}
	else if (PropertyName ==
			 GET_MEMBER_NAME_CHECKED(UKClonerModifierComponent, StartTime))
	{
		ResetClock();
	}
	else if (PropertyName ==
			 GET_MEMBER_NAME_CHECKED(UKClonerModifierComponent, bEnabled) &&
			!bEnabled && bRestoreWhenDisabled)
	{
		ResetToBaseTransform();
	}
}
#endif
