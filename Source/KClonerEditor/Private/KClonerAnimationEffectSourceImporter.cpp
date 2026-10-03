// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerAnimationEffectSourceImporter.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/IPluginManager.h"
#include "KClonerAnimationEffectPreset.h"
#include "KClonerModifier.h"
#include "KClonerTypes.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Field.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#define LOCTEXT_NAMESPACE "KClonerAnimationEffectSourceImporter"

FDelegateHandle FKClonerAnimationEffectSourceImporter::EngineInitDelegateHandle;
TArray<TWeakObjectPtr<UKClonerAnimationEffectPreset>>
	FKClonerAnimationEffectSourceImporter::ImportedEffects;

namespace
{
struct FEffectSourceDefinition
{
	FString AssetName;
	FString DisplayName;
	FString Category;
	FString Description;
	FName DefaultLayer = TEXT("Animation");
	int32 Priority = 0;
	float Weight = 1.0f;
	float Duration = 0.35f;
	float BlendIn = 0.03f;
	float BlendOut = 0.15f;
	float TimeScale = 1.0f;
	bool bReplaceSameLayer = false;
	bool bSpawnDistributedCloner = false;
	bool bUseOwnerMesh = true;
	EKClonerSkeletalMode SkeletalMode = EKClonerSkeletalMode::PhysicsIK;
	FString SourceMeshPath;
	FString SourceSkeletalMeshPath;
	TArray<TSharedPtr<FJsonObject>> ModifierObjects;
	TArray<FKClonerDistributionLayer> DistributionLayers;
	FString SourceFile;
	FString SourceId;
	int64 SourceTimestamp = 0;
};

FString NormalizeToken(FString Value)
{
	Value = Value.ToLower();
	Value.ReplaceInline(TEXT("ukclonermodifier_"), TEXT(""));
	Value.ReplaceInline(TEXT("ukclonermodifier"), TEXT(""));
	Value.ReplaceInline(TEXT("modifier_"), TEXT(""));
	Value.ReplaceInline(TEXT("modifier"), TEXT(""));
	Value.ReplaceInline(TEXT("_"), TEXT(""));
	Value.ReplaceInline(TEXT("-"), TEXT(""));
	Value.ReplaceInline(TEXT(" "), TEXT(""));
	return Value;
}

FString SanitizeAnimationAssetName(FString Name)
{
	Name.TrimStartAndEndInline();
	if (Name.IsEmpty())
	{
		Name = TEXT("AnimationEffect");
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
		Name = TEXT("FX_") + Name;
	}
	return Name;
}

bool TryGetNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key,
	double& OutValue)
{
	return Object.IsValid() && Key && Object->TryGetNumberField(Key, OutValue);
}

bool TryGetBool(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key,
	bool& OutValue)
{
	return Object.IsValid() && Key && Object->TryGetBoolField(Key, OutValue);
}

bool TryGetString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key,
	FString& OutValue)
{
	return Object.IsValid() && Key && Object->TryGetStringField(Key, OutValue);
}

bool JsonNumber(const TSharedPtr<FJsonValue>& Value, double& OutValue)
{
	return Value.IsValid() && Value->TryGetNumber(OutValue);
}

bool ParseVector(const TSharedPtr<FJsonValue>& Value, FVector& OutVector)
{
	if (!Value.IsValid())
	{
		return false;
	}

	if (const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		Value->TryGetArray(Array) && Array && Array->Num() >= 3)
	{
		double X = 0.0;
		double Y = 0.0;
		double Z = 0.0;
		if (JsonNumber((*Array)[0], X) && JsonNumber((*Array)[1], Y) &&
			JsonNumber((*Array)[2], Z))
		{
			OutVector = FVector(static_cast<float>(X), static_cast<float>(Y),
				static_cast<float>(Z));
			return true;
		}
	}

	const TSharedPtr<FJsonObject> Object = Value->AsObject();
	if (!Object.IsValid())
	{
		return false;
	}

	double X = 0.0;
	double Y = 0.0;
	double Z = 0.0;
	const bool bHasX = Object->TryGetNumberField(TEXT("X"), X) ||
		Object->TryGetNumberField(TEXT("x"), X);
	const bool bHasY = Object->TryGetNumberField(TEXT("Y"), Y) ||
		Object->TryGetNumberField(TEXT("y"), Y);
	const bool bHasZ = Object->TryGetNumberField(TEXT("Z"), Z) ||
		Object->TryGetNumberField(TEXT("z"), Z);
	if (bHasX && bHasY && bHasZ)
	{
		OutVector = FVector(static_cast<float>(X), static_cast<float>(Y),
			static_cast<float>(Z));
		return true;
	}
	return false;
}

bool ParseIntVector(const TSharedPtr<FJsonValue>& Value, FIntVector& OutVector)
{
	FVector Parsed;
	if (!ParseVector(Value, Parsed))
	{
		return false;
	}
	OutVector = FIntVector(FMath::RoundToInt(Parsed.X),
		FMath::RoundToInt(Parsed.Y), FMath::RoundToInt(Parsed.Z));
	return true;
}

