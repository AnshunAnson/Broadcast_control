#include "VPAgentMetadata.h"
#include "Json.h"
#include "JsonUtilities.h"

TArray<FVPPropertyMeta> FVPAgentMetadataScanner::ScanBlueprintProperties(UClass* InClass)
{
	TArray<FVPPropertyMeta> Results;

	if (!InClass)
	{
		return Results;
	}

	for (TFieldIterator<FProperty> It(InClass, EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		FProperty* Property = *It;

		if (!Property->HasMetaData(TEXT("Category")))
		{
			continue;
		}

		FString CategoryValue = Property->GetMetaData(TEXT("Category"));
		if (!CategoryValue.Contains(VPBroadcastConst::AgentCategoryTag))
		{
			continue;
		}

		FVPPropertyMeta Meta;
		Meta.Name = Property->GetName();
		Meta.Category = CategoryValue;

		Meta.MinValue = 0.0f;
		Meta.MaxValue = 1.0f;
		Meta.Default = 0.0f;

		if (Property->HasMetaData(TEXT("ClampMin")))
		{
			Meta.MinValue = FCString::Atof(*Property->GetMetaData(TEXT("ClampMin")));
		}
		if (Property->HasMetaData(TEXT("ClampMax")))
		{
			Meta.MaxValue = FCString::Atof(*Property->GetMetaData(TEXT("ClampMax")));
		}

		if (Property->HasMetaData(TEXT("VPA_Min")))
		{
			Meta.MinValue = FCString::Atof(*Property->GetMetaData(TEXT("VPA_Min")));
		}
		if (Property->HasMetaData(TEXT("VPA_Max")))
		{
			Meta.MaxValue = FCString::Atof(*Property->GetMetaData(TEXT("VPA_Max")));
		}

		if (Property->HasMetaData(TEXT("VPA_Tags")))
		{
			FString TagsStr = Property->GetMetaData(TEXT("VPA_Tags"));
			TagsStr.ParseIntoArray(Meta.Tags, TEXT(","), true);
			for (FString& Tag : Meta.Tags)
			{
				Tag = Tag.TrimStartAndEnd();
			}
		}

		if (Property->HasMetaData(TEXT("VPA_Description")))
		{
			Meta.Description = Property->GetMetaData(TEXT("VPA_Description"));
		}

		if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
		{
			Meta.Type = EVPPropertyType::Event;
			Meta.Default = 0.0f;
		}
		else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
		{
			Meta.Type = EVPPropertyType::Bool;
			bool* BoolValue = Property->ContainerPtrToValuePtr<bool>(InClass->GetDefaultObject());
			Meta.Default = (BoolValue && *BoolValue) ? 1.0f : 0.0f;
		}
		else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			if (StructProp->Struct->GetFName() == NAME_LinearColor ||
				StructProp->Struct->GetFName() == NAME_Color)
			{
				Meta.Type = EVPPropertyType::Color;
				Meta.Default = 1.0f;
			}
			else
			{
				Meta.Type = EVPPropertyType::Float;
			}
		}
		else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
		{
			Meta.Type = EVPPropertyType::Float;
			float* FloatValue = Property->ContainerPtrToValuePtr<float>(InClass->GetDefaultObject());
			Meta.Default = FloatValue ? *FloatValue : 0.0f;
		}
		else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
		{
			Meta.Type = EVPPropertyType::Float;
			double* DoubleValue = Property->ContainerPtrToValuePtr<double>(InClass->GetDefaultObject());
			Meta.Default = DoubleValue ? static_cast<float>(*DoubleValue) : 0.0f;
		}
		else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
		{
			Meta.Type = EVPPropertyType::Float;
			int32* IntValue = Property->ContainerPtrToValuePtr<int32>(InClass->GetDefaultObject());
			Meta.Default = IntValue ? static_cast<float>(*IntValue) : 0.0f;
		}
		else
		{
			continue;
		}

		if (!Meta.MinValue && !Meta.MaxValue && Meta.MinValue == Meta.MaxValue)
		{
			Meta.MinValue = 0.0f;
			Meta.MaxValue = 1.0f;
		}

		Meta.ParamHash = static_cast<int32>(FNV1aHash32(Meta.Name));

		Results.Add(Meta);
	}

	return Results;
}

TArray<FVPPropertyMeta> FVPAgentMetadataScanner::ScanAllAgentProperties()
{
	TArray<FVPPropertyMeta> AllProperties;

	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;

		if (!Class || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}

		if (!Class->IsChildOf(AActor::StaticClass()) && !Class->IsChildOf(UActorComponent::StaticClass()))
		{
			continue;
		}

		if (Class->GetDefaultObject() == nullptr)
		{
			continue;
		}

		bool bAnyAgentProperty = false;
		for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::IncludeSuper); It; ++It)
		{
			if ((*It)->HasMetaData(TEXT("Category")))
			{
				FString CatValue = (*It)->GetMetaData(TEXT("Category"));
				if (CatValue.Contains(VPBroadcastConst::AgentCategoryTag))
				{
					bAnyAgentProperty = true;
					break;
				}
			}
		}

		if (!bAnyAgentProperty)
		{
			continue;
		}

		TArray<FVPPropertyMeta> ClassProps = ScanBlueprintProperties(Class);
		AllProperties.Append(ClassProps);
	}

	return AllProperties;
}

FString FVPAgentMetadataScanner::GenerateSchemaJson(
	const TArray<FVPPropertyMeta>& Properties,
	const FString& Source,
	int32 Version)
{
	TSharedRef<FJsonObject> RootObj = MakeShared<FJsonObject>();
	RootObj->SetStringField(TEXT("source"), Source);
	RootObj->SetNumberField(TEXT("version"), Version);

	TArray<TSharedPtr<FJsonValue>> ParamArray;
	for (const FVPPropertyMeta& Meta : Properties)
	{
		TSharedRef<FJsonObject> ParamObj = MakeShared<FJsonObject>();
		ParamObj->SetStringField(TEXT("name"), Meta.Name);
		ParamObj->SetNumberField(TEXT("default"), Meta.Default);
		ParamObj->SetNumberField(TEXT("min_value"), Meta.MinValue);
		ParamObj->SetNumberField(TEXT("max_value"), Meta.MaxValue);
		ParamObj->SetStringField(TEXT("description"), Meta.Description);
		ParamObj->SetStringField(TEXT("category"), Meta.Category);

		FString TypeStr;
		switch (Meta.Type)
		{
		case EVPPropertyType::Bool:  TypeStr = TEXT("bool"); break;
		case EVPPropertyType::Color: TypeStr = TEXT("color"); break;
		case EVPPropertyType::Event: TypeStr = TEXT("event"); break;
		default:                     TypeStr = TEXT("float"); break;
		}
		ParamObj->SetStringField(TEXT("type"), TypeStr);

		TArray<TSharedPtr<FJsonValue>> TagsArray;
		for (const FString& Tag : Meta.Tags)
		{
			TagsArray.Add(MakeShared<FJsonValueString>(Tag));
		}
		ParamObj->SetArrayField(TEXT("tags"), TagsArray);

		ParamArray.Add(MakeShared<FJsonValueObject>(ParamObj));
	}

	RootObj->SetArrayField(TEXT("parameters"), ParamArray);

	FString OutputJson;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutputJson);
	FJsonSerializer::Serialize(RootObj, Writer);
	Writer->Close();

	return OutputJson;
}
