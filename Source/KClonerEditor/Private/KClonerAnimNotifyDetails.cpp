// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerAnimNotifyDetails.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "KClonerAnimNotify.h"
#include "KClonerAnimationEffectPreset.h"
#include "KClonerAnimationEffectSourceImporter.h"
#include "KClonerModifier.h"
#include "KClonerModifierPreset.h"
#include "KClonerModifierSourceImporter.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "FKClonerAnimNotifyDetails"

TSharedRef<IDetailCustomization> FKClonerAnimNotifyDetails::MakeInstance()
{
	return MakeShareable(new FKClonerAnimNotifyDetails);
}

UObject* FKClonerAnimNotifyDetails::GetSelectedObject() const
{
	return SelectedObject.Get();
}

UKClonerAnimationEffectPreset* FKClonerAnimNotifyDetails::GetEffectPreset() const
{
	if (const UAnimNotify_KClonerEvent* Event =
		Cast<UAnimNotify_KClonerEvent>(GetSelectedObject()))
	{
		return Event->EffectPreset;
	}
	if (const UAnimNotifyState_KClonerEvent* State =
		Cast<UAnimNotifyState_KClonerEvent>(GetSelectedObject()))
	{
		return State->EffectPreset;
	}
	return nullptr;
}

void FKClonerAnimNotifyDetails::SetEffectPreset(
	UKClonerAnimationEffectPreset* EffectPreset)
{
	if (UAnimNotify_KClonerEvent* Event =
		Cast<UAnimNotify_KClonerEvent>(GetSelectedObject()))
	{
		Event->Modify();
		Event->EffectPreset = EffectPreset;
		Event->MarkPackageDirty();
		return;
	}
	if (UAnimNotifyState_KClonerEvent* State =
		Cast<UAnimNotifyState_KClonerEvent>(GetSelectedObject()))
	{
		State->Modify();
		State->EffectPreset = EffectPreset;
		State->MarkPackageDirty();
	}
}

void FKClonerAnimNotifyDetails::AddExternalModifier(
	UKClonerModifierPreset* Preset)
{
	UObject* NotifyObject = GetSelectedObject();
	if (!NotifyObject || !Preset)
	{
		return;
	}

	FScopedTransaction Transaction(
		LOCTEXT("AddNotifyModifier", "Add K-Cloner Modifier to Notify"));
	NotifyObject->Modify();
	if (UKClonerModifier_Preset* Modifier =
		FKClonerModifierSourceImporter::CreatePresetModifier(NotifyObject,
			Preset))
	{
		if (UAnimNotify_KClonerEvent* Event =
			Cast<UAnimNotify_KClonerEvent>(NotifyObject))
		{
			Event->InlineModifiers.Add(Modifier);
			Event->bUseInlineModifiers = true;
		}
		else if (UAnimNotifyState_KClonerEvent* State =
			Cast<UAnimNotifyState_KClonerEvent>(NotifyObject))
		{
			State->InlineModifiers.Add(Modifier);
			State->bUseInlineModifiers = true;
		}
		NotifyObject->MarkPackageDirty();
	}
}

void FKClonerAnimNotifyDetails::AddBuiltInModifier(UClass* ModifierClass)
{
	UObject* NotifyObject = GetSelectedObject();
	if (!NotifyObject || !ModifierClass ||
		!ModifierClass->IsChildOf(UKClonerModifier::StaticClass()))
	{
		return;
	}

	FScopedTransaction Transaction(
		LOCTEXT("AddBuiltInNotifyModifier", "Add K-Cloner Modifier to Notify"));
	NotifyObject->Modify();
	UKClonerModifier* Modifier = NewObject<UKClonerModifier>(
		NotifyObject, ModifierClass,
		MakeUniqueObjectName(NotifyObject, ModifierClass,
			TEXT("InlineModifier")), RF_Transactional);
	if (!Modifier)
	{
		return;
	}

	if (UAnimNotify_KClonerEvent* Event =
		Cast<UAnimNotify_KClonerEvent>(NotifyObject))
	{
		Event->InlineModifiers.Add(Modifier);
		Event->bUseInlineModifiers = true;
	}
	else if (UAnimNotifyState_KClonerEvent* State =
		Cast<UAnimNotifyState_KClonerEvent>(NotifyObject))
	{
		State->InlineModifiers.Add(Modifier);
		State->bUseInlineModifiers = true;
	}
	NotifyObject->MarkPackageDirty();
}