EKClonerMode ParseClonerMode(const FString& Value)
{
	const FString Token = NormalizeToken(Value);
	if (Token == TEXT("radial")) return EKClonerMode::Radial;
	if (Token == TEXT("linear")) return EKClonerMode::Linear;
	if (Token == TEXT("spline")) return EKClonerMode::Spline;
	if (Token == TEXT("single")) return EKClonerMode::Single;
	if (Token == TEXT("honeycomb")) return EKClonerMode::Honeycomb;
	if (Token == TEXT("scatter")) return EKClonerMode::Scatter;
	if (Token == TEXT("mesh") || Token == TEXT("surface")) return EKClonerMode::Mesh;
	if (Token == TEXT("arc") || Token == TEXT("fan")) return EKClonerMode::Arc;
	if (Token == TEXT("spiral") || Token == TEXT("helix")) return EKClonerMode::Spiral;
	if (Token == TEXT("sphere") || Token == TEXT("fibonacci")) return EKClonerMode::Sphere;
	if (Token == TEXT("cone") || Token == TEXT("frustum")) return EKClonerMode::Cone;
	if (Token == TEXT("poisson") || Token == TEXT("bluenoise")) return EKClonerMode::Poisson;
	return EKClonerMode::Grid;
}

EKClonerMeshSampleMode ParseSampleMode(const FString& Value)
{
	const FString Token = NormalizeToken(Value);
	if (Token == TEXT("vertex")) return EKClonerMeshSampleMode::Vertex;
	if (Token == TEXT("volume")) return EKClonerMeshSampleMode::Volume;
	return EKClonerMeshSampleMode::Surface;
}

EKClonerSkeletalMode ParseSkeletalMode(const FString& Value)
{
	const FString Token = NormalizeToken(Value);
	if (Token == TEXT("vat") || Token == TEXT("vatbaked") ||
		Token == TEXT("baked"))
	{
		return EKClonerSkeletalMode::VATBaked;
	}
	if (Token == TEXT("auto")) return EKClonerSkeletalMode::Auto;
	return EKClonerSkeletalMode::PhysicsIK;
}

