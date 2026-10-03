// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerModifierSourceImporter.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/IPluginManager.h"
#include "KClonerModifier.h"
#include "KClonerModifierPreset.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#define LOCTEXT_NAMESPACE "KClonerModifierSourceImporter"

FDelegateHandle FKClonerModifierSourceImporter::EngineInitDelegateHandle;

TArray<TWeakObjectPtr<UKClonerModifierPreset>>
	FKClonerModifierSourceImporter::ImportedPresets;

namespace
{
struct FKClonerSourceDefinition
{
	FString AssetName;
	FString DisplayName;
	FString Category;
	FString Description;
	FString PositionExpression;
	FString RotationExpression;
	FString ScaleExpression;
	FString KScript;
	TArray<FKClonerPresetVariable> Variables;
	float Step = 0.1f;
	float SpeedMultiplier = 1.0f;
	FString SourceFile;
	FString SourceId;
	int64 SourceTimestamp = 0;
};

bool TryGetStringAny(const TSharedPtr<FJsonObject>& Object,
	const TCHAR* KeyA, const TCHAR* KeyB, const TCHAR* KeyC, FString& OutValue)
{
	if (!Object.IsValid())
	{
		return false;
	}

	const TCHAR* Keys[] = {KeyA, KeyB, KeyC};
	for (const TCHAR* Key : Keys)
	{
		if (Key && Object->TryGetStringField(Key, OutValue))
		{
			return true;
		}
	}

	return false;
}

bool TryGetNumberAny(const TSharedPtr<FJsonObject>& Object,
	const TCHAR* KeyA, const TCHAR* KeyB, double& OutValue)
{
	if (!Object.IsValid())
	{
		return false;
	}

	if (KeyA && Object->TryGetNumberField(KeyA, OutValue))
	{
		return true;
	}
	if (KeyB && Object->TryGetNumberField(KeyB, OutValue))
	{
		return true;
	}

	return false;
}

FString SanitizeAssetName(FString Name)
{
	Name.TrimStartAndEndInline();
	if (Name.IsEmpty())
	{
		Name = TEXT("ExternalModifier");
	}

	for (TCHAR& Character : Name)
	{
		if (!(FChar::IsAlnum(Character) || Character == TEXT('_')))
		{
			Character = TEXT('_');
		}
	}

	if (!Name.IsEmpty() && FChar::IsDigit(Name[0]))
	{
		Name = TEXT("MP_") + Name;
	}

	return Name;
}

bool ParseDefinition(const TSharedPtr<FJsonObject>& Object,
	const FString& SourceFile, const FString& FallbackName, int32 DefinitionIndex,
	int64 SourceTimestamp, FKClonerSourceDefinition& OutDefinition)
{
	if (!Object.IsValid())
	{
		return false;
	}

	FString RawAssetName;
	TryGetStringAny(Object, TEXT("AssetName"), TEXT("Name"), TEXT("name"), RawAssetName);
	if (RawAssetName.IsEmpty())
	{
		RawAssetName = FallbackName;
	}
	if (RawAssetName.IsEmpty())
	{
		RawAssetName = FString::Printf(TEXT("ExternalModifier_%d"), DefinitionIndex);
	}

	OutDefinition.AssetName = SanitizeAssetName(RawAssetName);
	OutDefinition.DisplayName = OutDefinition.AssetName;
	TryGetStringAny(Object, TEXT("DisplayName"), TEXT("display_name"), TEXT("displayName"),
		OutDefinition.DisplayName);
	TryGetStringAny(Object, TEXT("Category"), TEXT("category"), nullptr,
		OutDefinition.Category);
	TryGetStringAny(Object, TEXT("Description"), TEXT("description"), nullptr,
		OutDefinition.Description);
	TryGetStringAny(Object, TEXT("PositionExpression"), TEXT("position_expression"),
		TEXT("position"), OutDefinition.PositionExpression);
	TryGetStringAny(Object, TEXT("RotationExpression"), TEXT("rotation_expression"),
		TEXT("rotation"), OutDefinition.RotationExpression);
	TryGetStringAny(Object, TEXT("ScaleExpression"), TEXT("scale_expression"),
		TEXT("scale"), OutDefinition.ScaleExpression);
	TryGetStringAny(Object, TEXT("KScript"), TEXT("kscript"), TEXT("Code"),
		OutDefinition.KScript);

	double NumberValue = 0.0;
	if (TryGetNumberAny(Object, TEXT("Step"), TEXT("step"), NumberValue))
	{
		OutDefinition.Step = static_cast<float>(NumberValue);
	}
	if (TryGetNumberAny(Object, TEXT("SpeedMultiplier"), TEXT("speed_multiplier"),
		NumberValue))
	{
		OutDefinition.SpeedMultiplier = static_cast<float>(NumberValue);
	}

	const TArray<TSharedPtr<FJsonValue>>* Variables = nullptr;
	if (Object->TryGetArrayField(TEXT("Variables"), Variables) ||
		Object->TryGetArrayField(TEXT("variables"), Variables))
	{
		for (const TSharedPtr<FJsonValue>& VariableValue : *Variables)
		{
			const TSharedPtr<FJsonObject> VariableObject =
				VariableValue.IsValid() ? VariableValue->AsObject() : nullptr;
			if (!VariableObject.IsValid())
			{
				continue;
			}

			FKClonerPresetVariable Variable;
			TryGetStringAny(VariableObject, TEXT("Name"), TEXT("name"), nullptr,
				Variable.Name);
			if (Variable.Name.IsEmpty())
			{
				Variable.Name = FString::Printf(TEXT("Parameter_%d"), OutDefinition.Variables.Num());
			}

			if (TryGetNumberAny(VariableObject, TEXT("DefaultValue"), TEXT("default"),
				NumberValue))
			{
				Variable.DefaultValue = static_cast<float>(NumberValue);
			}
			if (TryGetNumberAny(VariableObject, TEXT("MinValue"), TEXT("min"), NumberValue))
			{
				Variable.MinValue = static_cast<float>(NumberValue);
			}
			if (TryGetNumberAny(VariableObject, TEXT("MaxValue"), TEXT("max"), NumberValue))
			{
				Variable.MaxValue = static_cast<float>(NumberValue);
			}
			TryGetStringAny(VariableObject, TEXT("Tooltip"), TEXT("tooltip"), nullptr,
				Variable.Tooltip);

			OutDefinition.Variables.Add(Variable);
		}
	}

	if (OutDefinition.Category.IsEmpty())
	{
		OutDefinition.Category = TEXT("External");
	}

	OutDefinition.SourceFile = SourceFile;
	OutDefinition.SourceId = OutDefinition.AssetName;
	OutDefinition.SourceTimestamp = SourceTimestamp;
	return true;
}

bool ParseSourceFile(const FString& Filename,
	TArray<FKClonerSourceDefinition>& OutDefinitions)
{
	FString Contents;
	if (!FFileHelper::LoadFileToString(Contents, *Filename))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("K-Cloner: Could not read modifier source '%s'"), *Filename);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Contents);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("K-Cloner: Invalid modifier JSON '%s'"), *Filename);
		return false;
	}

	const FDateTime FileTimestamp = IFileManager::Get().GetTimeStamp(*Filename);
	const int64 Timestamp = FileTimestamp.ToUnixTimestamp();
	FString FallbackName = FPaths::GetBaseFilename(Filename);
	FallbackName.RemoveFromEnd(TEXT(".kmod"), ESearchCase::IgnoreCase);

	const TArray<TSharedPtr<FJsonValue>>* PresetValues = nullptr;
	if (RootObject->TryGetArrayField(TEXT("Presets"), PresetValues) ||
		RootObject->TryGetArrayField(TEXT("presets"), PresetValues))
	{
		int32 DefinitionIndex = 0;
		for (const TSharedPtr<FJsonValue>& PresetValue : *PresetValues)
		{
			const TSharedPtr<FJsonObject> PresetObject =
				PresetValue.IsValid() ? PresetValue->AsObject() : nullptr;
			FKClonerSourceDefinition Definition;
			if (ParseDefinition(PresetObject, Filename, FallbackName, DefinitionIndex,
				Timestamp, Definition))
			{
				OutDefinitions.Add(MoveTemp(Definition));
			}
			++DefinitionIndex;
		}
		return true;
	}

	FKClonerSourceDefinition Definition;
	if (ParseDefinition(RootObject, Filename, FallbackName, 0, Timestamp, Definition))
	{
		OutDefinitions.Add(MoveTemp(Definition));
		return true;
	}

	return false;
}