void FKClonerAnimNotifyDetails::AddDistributionLayer(EKClonerMode Mode)
{
	UObject* NotifyObject = GetSelectedObject();
	if (!NotifyObject)
	{
		return;
	}

	FKClonerDistributionLayer Layer;
	Layer.Mode = Mode;
	if (Mode == EKClonerMode::Radial)
	{
		Layer.RadialCount = 8;
		Layer.RadialRadius = 60.0f;
	}

	NotifyObject->Modify();
	if (UAnimNotify_KClonerEvent* Event =
		Cast<UAnimNotify_KClonerEvent>(NotifyObject))
	{
		Event->InlineDistributionLayers.Add(Layer);
		Event->bUseInlineDistribution = true;
		Event->bSpawnDistributedCloner = true;
	}
	else if (UAnimNotifyState_KClonerEvent* State =
		Cast<UAnimNotifyState_KClonerEvent>(NotifyObject))
	{
		State->InlineDistributionLayers.Add(Layer);
		State->bUseInlineDistribution = true;
		State->bSpawnDistributedCloner = true;
	}
	NotifyObject->MarkPackageDirty();
}

void FKClonerAnimNotifyDetails::CopyEffectToInlineStack()
{
	UObject* NotifyObject = GetSelectedObject();
	UKClonerAnimationEffectPreset* Effect = GetEffectPreset();
	if (!NotifyObject || !Effect)
	{
		return;
	}

	FScopedTransaction Transaction(
		LOCTEXT("CopyEffectToNotify", "Copy K-Cloner Effect to Notify"));
	NotifyObject->Modify();

	auto CopyStack = [NotifyObject, Effect](
		TArray<UKClonerModifier*>& InlineModifiers,
		TArray<FKClonerDistributionLayer>& InlineDistributionLayers,
		bool& bUseInlineModifiers, bool& bUseInlineDistribution,
		bool& bSpawnDistributedCloner)
	{
		InlineModifiers.Empty();
		for (UKClonerModifier* SourceModifier : Effect->Modifiers)
		{
			if (SourceModifier)
			{
				if (UKClonerModifier* Copy =
					DuplicateObject<UKClonerModifier>(SourceModifier, NotifyObject))
				{
					InlineModifiers.Add(Copy);
				}
			}
		}
		InlineDistributionLayers = Effect->DistributionLayers;
		bUseInlineModifiers = true;
		bUseInlineDistribution = !InlineDistributionLayers.IsEmpty();
		bSpawnDistributedCloner = Effect->bSpawnDistributedCloner;
	};

	if (UAnimNotify_KClonerEvent* Event =
		Cast<UAnimNotify_KClonerEvent>(NotifyObject))
	{
		CopyStack(Event->InlineModifiers, Event->InlineDistributionLayers,
			Event->bUseInlineModifiers, Event->bUseInlineDistribution,
			Event->bSpawnDistributedCloner);
	}
	else if (UAnimNotifyState_KClonerEvent* State =
		Cast<UAnimNotifyState_KClonerEvent>(NotifyObject))
	{
		CopyStack(State->InlineModifiers, State->InlineDistributionLayers,
			State->bUseInlineModifiers, State->bUseInlineDistribution,
			State->bSpawnDistributedCloner);
	}

	NotifyObject->MarkPackageDirty();
}

void FKClonerAnimNotifyDetails::EnableInlineDistribution()
{
	if (UAnimNotify_KClonerEvent* Event =
		Cast<UAnimNotify_KClonerEvent>(GetSelectedObject()))
	{
		Event->Modify();
		Event->bUseInlineDistribution = true;
		if (Event->InlineDistributionLayers.IsEmpty())
		{
			FKClonerDistributionLayer Layer;
			Layer.Mode = EKClonerMode::Radial;
			Layer.RadialCount = 8;
			Layer.RadialRadius = 60.0f;
			Event->InlineDistributionLayers.Add(Layer);
		}
		Event->bSpawnDistributedCloner = true;
		Event->MarkPackageDirty();
	}
	else if (UAnimNotifyState_KClonerEvent* State =
		Cast<UAnimNotifyState_KClonerEvent>(GetSelectedObject()))
	{
		State->Modify();
		State->bUseInlineDistribution = true;
		if (State->InlineDistributionLayers.IsEmpty())
		{
			FKClonerDistributionLayer Layer;
			Layer.Mode = EKClonerMode::Radial;
			Layer.RadialCount = 8;
			Layer.RadialRadius = 60.0f;
			State->InlineDistributionLayers.Add(Layer);
		}
		State->bSpawnDistributedCloner = true;
		State->MarkPackageDirty();
	}
}

