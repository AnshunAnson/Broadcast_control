#pragma once

#include "CoreMinimal.h"
#include "VPBroadcastTypes.generated.h"

UENUM(BlueprintType)
enum class EVPPropertyType : uint8
{
	Float  UMETA(DisplayName = "float"),
	Bool   UMETA(DisplayName = "bool"),
	Color  UMETA(DisplayName = "color"),
	Event  UMETA(DisplayName = "event"),
};

USTRUCT(BlueprintType)
struct FVPPropertyMeta
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "VPBroadcast")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "VPBroadcast")
	EVPPropertyType Type = EVPPropertyType::Float;

	UPROPERTY(BlueprintReadOnly, Category = "VPBroadcast")
	float Default = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VPBroadcast")
	float MinValue = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VPBroadcast")
	float MaxValue = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VPBroadcast")
	TArray<FString> Tags;

	UPROPERTY(BlueprintReadOnly, Category = "VPBroadcast")
	FString Description;

	UPROPERTY(BlueprintReadOnly, Category = "VPBroadcast")
	FString Category;

	UPROPERTY(BlueprintReadOnly, Category = "VPBroadcast")
	int32 ParamHash = 0;
};

namespace VPBroadcastConst
{
	inline const FString AgentCategoryTag = TEXT("VPAgent");
	inline const FString SchemaUploadEndpoint = TEXT("http://127.0.0.1:8000/schema/upload");
	inline constexpr int32 DefaultUDPListenPort = 7000;
	inline constexpr float TickInterval = 0.016f;
}

FORCEINLINE uint32 FNV1aHash32(const FString& InString)
{
	constexpr uint32 FnvOffset = 0x811C9DC5u;
	constexpr uint32 FnvPrime  = 0x01000193u;

	uint32 Hash = FnvOffset;
	const auto& Data = InString.GetCharArray();
	for (int32 i = 0; i < Data.Num(); ++i)
	{
		if (Data[i] == 0) break;
		uint8 Byte = static_cast<uint8>(Data[i]);
		Hash ^= Byte;
		Hash *= FnvPrime;
	}
	return Hash;
}