void ParseDistributionLayer(const TSharedPtr<FJsonObject>& Object,
	FKClonerDistributionLayer& Layer)
{
	if (!Object.IsValid())
	{
		return;
	}

	bool BoolValue = false;
	double NumberValue = 0.0;
	FString StringValue;
	if (TryGetBool(Object, TEXT("Enabled"), BoolValue) ||
		TryGetBool(Object, TEXT("bEnabled"), BoolValue))
	{
		Layer.bEnabled = BoolValue;
	}
	if (TryGetString(Object, TEXT("Mode"), StringValue))
	{
		Layer.Mode = ParseClonerMode(StringValue);
	}

	const TSharedPtr<FJsonValue> GridCountValue =
		Object->TryGetField(TEXT("GridCount"));
	if (GridCountValue.IsValid())
		ParseIntVector(GridCountValue, Layer.GridCount);
	const TSharedPtr<FJsonValue> GridSpacingValue =
		Object->TryGetField(TEXT("GridSpacing"));
	if (GridSpacingValue.IsValid())
		ParseVector(GridSpacingValue, Layer.GridSpacing);
	const TSharedPtr<FJsonValue> HoneycombCountValue =
		Object->TryGetField(TEXT("HoneycombCount"));
	if (HoneycombCountValue.IsValid())
		ParseIntVector(HoneycombCountValue, Layer.HoneycombCount);
	const TSharedPtr<FJsonValue> ScatterBoundsValue =
		Object->TryGetField(TEXT("ScatterBounds"));
	if (ScatterBoundsValue.IsValid())
		ParseVector(ScatterBoundsValue, Layer.ScatterBounds);
	const TSharedPtr<FJsonValue> LinearOffsetValue =
		Object->TryGetField(TEXT("LinearOffset"));
	if (LinearOffsetValue.IsValid())
		ParseVector(LinearOffsetValue, Layer.LinearOffset);

	if (TryGetNumber(Object, TEXT("RadialCount"), NumberValue))
		Layer.RadialCount = static_cast<int32>(NumberValue);
	if (TryGetNumber(Object, TEXT("RadialRadius"), NumberValue))
		Layer.RadialRadius = static_cast<float>(NumberValue);
	if (TryGetBool(Object, TEXT("RadialAlign"), BoolValue) ||
		TryGetBool(Object, TEXT("bRadialAlign"), BoolValue))
		Layer.bRadialAlign = BoolValue;
	if (TryGetNumber(Object, TEXT("LinearCount"), NumberValue))
		Layer.LinearCount = static_cast<int32>(NumberValue);
	if (TryGetNumber(Object, TEXT("SplineCount"), NumberValue))
		Layer.SplineCount = static_cast<int32>(NumberValue);
	if (TryGetBool(Object, TEXT("SplineAlign"), BoolValue) ||
		TryGetBool(Object, TEXT("bSplineAlign"), BoolValue))
		Layer.bSplineAlign = BoolValue;
	if (TryGetNumber(Object, TEXT("HoneycombSize"), NumberValue))
		Layer.HoneycombSize = static_cast<float>(NumberValue);
	if (TryGetNumber(Object, TEXT("HoneycombCountX"), NumberValue))
		Layer.HoneycombCount.X = static_cast<int32>(NumberValue);
	if (TryGetNumber(Object, TEXT("HoneycombCountY"), NumberValue))
		Layer.HoneycombCount.Y = static_cast<int32>(NumberValue);
	if (TryGetNumber(Object, TEXT("ScatterCount"), NumberValue))
		Layer.ScatterCount = static_cast<int32>(NumberValue);
	if (TryGetNumber(Object, TEXT("ScatterSeed"), NumberValue))
		Layer.ScatterSeed = static_cast<int32>(NumberValue);
	if (TryGetNumber(Object, TEXT("MeshCount"), NumberValue))
		Layer.MeshCount = static_cast<int32>(NumberValue);
	if (TryGetString(Object, TEXT("MeshSampleMode"), StringValue))
		Layer.MeshSampleMode = ParseSampleMode(StringValue);
	if (TryGetNumber(Object, TEXT("MeshSeed"), NumberValue))
		Layer.MeshSeed = static_cast<int32>(NumberValue);

	const TSharedPtr<FJsonValue> ArcAxisValue = Object->TryGetField(TEXT("ArcAxis"));
	if (ArcAxisValue.IsValid()) ParseVector(ArcAxisValue, Layer.ArcAxis);
	if (TryGetNumber(Object, TEXT("ArcCount"), NumberValue))
		Layer.ArcCount = static_cast<int32>(NumberValue);
	if (TryGetNumber(Object, TEXT("ArcRadius"), NumberValue))
		Layer.ArcRadius = static_cast<float>(NumberValue);
	if (TryGetNumber(Object, TEXT("ArcStartAngle"), NumberValue))
		Layer.ArcStartAngle = static_cast<float>(NumberValue);
	if (TryGetNumber(Object, TEXT("ArcEndAngle"), NumberValue))
		Layer.ArcEndAngle = static_cast<float>(NumberValue);
	if (TryGetBool(Object, TEXT("ArcClosed"), BoolValue) ||
		TryGetBool(Object, TEXT("bArcClosed"), BoolValue))
		Layer.bArcClosed = BoolValue;
	if (TryGetBool(Object, TEXT("ArcAlign"), BoolValue) ||
		TryGetBool(Object, TEXT("bArcAlign"), BoolValue))
		Layer.bArcAlign = BoolValue;

	const TSharedPtr<FJsonValue> SpiralAxisValue = Object->TryGetField(TEXT("SpiralAxis"));
	if (SpiralAxisValue.IsValid()) ParseVector(SpiralAxisValue, Layer.SpiralAxis);
	if (TryGetNumber(Object, TEXT("SpiralCount"), NumberValue))
		Layer.SpiralCount = static_cast<int32>(NumberValue);
	if (TryGetNumber(Object, TEXT("SpiralInnerRadius"), NumberValue))
		Layer.SpiralInnerRadius = static_cast<float>(NumberValue);
	if (TryGetNumber(Object, TEXT("SpiralOuterRadius"), NumberValue))
		Layer.SpiralOuterRadius = static_cast<float>(NumberValue);
	if (TryGetNumber(Object, TEXT("SpiralTurns"), NumberValue))
		Layer.SpiralTurns = static_cast<float>(NumberValue);
	if (TryGetNumber(Object, TEXT("SpiralHeight"), NumberValue))
		Layer.SpiralHeight = static_cast<float>(NumberValue);
	if (TryGetNumber(Object, TEXT("SpiralPhase"), NumberValue))
		Layer.SpiralPhase = static_cast<float>(NumberValue);
	if (TryGetBool(Object, TEXT("SpiralAlign"), BoolValue) ||
		TryGetBool(Object, TEXT("bSpiralAlign"), BoolValue))
		Layer.bSpiralAlign = BoolValue;

	const TSharedPtr<FJsonValue> SphereAxisValue = Object->TryGetField(TEXT("SphereAxis"));
	if (SphereAxisValue.IsValid()) ParseVector(SphereAxisValue, Layer.SphereAxis);
	if (TryGetNumber(Object, TEXT("SphereCount"), NumberValue))
		Layer.SphereCount = static_cast<int32>(NumberValue);
	if (TryGetNumber(Object, TEXT("SphereRadius"), NumberValue))
		Layer.SphereRadius = static_cast<float>(NumberValue);
	if (TryGetNumber(Object, TEXT("SphereSeed"), NumberValue))
		Layer.SphereSeed = static_cast<int32>(NumberValue);
	if (TryGetBool(Object, TEXT("SphereVolume"), BoolValue) ||
		TryGetBool(Object, TEXT("bSphereVolume"), BoolValue))
		Layer.bSphereVolume = BoolValue;
	if (TryGetBool(Object, TEXT("SphereHemisphere"), BoolValue) ||
		TryGetBool(Object, TEXT("bSphereHemisphere"), BoolValue))
		Layer.bSphereHemisphere = BoolValue;
	if (TryGetBool(Object, TEXT("SphereAlign"), BoolValue) ||
		TryGetBool(Object, TEXT("bSphereAlign"), BoolValue))
		Layer.bSphereAlign = BoolValue;

	const TSharedPtr<FJsonValue> ConeAxisValue = Object->TryGetField(TEXT("ConeAxis"));
	if (ConeAxisValue.IsValid()) ParseVector(ConeAxisValue, Layer.ConeAxis);
	if (TryGetNumber(Object, TEXT("ConeCount"), NumberValue))
		Layer.ConeCount = static_cast<int32>(NumberValue);
	if (TryGetNumber(Object, TEXT("ConeRings"), NumberValue))
		Layer.ConeRings = static_cast<int32>(NumberValue);
	if (TryGetNumber(Object, TEXT("ConeHeight"), NumberValue))
		Layer.ConeHeight = static_cast<float>(NumberValue);
	if (TryGetNumber(Object, TEXT("ConeBaseRadius"), NumberValue))
		Layer.ConeBaseRadius = static_cast<float>(NumberValue);
	if (TryGetNumber(Object, TEXT("ConeTipRadius"), NumberValue))
		Layer.ConeTipRadius = static_cast<float>(NumberValue);
	if (TryGetNumber(Object, TEXT("ConeSeed"), NumberValue))
		Layer.ConeSeed = static_cast<int32>(NumberValue);
	if (TryGetBool(Object, TEXT("ConeVolume"), BoolValue) ||
		TryGetBool(Object, TEXT("bConeVolume"), BoolValue))
		Layer.bConeVolume = BoolValue;
	if (TryGetBool(Object, TEXT("ConeAlign"), BoolValue) ||
		TryGetBool(Object, TEXT("bConeAlign"), BoolValue))
		Layer.bConeAlign = BoolValue;

	const TSharedPtr<FJsonValue> PoissonBoundsValue = Object->TryGetField(TEXT("PoissonBounds"));
	if (PoissonBoundsValue.IsValid()) ParseVector(PoissonBoundsValue, Layer.PoissonBounds);
	if (TryGetNumber(Object, TEXT("PoissonCount"), NumberValue))
		Layer.PoissonCount = static_cast<int32>(NumberValue);
	if (TryGetNumber(Object, TEXT("PoissonMinimumDistance"), NumberValue))
		Layer.PoissonMinimumDistance = static_cast<float>(NumberValue);
	if (TryGetNumber(Object, TEXT("PoissonAttemptsPerPoint"), NumberValue))
		Layer.PoissonAttemptsPerPoint = static_cast<int32>(NumberValue);
	if (TryGetNumber(Object, TEXT("PoissonSeed"), NumberValue))
		Layer.PoissonSeed = static_cast<int32>(NumberValue);

	if (TryGetString(Object, TEXT("MeshAsset"), StringValue))
	{
		Layer.MeshAsset = LoadObject<UStaticMesh>(nullptr, *StringValue);
	}
}