void AddSourceRoot(TArray<FString>& Roots, const FString& Root)
{
	if (!Root.IsEmpty() && FPaths::DirectoryExists(Root))
	{
		Roots.AddUnique(FPaths::ConvertRelativePathToFull(Root));
	}
}

TArray<FString> FindModifierSourceFiles()
{
	TArray<FString> Roots;
	AddSourceRoot(Roots, FPaths::Combine(FPaths::ProjectContentDir(), TEXT("KCloner/Modifiers")));
	AddSourceRoot(Roots, FPaths::Combine(FPaths::ProjectDir(), TEXT("KCloner/Modifiers")));

	if (const TSharedPtr<IPlugin> KClonerPlugin =
		IPluginManager::Get().FindPlugin(TEXT("KCloner")))
	{
		const FString PluginContent = KClonerPlugin->GetContentDir();
		AddSourceRoot(Roots, FPaths::Combine(PluginContent, TEXT("Modifiers")));
		AddSourceRoot(Roots, FPaths::Combine(PluginContent, TEXT("Presets")));
	}

	TArray<FString> Files;
	for (const FString& Root : Roots)
	{
		TArray<FString> RootFiles;
		IFileManager::Get().FindFilesRecursive(RootFiles, *Root, TEXT("*.json"),
			true, false);
		for (FString& File : RootFiles)
		{
			const FString LowerFile = File.ToLower();
			if (LowerFile.EndsWith(TEXT(".keffect.json")) ||
				LowerFile.EndsWith(TEXT(".kfx.json")))
			{
				continue;
			}
			Files.AddUnique(FPaths::ConvertRelativePathToFull(File));
		}
	}

	Files.Sort();
	return Files;
}

