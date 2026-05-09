#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VPBroadcastTypes.h"
#include "VPStageUDPReceiver.h"
#include "VPBroadcastSubsystem.generated.h"

class FHttpServerModule;
class IHttpRouter;

UCLASS(Config=Game)
class VPBROADCAST_API UVPBroadcastSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "VPBroadcast")
	void StartUDPReceiver(int32 Port = 0);

	UFUNCTION(BlueprintCallable, Category = "VPBroadcast")
	void StopUDPReceiver();

	UFUNCTION(BlueprintCallable, Category = "VPBroadcast")
	void ReportSchemaToHub();

	UFUNCTION(BlueprintCallable, Category = "VPBroadcast")
	void ScanAndMapAgentActors();

	UFUNCTION(BlueprintCallable, Category = "VPBroadcast")
	int32 GetParameterCount() const { return PropertyMetaArray.Num(); }

protected:
	void ApplyControlCommand(int32 ParamHash, float Value);
	void ProcessCommandQueue();

	bool bUDPReceiverRunning;
	TUniquePtr<FVPStageUDPReceiver> UDPReceiver;
	TQueue<FVPStageCommand, EQueueMode::Mpsc> CommandQueue;

	TArray<FVPPropertyMeta> PropertyMetaArray;
	TMap<int32, FProperty*> HashToPropertyMap;
	TMap<int32, UObject*> HashToOwnerMap;

	FTimerHandle PollTimerHandle;
};