UClass* FindModifierClass(const FString& TypeName)
{
	const FString Wanted = NormalizeToken(TypeName);
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Candidate = *It;
		if (!Candidate || Candidate->HasAnyClassFlags(CLASS_Abstract) ||
			!Candidate->IsChildOf(UKClonerModifier::StaticClass()))
		{
			continue;
		}

		const FString ClassToken = NormalizeToken(Candidate->GetName());
		const FString DisplayToken = NormalizeToken(
			Candidate->GetDisplayNameText().ToString());
		if (Wanted == ClassToken || Wanted == DisplayToken)
		{
			return Candidate;
		}
	}
	return nullptr;
}

FProperty* FindPropertyCaseInsensitive(UClass* Class, const FString& Name)
{
	for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::IncludeSuper);
		It; ++It)
	{
		if (It->GetName().Equals(Name, ESearchCase::IgnoreCase) ||
			It->GetAuthoredName().Equals(Name, ESearchCase::IgnoreCase))
		{
			return *It;
		}
	}
	return nullptr;
}

bool SetJsonProperty(UObject* Object, FProperty* Property,
	const TSharedPtr<FJsonValue>& Value)
{
	if (!Object || !Property || !Value.IsValid())
	{
		return false;
	}

	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
	if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
	{
		bool BoolValue = false;
		if (Value->TryGetBool(BoolValue))
		{
			BoolProperty->SetPropertyValue(ValuePtr, BoolValue);
			return true;
		}
	}

	if (FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
	{
		double NumberValue = 0.0;
		if (Value->TryGetNumber(NumberValue))
		{
			if (NumericProperty->IsFloatingPoint())
			{
				NumericProperty->SetFloatingPointPropertyValue(ValuePtr,
					NumberValue);
			}
			else
			{
				NumericProperty->SetIntPropertyValue(ValuePtr,
					static_cast<int64>(NumberValue));
			}
			return true;
		}
	}

	if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
	{
		FString EnumName;
		if (Value->TryGetString(EnumName))
		{
			if (int64 EnumValue = EnumProperty->GetEnum()->GetValueByNameString(
				EnumName); EnumValue != INDEX_NONE)
			{
				EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(
					ValuePtr, EnumValue);
				return true;
			}
		}
	}

	if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property);
		ByteProperty && ByteProperty->Enum)
	{
		FString EnumName;
		if (Value->TryGetString(EnumName))
		{
			const int64 EnumValue =
				ByteProperty->Enum->GetValueByNameString(EnumName);
			if (EnumValue != INDEX_NONE)
			{
				ByteProperty->SetPropertyValue(ValuePtr,
					static_cast<uint8>(EnumValue));
				return true;
			}
		}
	}

	if (FStrProperty* StringProperty = CastField<FStrProperty>(Property))
	{
		FString StringValue;
		if (Value->TryGetString(StringValue))
		{
			StringProperty->SetPropertyValue(ValuePtr, StringValue);
			return true;
		}
	}

	if (FNameProperty* NameProperty = CastField<FNameProperty>(Property))
	{
		FString StringValue;
		if (Value->TryGetString(StringValue))
		{
			NameProperty->SetPropertyValue(ValuePtr, FName(*StringValue));
			return true;
		}
	}

	if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		FVector VectorValue;
		if (StructProperty->Struct == TBaseStructure<FVector>::Get() &&
			ParseVector(Value, VectorValue))
		{
			*static_cast<FVector*>(ValuePtr) = VectorValue;
			return true;
		}
		if (StructProperty->Struct == TBaseStructure<FRotator>::Get() &&
			ParseVector(Value, VectorValue))
		{
			*static_cast<FRotator*>(ValuePtr) =
				FRotator(VectorValue.X, VectorValue.Y, VectorValue.Z);
			return true;
		}
		if (StructProperty->Struct == TBaseStructure<FLinearColor>::Get() &&
			ParseVector(Value, VectorValue))
		{
			*static_cast<FLinearColor*>(ValuePtr) = FLinearColor(
				VectorValue.X, VectorValue.Y, VectorValue.Z, 1.0f);
			return true;
		}
	}

	if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (Value->TryGetArray(Values) && Values &&
			ArrayProperty->Inner->IsA<FNumericProperty>())
		{
			FScriptArrayHelper Helper(ArrayProperty, ValuePtr);
			Helper.Resize(Values->Num());
			for (int32 Index = 0; Index < Values->Num(); ++Index)
			{
				double NumberValue = 0.0;
				if (JsonNumber((*Values)[Index], NumberValue))
				{
					FNumericProperty* InnerNumeric =
						CastField<FNumericProperty>(ArrayProperty->Inner);
					void* ElementPtr = Helper.GetRawPtr(Index);
					if (InnerNumeric->IsFloatingPoint())
						InnerNumeric->SetFloatingPointPropertyValue(ElementPtr,
							NumberValue);
					else
						InnerNumeric->SetIntPropertyValue(ElementPtr,
							static_cast<int64>(NumberValue));
				}
			}
			return true;
		}
	}

	if (FObjectPropertyBase* ObjectProperty =
		CastField<FObjectPropertyBase>(Property))
	{
		FString ObjectPath;
		if (Value->TryGetString(ObjectPath) && !ObjectPath.IsEmpty())
		{
			UObject* Loaded = LoadObject<UObject>(nullptr, *ObjectPath);
			if (Loaded && Loaded->IsA(ObjectProperty->PropertyClass))
			{
				ObjectProperty->SetObjectPropertyValue(ValuePtr, Loaded);
				return true;
			}
		}
	}

	return false;
}