bool SavePresetAsset(UKClonerModifierPreset* Preset)
{
	if (!Preset)
	{
		return false;
	}

	UPackage* Package = Preset->GetOutermost();
	if (!Package)
	{
		return false;
	}

	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		Package->GetName(), FPackageName::GetAssetPackageExtension());
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(PackageFilename), true);

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	return UPackage::SavePackage(Package, Preset, *PackageFilename, SaveArgs);
}

UKClonerModifierPreset* FindOrCreatePresetAsset(
	const FKClonerSourceDefinition& Definition, bool& bOutCreated)
{
	bOutCreated = false;
	const FString PackageName = FString::Printf(
		TEXT("/Game/_Data/KCloner/ModifierPresets/%s"), *Definition.AssetName);
	const FString ObjectPath = PackageName + TEXT(".") + Definition.AssetName;

	if (FPackageName::DoesPackageExist(PackageName))
	{
		if (UKClonerModifierPreset* Existing =
			LoadObject<UKClonerModifierPreset>(nullptr, *ObjectPath))
		{
			return Existing;
		}
	}

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		return nullptr;
	}

	UKClonerModifierPreset* Preset = NewObject<UKClonerModifierPreset>(
		Package, UKClonerModifierPreset::StaticClass(),
		*Definition.AssetName, RF_Public | RF_Standalone);
	if (!Preset)
	{
		return nullptr;
	}

	FAssetRegistryModule::AssetCreated(Preset);
	bOutCreated = true;
	return Preset;
}

} // namespace

void FKClonerModifierSourceImporter::Startup()
{
	// Saving packages during module startup is unsafe: editor subsystems such as
	// DataValidation are not initialized yet. Defer the first import until the
	// engine has completed its startup delegate.
	EngineInitDelegateHandle = FCoreDelegates::OnFEngineLoopInitComplete.AddStatic(
		&FKClonerModifierSourceImporter::OnEngineLoopInitComplete);
}

