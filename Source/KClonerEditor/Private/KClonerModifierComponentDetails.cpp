// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerModifierComponentDetails.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "KClonerModifierComponent.h"
#include "KClonerModifierSourceImporter.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FKClonerModifierComponentDetails"

TSharedRef<IDetailCustomization> FKClonerModifierComponentDetails::MakeInstance()
{
	return MakeShareable(new FKClonerModifierComponentDetails);
}

void FKClonerModifierComponentDetails::CustomizeDetails(
	IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	if (ObjectsBeingCustomized.Num() == 1)
	{
		SelectedComponent =
			Cast<UKClonerModifierComponent>(ObjectsBeingCustomized[0].Get());
	}

	if (!SelectedComponent.IsValid())
	{
		return;
	}

	IDetailCategoryBuilder& LibraryCategory = DetailBuilder.EditCategory(
		"Modifier|Stack", LOCTEXT("ModifierLibraryCategory", "Modifier Stack"),
		ECategoryPriority::Important);

	TWeakObjectPtr<UKClonerModifierComponent> WeakComponent = SelectedComponent;
	LibraryCategory.AddCustomRow(
		LOCTEXT("ExternalModifierSearch", "External Modifier"))
		.WholeRowContent()
		[
			SNew(SComboButton)
			.OnGetMenuContent(FOnGetContent::CreateLambda([WeakComponent]()
			{
				FMenuBuilder MenuBuilder(true, nullptr);
				FKClonerModifierSourceImporter::BuildPresetMenu(
					MenuBuilder,
					[WeakComponent](UKClonerModifierPreset* Preset)
					{
						UKClonerModifierComponent* Component = WeakComponent.Get();
						if (!Component || !Preset)
						{
							return;
						}

						FScopedTransaction Transaction(
							LOCTEXT("AddExternalModifier",
								"Add External K-Cloner Modifier"));
						Component->Modify();
						if (UKClonerModifier_Preset* Modifier =
							FKClonerModifierSourceImporter::CreatePresetModifier(
								Component, Preset))
						{
							Component->Modifiers.Add(Modifier);
							Component->ApplyNow();
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
}

#undef LOCTEXT_NAMESPACE