void ConfigureModifier(UObject* ModifierObject,
	const TSharedPtr<FJsonObject>& Definition)
{
	if (!ModifierObject || !Definition.IsValid())
	{
		return;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair :
		Definition->Values)
	{
		if (Pair.Key.Equals(TEXT("Type"), ESearchCase::IgnoreCase) ||
			Pair.Key.Equals(TEXT("Name"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		if (FProperty* Property = FindPropertyCaseInsensitive(
			ModifierObject->GetClass(), Pair.Key))
		{
			if (!SetJsonProperty(ModifierObject, Property, Pair.Value))
			{
				UE_LOG(LogTemp, Warning,
					TEXT("K-Cloner: Could not assign effect modifier property '%s' on '%s'"),
					*Pair.Key, *ModifierObject->GetClass()->GetName());
			}
		}
	}
}

bool ParseEffectDefinition(const TSharedPtr<FJsonObject>& Object,
	const FString& SourceFile, const FString& FallbackName, int32 Index,
	int64 Timestamp, FEffectSourceDefinition& OutDefinition)
{
	if (!Object.IsValid())
	{
		return false;
	}

	FString AssetName;
	TryGetString(Object, TEXT("AssetName"), AssetName);
	if (AssetName.IsEmpty())
		TryGetString(Object, TEXT("Name"), AssetName);
	if (AssetName.IsEmpty())
		AssetName = FallbackName;
	if (AssetName.IsEmpty())
		AssetName = FString::Printf(TEXT("AnimationEffect_%d"), Index);
	OutDefinition.AssetName = SanitizeAnimationAssetName(AssetName);
	OutDefinition.DisplayName = OutDefinition.AssetName;
	TryGetString(Object, TEXT("DisplayName"), OutDefinition.DisplayName);
	TryGetString(Object, TEXT("Category"), OutDefinition.Category);
	TryGetString(Object, TEXT("Description"), OutDefinition.Description);

	FString LayerName;
	if (TryGetString(Object, TEXT("DefaultLayer"), LayerName) ||
		TryGetString(Object, TEXT("Layer"), LayerName))
	{
		OutDefinition.DefaultLayer = FName(*LayerName);
	}

	double NumberValue = 0.0;
	bool BoolValue = false;
	if (TryGetNumber(Object, TEXT("Priority"), NumberValue))
		OutDefinition.Priority = static_cast<int32>(NumberValue);
	if (TryGetNumber(Object, TEXT("Weight"), NumberValue))
		OutDefinition.Weight = static_cast<float>(NumberValue);
	if (TryGetNumber(Object, TEXT("Duration"), NumberValue))
		OutDefinition.Duration = static_cast<float>(NumberValue);
	if (TryGetNumber(Object, TEXT("BlendIn"), NumberValue))
		OutDefinition.BlendIn = static_cast<float>(NumberValue);
	if (TryGetNumber(Object, TEXT("BlendOut"), NumberValue))
		OutDefinition.BlendOut = static_cast<float>(NumberValue);
	if (TryGetNumber(Object, TEXT("TimeScale"), NumberValue))
		OutDefinition.TimeScale = static_cast<float>(NumberValue);
	if (TryGetBool(Object, TEXT("ReplaceSameLayer"), BoolValue) ||
		TryGetBool(Object, TEXT("bReplaceSameLayer"), BoolValue))
		OutDefinition.bReplaceSameLayer = BoolValue;
	if (TryGetBool(Object, TEXT("SpawnDistributedCloner"), BoolValue) ||
		TryGetBool(Object, TEXT("bSpawnDistributedCloner"), BoolValue))
		OutDefinition.bSpawnDistributedCloner = BoolValue;
	if (TryGetBool(Object, TEXT("UseOwnerMesh"), BoolValue) ||
		TryGetBool(Object, TEXT("bUseOwnerMesh"), BoolValue))
		OutDefinition.bUseOwnerMesh = BoolValue;

	FString SkeletalMode;
	if (TryGetString(Object, TEXT("SkeletalMode"), SkeletalMode))
		OutDefinition.SkeletalMode = ParseSkeletalMode(SkeletalMode);
	TryGetString(Object, TEXT("SourceMesh"), OutDefinition.SourceMeshPath);
	TryGetString(Object, TEXT("SourceSkeletalMesh"),
		OutDefinition.SourceSkeletalMeshPath);

	const TArray<TSharedPtr<FJsonValue>>* Modifiers = nullptr;
	if (Object->TryGetArrayField(TEXT("Modifiers"), Modifiers) && Modifiers)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Modifiers)
		{
			if (TSharedPtr<FJsonObject> ModifierObject =
				Value.IsValid() ? Value->AsObject() : nullptr)
			{
				OutDefinition.ModifierObjects.Add(ModifierObject);
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Layers = nullptr;
	if (Object->TryGetArrayField(TEXT("DistributionLayers"), Layers) ||
		Object->TryGetArrayField(TEXT("Distribution"), Layers))
	{
		if (Layers)
		{
			for (const TSharedPtr<FJsonValue>& Value : *Layers)
			{
				FKClonerDistributionLayer Layer;
				ParseDistributionLayer(
					Value.IsValid() ? Value->AsObject() : nullptr, Layer);
				OutDefinition.DistributionLayers.Add(Layer);
			}
		}
	}

	if (OutDefinition.Category.IsEmpty())
		OutDefinition.Category = TEXT("Animation");
	OutDefinition.SourceFile = SourceFile;
	OutDefinition.SourceId = OutDefinition.AssetName;
	OutDefinition.SourceTimestamp = Timestamp;
	return true;
}

bool ParseAnimationSourceFile(const FString& Filename,
	TArray<FEffectSourceDefinition>& OutDefinitions)
{
	FString Contents;
	if (!FFileHelper::LoadFileToString(Contents, *Filename))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("K-Cloner: Could not read effect source '%s'"), *Filename);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Contents);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("K-Cloner: Invalid animation effect JSON '%s'"), *Filename);
		return false;
	}

	const int64 Timestamp =
		IFileManager::Get().GetTimeStamp(*Filename).ToUnixTimestamp();
	FString FallbackName = FPaths::GetBaseFilename(Filename);
	FallbackName.RemoveFromEnd(TEXT(".keffect"), ESearchCase::IgnoreCase);
	FallbackName.RemoveFromEnd(TEXT(".kfx"), ESearchCase::IgnoreCase);

	const TArray<TSharedPtr<FJsonValue>>* Effects = nullptr;
	if (RootObject->TryGetArrayField(TEXT("Effects"), Effects) && Effects)
	{
		int32 Index = 0;
		for (const TSharedPtr<FJsonValue>& Value : *Effects)
		{
			FEffectSourceDefinition Definition;
			if (ParseEffectDefinition(Value.IsValid() ? Value->AsObject() : nullptr,
				Filename, FallbackName, Index, Timestamp, Definition))
			{
				OutDefinitions.Add(MoveTemp(Definition));
			}
			++Index;
		}
		return true;
	}

	FEffectSourceDefinition Definition;
	if (ParseEffectDefinition(RootObject, Filename, FallbackName, 0,
		Timestamp, Definition))
	{
		OutDefinitions.Add(MoveTemp(Definition));
		return true;
	}
	return false;
}