void FKClonerModifierSourceImporter::Shutdown()
{
	if (EngineInitDelegateHandle.IsValid())
	{
		FCoreDelegates::OnFEngineLoopInitComplete.Remove(
			EngineInitDelegateHandle);
		EngineInitDelegateHandle.Reset();
	}
	ImportedPresets.Empty();
}

void FKClonerModifierSourceImporter::OnEngineLoopInitComplete()
{
	if (EngineInitDelegateHandle.IsValid())
	{
		FCoreDelegates::OnFEngineLoopInitComplete.Remove(
			EngineInitDelegateHandle);
		EngineInitDelegateHandle.Reset();
	}

	// Never auto-import during automation tests, unattended execution, headless runs, or commandlets.
	FString ExecCmds;
	const bool bHasExecCmds = FParse::Value(FCommandLine::Get(), TEXT("ExecCmds="), ExecCmds);
	const bool bIsAutomationOrHeadless = IsRunningCommandlet()
		|| GIsAutomationTesting
		|| FApp::IsUnattended()
		|| FParse::Param(FCommandLine::Get(), TEXT("NullRHI"))
		|| FParse::Param(FCommandLine::Get(), TEXT("NoKClonerGenerate"))
		|| (bHasExecCmds && ExecCmds.Contains(TEXT("Automation")));

	if (GIsEditor && !bIsAutomationOrHeadless)
	{
		ImportAll(false);
	}
}

void FKClonerModifierSourceImporter::Reload()
{
	ImportAll(true);
}

