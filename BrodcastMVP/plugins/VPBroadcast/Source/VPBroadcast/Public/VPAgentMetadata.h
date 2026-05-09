#pragma once

#include "CoreMinimal.h"
#include "VPBroadcastTypes.h"

class VPBROADCAST_API FVPAgentMetadataScanner
{
public:
	static TArray<FVPPropertyMeta> ScanBlueprintProperties(UClass* InClass);

	static TArray<FVPPropertyMeta> ScanAllAgentProperties();

	static FString GenerateSchemaJson(
		const TArray<FVPPropertyMeta>& Properties,
		const FString& Source = TEXT("ue5"),
		int32 Version = 1
	);
};
