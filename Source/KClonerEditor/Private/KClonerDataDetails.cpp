// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerDataDetails.h"
#include "KClonerData.h"
#include "KClonerAnimBakeUtils.h"
#include "KClonerModifier.h"
#include "KClonerModifierSourceImporter.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FKClonerDataDetails"

void FKClonerDataDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() == 1)
	{
		SelectedData = Cast<UKClonerData>(Objects[0].Get());
	}

	// Only show buttons if we have a valid selection
	if (!SelectedData.IsValid()) return;

	IDetailCategoryBuilder& ModifierCategory = DetailBuilder.EditCategory(
		"Modifiers", LOCTEXT("ModifierLibraryCategory", "Modifiers"),
		ECategoryPriority::Important);
	TWeakObjectPtr<UKClonerData> WeakData = SelectedData;
	ModifierCategory.AddCustomRow(LOCTEXT("ExternalModifierSearch", "External Modifier"))
		.WholeRowContent()
		[
			SNew(SComboButton)
			.OnGetMenuContent(FOnGetContent::CreateLambda([WeakData]()
			{
				FMenuBuilder MenuBuilder(true, nullptr);
				FKClonerModifierSourceImporter::BuildPresetMenu(
					MenuBuilder,
					[WeakData](UKClonerModifierPreset* Preset)
					{
						UKClonerData* Data = WeakData.Get();
						if (!Data || !Preset)
						{
							return;
						}

						FScopedTransaction Transaction(
							LOCTEXT("AddExternalModifier",
								"Add External K-Cloner Modifier"));
						Data->Modify();
						if (UKClonerModifier_Preset* Modifier =
							FKClonerModifierSourceImporter::CreatePresetModifier(
								Data, Preset))
						{
							Data->Modifiers.Add(Modifier);
							Data->MarkPackageDirty();
						}
					});
				return MenuBuilder.MakeWidget();
			}))
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AddExternalModifierButton",
					"Add from Modifier Library"))
			]
		];

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("AnimTweak");
	
	Category.AddCustomRow(LOCTEXT("BakeRow", "Bake"))
		.WholeRowContent()
		.HAlign(HAlign_Center)
		[
			SNew(SButton)
			.OnClicked(this, &FKClonerDataDetails::OnBakeAnimationClicked)
			.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda([this]()
			{
				// Only enable if Tweak Mode is on and we have an animation
				return SelectedData.IsValid() && SelectedData->bAnimTweakMode && SelectedData->SourceAnimSequence != nullptr;
			})))
			.Content()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(5.f, 0.f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BakeButton", "Bake Animation from Preset"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
		];
}

FReply FKClonerDataDetails::OnBakeAnimationClicked()
{
	if (SelectedData.IsValid())
	{
		UKClonerAnimBakeUtils::BakeAnimSequenceFromData(SelectedData.Get());
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