void FKClonerModifierSourceImporter::ImportAll(bool bForce)
{
	ImportedPresets.Empty();

	const TArray<FString> SourceFiles = FindModifierSourceFiles();
	TSet<FString> SeenSourceKeys;
	int32 ImportedCount = 0;
	int32 UpdatedCount = 0;
	int32 ErrorCount = 0;

	for (const FString& SourceFile : SourceFiles)
	{
		TArray<FKClonerSourceDefinition> Definitions;
		if (!ParseSourceFile(SourceFile, Definitions))
		{
			++ErrorCount;
			continue;
		}

		for (const FKClonerSourceDefinition& Definition : Definitions)
		{
			const FString SourceKey = Definition.SourceFile + TEXT("::") + Definition.SourceId;
			if (SeenSourceKeys.Contains(SourceKey))
			{
				UE_LOG(LogTemp, Warning,
					TEXT("K-Cloner: Duplicate modifier source '%s'"), *SourceKey);
				continue;
			}
			SeenSourceKeys.Add(SourceKey);

			bool bCreated = false;
			UKClonerModifierPreset* Preset =
				FindOrCreatePresetAsset(Definition, bCreated);
			if (!Preset)
			{
				++ErrorCount;
				continue;
			}

			const bool bSameSource =
				Preset->bImportedFromSource &&
				Preset->SourceFile == Definition.SourceFile &&
				Preset->SourceId == Definition.SourceId &&
				Preset->SourceTimestamp == Definition.SourceTimestamp;

			if (bForce || bCreated || !bSameSource)
			{
				Preset->Modify();
				Preset->DisplayName = Definition.DisplayName;
				Preset->Category = Definition.Category;
				Preset->Description = Definition.Description;
				Preset->PositionExpression = Definition.PositionExpression;
				Preset->RotationExpression = Definition.RotationExpression;
				Preset->ScaleExpression = Definition.ScaleExpression;
				Preset->KScript = Definition.KScript;
				Preset->Variables = Definition.Variables;
				Preset->Step = Definition.Step;
				Preset->SpeedMultiplier = Definition.SpeedMultiplier;
				Preset->bImportedFromSource = true;
				Preset->SourceFile = Definition.SourceFile;
				Preset->SourceId = Definition.SourceId;
				Preset->SourceTimestamp = Definition.SourceTimestamp;
				Preset->SourceRevision = FMath::Max(1, Preset->SourceRevision + 1);

				FString ValidationError;
				if (!Preset->ValidateExpressions(ValidationError))
				{
					UE_LOG(LogTemp, Warning,
						TEXT("K-Cloner: Imported preset '%s' has invalid expressions: %s"),
						*Preset->DisplayName, *ValidationError);
				}

				Preset->MarkPackageDirty();

				if (bCreated)
				{
					++ImportedCount;
				}
				else
				{
					++UpdatedCount;
				}

				if (!SavePresetAsset(Preset))
				{
					UE_LOG(LogTemp, Warning,
						TEXT("K-Cloner: Failed to save imported preset '%s'"),
						*Preset->GetPathName());
				}
			}

			ImportedPresets.Add(Preset);
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("K-Cloner: Modifier source reload complete (%d files, %d imported, %d updated, %d errors)"),
		SourceFiles.Num(), ImportedCount, UpdatedCount, ErrorCount);
}

void FKClonerModifierSourceImporter::BuildPresetMenu(
	FMenuBuilder& MenuBuilder,
	TFunction<void(UKClonerModifierPreset*)> OnPresetSelected)
{
	TMap<FString, TArray<UKClonerModifierPreset*>> PresetsByCategory;
	for (const TWeakObjectPtr<UKClonerModifierPreset>& WeakPreset : ImportedPresets)
	{
		UKClonerModifierPreset* Preset = WeakPreset.Get();
		if (Preset)
		{
			PresetsByCategory.FindOrAdd(Preset->Category).Add(Preset);
		}
	}

	TArray<FString> Categories;
	PresetsByCategory.GetKeys(Categories);
	Categories.Sort();

	MenuBuilder.BeginSection(
		FName(TEXT("KClonerExternalModifiers")),
		LOCTEXT("ExternalModifierSection", "External Modifiers"));

	if (Categories.Num() == 0)
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("NoExternalModifiers", "No external modifiers found"),
			LOCTEXT("NoExternalModifiersTooltip",
				"Add JSON files to Content/KCloner/Modifiers and reload them."),
			FSlateIcon(), FUIAction());
	}
	else
	{
		for (const FString& Category : Categories)
		{
			TArray<UKClonerModifierPreset*> CategoryPresets =
				PresetsByCategory.FindChecked(Category);
			CategoryPresets.Sort([](const UKClonerModifierPreset& A,
				const UKClonerModifierPreset& B)
			{
				return A.DisplayName < B.DisplayName;
			});

			MenuBuilder.AddSubMenu(
				FText::FromString(Category),
				LOCTEXT("ExternalModifierCategoryTooltip",
					"Add an imported K-Cloner modifier preset"),
				FNewMenuDelegate::CreateLambda(
					[CategoryPresets, OnPresetSelected](FMenuBuilder& SubMenu)
				{
					for (UKClonerModifierPreset* Preset : CategoryPresets)
					{
						if (!Preset)
						{
							continue;
						}

						SubMenu.AddMenuEntry(
							FText::FromString(Preset->DisplayName),
							FText::FromString(Preset->Description), FSlateIcon(),
							FUIAction(FExecuteAction::CreateLambda(
								[Preset, OnPresetSelected]()
								{
									OnPresetSelected(Preset);
								}))); 
					}
				}));
		}
	}

	MenuBuilder.EndSection();
	MenuBuilder.AddMenuEntry(
		LOCTEXT("ReloadExternalModifiers", "Reload External Modifiers"),
		LOCTEXT("ReloadExternalModifiersTooltip",
			"Rescan JSON modifier sources and update generated preset assets."),
		FSlateIcon(), FUIAction(FExecuteAction::CreateStatic(
			&FKClonerModifierSourceImporter::Reload)));
}

UKClonerModifier_Preset* FKClonerModifierSourceImporter::CreatePresetModifier(
	UObject* Outer, UKClonerModifierPreset* Preset)
{
	if (!Outer || !Preset)
	{
		return nullptr;
	}

	UKClonerModifier_Preset* Modifier =
		NewObject<UKClonerModifier_Preset>(Outer);
	if (Modifier)
	{
		Modifier->Preset = Preset;
	}
	return Modifier;
}

static FAutoConsoleCommand GReloadKClonerModifiersCommand(
	TEXT("KCloner.ReloadModifiers"),
	TEXT("Rescan external K-Cloner JSON modifier sources and update preset assets."),
	FConsoleCommandDelegate::CreateStatic(
		&FKClonerModifierSourceImporter::Reload));

#undef LOCTEXT_NAMESPACE