void FKClonerAnimNotifyDetails::CustomizeDetails(
	IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() != 1)
	{
		return;
	}

	SelectedObject = Objects[0];
	if (!Cast<UAnimNotify_KClonerEvent>(SelectedObject.Get()) &&
		!Cast<UAnimNotifyState_KClonerEvent>(SelectedObject.Get()))
	{
		return;
	}

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(
		"KCloner Event", LOCTEXT("KClonerEventCategory", "K-Cloner Event"),
		ECategoryPriority::Important);

	Category.AddCustomRow(LOCTEXT("EffectLibrary", "Effect Library"))
		.WholeRowContent()
		[
			SNew(SComboButton)
			.OnGetMenuContent(FOnGetContent::CreateLambda([this]()
			{
				FMenuBuilder MenuBuilder(true, nullptr);
				FKClonerAnimationEffectSourceImporter::BuildEffectMenu(
					MenuBuilder,
					[this](UKClonerAnimationEffectPreset* Effect)
					{
						SetEffectPreset(Effect);
						});
				return MenuBuilder.MakeWidget();
			}))
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ChooseEffect", "Choose Animation Effect"))
			]
		];

	Category.AddCustomRow(LOCTEXT("BuiltInModifierLibrary", "Built In Modifiers"))
		.WholeRowContent()
		[
			SNew(SComboButton)
			.OnGetMenuContent(FOnGetContent::CreateLambda([this]()
			{
				FMenuBuilder MenuBuilder(true, nullptr);
				TArray<UClass*> ModifierClasses;
				for (TObjectIterator<UClass> It; It; ++It)
				{
					UClass* Class = *It;
					if (Class && !Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated) &&
						Class->IsChildOf(UKClonerModifier::StaticClass()))
					{
						ModifierClasses.Add(Class);
					}
				}
				ModifierClasses.Sort([](const UClass& A, const UClass& B)
				{
					return A.GetDisplayNameText().ToString() <
						B.GetDisplayNameText().ToString();
				});
				for (UClass* Class : ModifierClasses)
				{
					MenuBuilder.AddMenuEntry(
						Class->GetDisplayNameText(),
						FText::Format(LOCTEXT("AddBuiltInModifierTip", "Add {0} to this notify"),
							Class->GetDisplayNameText()), FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda([this, Class]()
						{
							AddBuiltInModifier(Class);
						})));
				}
				return MenuBuilder.MakeWidget();
			}))
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AddBuiltInModifier", "Add Built-In Modifier"))
			]
		];

	Category.AddCustomRow(LOCTEXT("ModifierLibrary", "Modifier Library"))
		.WholeRowContent()
		[
			SNew(SComboButton)
			.OnGetMenuContent(FOnGetContent::CreateLambda([this]()
			{
				FMenuBuilder MenuBuilder(true, nullptr);
				FKClonerModifierSourceImporter::BuildPresetMenu(
					MenuBuilder,
					[this](UKClonerModifierPreset* Preset)
					{
						AddExternalModifier(Preset);
					});
				return MenuBuilder.MakeWidget();
			}))
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AddInlineModifier", "Add Modifier from Library"))
			]
		];

	Category.AddCustomRow(LOCTEXT("CopyInline", "Copy Preset Inline"))
		.WholeRowContent()
		[
			SNew(SButton)
			.OnClicked_Lambda([this]()
			{
				CopyEffectToInlineStack();
				return FReply::Handled();
			})
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CopyInlineButton", "Copy Selected Preset to Inline Stack"))
			]
		];

	Category.AddCustomRow(LOCTEXT("DistributionLibrary", "Distribution"))
		.WholeRowContent()
		[
			SNew(SButton)
			.OnClicked_Lambda([this]()
			{
				EnableInlineDistribution();
				return FReply::Handled();
			})
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("EnableDistribution", "Add / Enable Radial Distribution"))
			]
		];
}

#undef LOCTEXT_NAMESPACE