void AddAnimationSourceRoot(TArray<FString>& Roots, const FString& Root)
{
	if (!Root.IsEmpty() && FPaths::DirectoryExists(Root))
	{
		Roots.AddUnique(FPaths::ConvertRelativePathToFull(Root));
	}
}

TArray<FString> FindEffectSourceFiles()
{
	TArray<FString> Roots;
	AddAnimationSourceRoot(Roots,
		FPaths::Combine(FPaths::ProjectContentDir(), TEXT("KCloner/Modifiers")));
	AddAnimationSourceRoot(Roots,
		FPaths::Combine(FPaths::ProjectDir(), TEXT("KCloner/Modifiers")));

	if (const TSharedPtr<IPlugin> Plugin =
		IPluginManager::Get().FindPlugin(TEXT("KCloner")))
	{
		AddAnimationSourceRoot(Roots,
			FPaths::Combine(Plugin->GetContentDir(), TEXT("Modifiers")));
	}

	TArray<FString> Files;
	for (const FString& Root : Roots)
	{
		TArray<FString> RootFiles;
		IFileManager::Get().FindFilesRecursive(RootFiles, *Root,
			TEXT("*.json"), true, false);
		for (const FString& File : RootFiles)
		{
			const FString LowerFile = File.ToLower();
			if (!LowerFile.EndsWith(TEXT(".keffect.json")) &&
				!LowerFile.EndsWith(TEXT(".kfx.json")))
			{
				continue;
			}
			Files.AddUnique(FPaths::ConvertRelativePathToFull(File));
		}
	}
	Files.Sort();
	return Files;
}

bool SaveEffectAsset(UKClonerAnimationEffectPreset* Effect)
{
	if (!Effect || !Effect->GetOutermost())
		return false;

	const FString Filename = FPackageName::LongPackageNameToFilename(
		Effect->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	return UPackage::SavePackage(Effect->GetOutermost(), Effect, *Filename,
		SaveArgs);
}

UKClonerAnimationEffectPreset* FindOrCreateEffect(
	const FEffectSourceDefinition& Definition, bool& bCreated)
{
	bCreated = false;
	const FString PackageName = FString::Printf(
		TEXT("/Game/_Data/KCloner/AnimationEffects/%s"), *Definition.AssetName);
	const FString ObjectPath = PackageName + TEXT(".") + Definition.AssetName;

	if (FPackageName::DoesPackageExist(PackageName))
	{
		if (UKClonerAnimationEffectPreset* Existing =
			LoadObject<UKClonerAnimationEffectPreset>(nullptr, *ObjectPath))
		{
			return Existing;
		}
	}

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
		return nullptr;

	UKClonerAnimationEffectPreset* Effect =
		NewObject<UKClonerAnimationEffectPreset>(Package,
			UKClonerAnimationEffectPreset::StaticClass(),
			*Definition.AssetName, RF_Public | RF_Standalone);
	if (Effect)
	{
		FAssetRegistryModule::AssetCreated(Effect);
		bCreated = true;
	}
	return Effect;
}

} // namespace

void FKClonerAnimationEffectSourceImporter::Startup()
{
	EngineInitDelegateHandle = FCoreDelegates::OnFEngineLoopInitComplete.AddStatic(
		&FKClonerAnimationEffectSourceImporter::OnEngineLoopInitComplete);
}

void FKClonerAnimationEffectSourceImporter::Shutdown()
{
	if (EngineInitDelegateHandle.IsValid())
	{
		FCoreDelegates::OnFEngineLoopInitComplete.Remove(
			EngineInitDelegateHandle);
		EngineInitDelegateHandle.Reset();
	}
	ImportedEffects.Empty();
}

void FKClonerAnimationEffectSourceImporter::OnEngineLoopInitComplete()
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

void FKClonerAnimationEffectSourceImporter::Reload()
{
	ImportAll(true);
}

void FKClonerAnimationEffectSourceImporter::ImportAll(bool bForce)
{
	ImportedEffects.Empty();
	const TArray<FString> SourceFiles = FindEffectSourceFiles();
	TSet<FString> SeenSourceKeys;
	int32 ImportedCount = 0;
	int32 UpdatedCount = 0;
	int32 ErrorCount = 0;

	for (const FString& SourceFile : SourceFiles)
	{
		TArray<FEffectSourceDefinition> Definitions;
		if (!ParseAnimationSourceFile(SourceFile, Definitions))
		{
			++ErrorCount;
			continue;
		}

		for (const FEffectSourceDefinition& Definition : Definitions)
		{
			const FString SourceKey = Definition.SourceFile + TEXT("::") +
				Definition.SourceId;
			if (SeenSourceKeys.Contains(SourceKey))
			{
				UE_LOG(LogTemp, Warning,
					TEXT("K-Cloner: Duplicate animation effect source '%s'"),
					*SourceKey);
				continue;
			}
			SeenSourceKeys.Add(SourceKey);

			bool bCreated = false;
			UKClonerAnimationEffectPreset* Effect =
				FindOrCreateEffect(Definition, bCreated);
			if (!Effect)
			{
				++ErrorCount;
				continue;
			}

			const bool bSameSource = Effect->bImportedFromSource &&
				Effect->SourceFile == Definition.SourceFile &&
				Effect->SourceId == Definition.SourceId &&
				Effect->SourceTimestamp == Definition.SourceTimestamp;

			if (bForce || bCreated || !bSameSource)
			{
				Effect->Modify();
				Effect->DisplayName = Definition.DisplayName;
				Effect->Category = Definition.Category;
				Effect->Description = Definition.Description;
				Effect->DefaultLayer = Definition.DefaultLayer;
				Effect->Priority = Definition.Priority;
				Effect->Weight = Definition.Weight;
				Effect->Duration = Definition.Duration;
				Effect->BlendIn = Definition.BlendIn;
				Effect->BlendOut = Definition.BlendOut;
				Effect->TimeScale = Definition.TimeScale;
				Effect->bReplaceSameLayer = Definition.bReplaceSameLayer;
				Effect->bSpawnDistributedCloner =
					Definition.bSpawnDistributedCloner;
				Effect->bUseOwnerMesh = Definition.bUseOwnerMesh;
				Effect->SkeletalMode = Definition.SkeletalMode;
				Effect->SourceMeshOverride =
					LoadObject<UStaticMesh>(nullptr, *Definition.SourceMeshPath);
				Effect->SourceSkeletalMeshOverride =
					LoadObject<USkeletalMesh>(nullptr,
						*Definition.SourceSkeletalMeshPath);
				Effect->DistributionLayers = Definition.DistributionLayers;
				Effect->Modifiers.Empty();

				for (int32 ModifierIndex = 0;
					ModifierIndex < Definition.ModifierObjects.Num();
					++ModifierIndex)
				{
					const TSharedPtr<FJsonObject>& ModifierDefinition =
						Definition.ModifierObjects[ModifierIndex];
					FString TypeName;
					if (!TryGetString(ModifierDefinition, TEXT("Type"), TypeName))
					{
						TryGetString(ModifierDefinition, TEXT("Class"), TypeName);
					}
					UClass* ModifierClass = FindModifierClass(TypeName);
					if (!ModifierClass)
					{
						UE_LOG(LogTemp, Warning,
							TEXT("K-Cloner: Unknown effect modifier type '%s' in '%s'"),
							*TypeName, *Definition.SourceFile);
						continue;
					}

					const FName ModifierName(*FString::Printf(
						TEXT("EffectModifier_%d"), ModifierIndex));
					UKClonerModifier* Modifier = NewObject<UKClonerModifier>(
						Effect, ModifierClass, ModifierName,
						RF_Transactional);
					if (Modifier)
					{
						ConfigureModifier(Modifier, ModifierDefinition);
						Effect->Modifiers.Add(Modifier);
					}
				}

				Effect->bImportedFromSource = true;
				Effect->SourceFile = Definition.SourceFile;
				Effect->SourceId = Definition.SourceId;
				Effect->SourceTimestamp = Definition.SourceTimestamp;
				Effect->SourceRevision = FMath::Max(1, Effect->SourceRevision + 1);
				Effect->MarkPackageDirty();

				if (bCreated)
					++ImportedCount;
				else
					++UpdatedCount;

				if (!SaveEffectAsset(Effect))
				{
					UE_LOG(LogTemp, Warning,
						TEXT("K-Cloner: Failed to save animation effect '%s'"),
						*Effect->GetPathName());
				}
			}

			ImportedEffects.Add(Effect);
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("K-Cloner: Animation effect reload complete (%d files, %d imported, %d updated, %d errors)"),
		SourceFiles.Num(), ImportedCount, UpdatedCount, ErrorCount);
}

void FKClonerAnimationEffectSourceImporter::BuildEffectMenu(
	FMenuBuilder& MenuBuilder,
	TFunction<void(UKClonerAnimationEffectPreset*)> OnEffectSelected)
{
	TMap<FString, TArray<UKClonerAnimationEffectPreset*>> EffectsByCategory;
	for (const TWeakObjectPtr<UKClonerAnimationEffectPreset>& WeakEffect :
		ImportedEffects)
	{
		if (UKClonerAnimationEffectPreset* Effect = WeakEffect.Get())
		{
			EffectsByCategory.FindOrAdd(Effect->Category).Add(Effect);
		}
	}

	TArray<FString> Categories;
	EffectsByCategory.GetKeys(Categories);
	Categories.Sort();
	MenuBuilder.BeginSection(FName(TEXT("KClonerAnimationEffects")),
		LOCTEXT("AnimationEffectsSection", "Animation Effects"));

	if (Categories.Num() == 0)
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("NoAnimationEffects", "No external animation effects found"),
			LOCTEXT("NoAnimationEffectsTip",
				"Add *.keffect.json files to Content/KCloner/Modifiers and reload."),
			FSlateIcon(), FUIAction());
	}
	else
	{
		for (const FString& Category : Categories)
		{
			const TArray<UKClonerAnimationEffectPreset*> Effects =
				EffectsByCategory.FindChecked(Category);
			MenuBuilder.AddSubMenu(FText::FromString(Category),
				LOCTEXT("AnimationEffectsCategoryTip", "Add an animation effect"),
				FNewMenuDelegate::CreateLambda(
					[Effects, OnEffectSelected](FMenuBuilder& SubMenu)
				{
						for (UKClonerAnimationEffectPreset* Effect : Effects)
						{
							if (!Effect) continue;
							SubMenu.AddMenuEntry(
								FText::FromString(Effect->DisplayName),
								FText::FromString(Effect->Description), FSlateIcon(),
								FUIAction(FExecuteAction::CreateLambda(
									[Effect, OnEffectSelected]()
									{
										OnEffectSelected(Effect);
									})));
						}
					}));
		}
	}

	MenuBuilder.EndSection();
	MenuBuilder.AddMenuEntry(
		LOCTEXT("ReloadAnimationEffects", "Reload External Animation Effects"),
		LOCTEXT("ReloadAnimationEffectsTip",
			"Rescan *.keffect.json and *.kfx.json source files."),
		FSlateIcon(), FUIAction(FExecuteAction::CreateStatic(
			&FKClonerAnimationEffectSourceImporter::Reload)));
}

static FAutoConsoleCommand GReloadKClonerAnimationEffectsCommand(
	TEXT("KCloner.ReloadAnimationEffects"),
	TEXT("Rescan external K-Cloner animation effect JSON sources."),
	FConsoleCommandDelegate::CreateStatic(
		&FKClonerAnimationEffectSourceImporter::Reload));

#undef LOCTEXT_NAMESPACE
