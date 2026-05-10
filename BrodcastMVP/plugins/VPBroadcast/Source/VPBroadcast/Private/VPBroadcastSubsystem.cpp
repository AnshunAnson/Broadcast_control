#include "VPBroadcastSubsystem.h"

#include "VPAgentMetadata.h"
#include "VPAgentComponent.h"
#include "VPStageEffectActor.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "TimerManager.h"

bool UVPBroadcastSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return true;
}

void UVPBroadcastSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	bUDPReceiverRunning = false;
	UDPReceiver.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("VPBroadcastSubsystem: No world available during initialization"));
		return;
	}

	World->GetTimerManager().SetTimer(
		PollTimerHandle,
		this,
		&UVPBroadcastSubsystem::ProcessCommandQueue,
		VPBroadcastConst::TickInterval,
		true
	);

	ScanAndMapAgentActors();
	StartUDPReceiver();
	ReportSchemaToHub();

	UE_LOG(LogTemp, Log, TEXT("VPBroadcastSubsystem: Initialized. Params: %d | Mapped: %d"),
		PropertyMetaArray.Num(), HashToPropertyMap.Num());
}

void UVPBroadcastSubsystem::Deinitialize()
{
	StopUDPReceiver();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PollTimerHandle);
	}

	Super::Deinitialize();
}

void UVPBroadcastSubsystem::StartUDPReceiver(int32 Port)
{
	if (bUDPReceiverRunning)
	{
		return;
	}

	if (Port <= 0)
	{
		Port = VPBroadcastConst::DefaultUDPListenPort;
	}

	UDPReceiver = MakeUnique<FVPStageUDPReceiver>();
	if (!UDPReceiver->Start(Port, &CommandQueue))
	{
		UE_LOG(LogTemp, Error, TEXT("VPBroadcastSubsystem: Failed to start UDP receiver on port %d"), Port);
		UDPReceiver.Reset();
		return;
	}

	bUDPReceiverRunning = true;
	UE_LOG(LogTemp, Log, TEXT("VPBroadcastSubsystem: UDP receiver started on port %d"), Port);
}

void UVPBroadcastSubsystem::StopUDPReceiver()
{
	if (!bUDPReceiverRunning || !UDPReceiver)
	{
		return;
	}

	UDPReceiver->Shutdown();
	UDPReceiver.Reset();
	bUDPReceiverRunning = false;
	UE_LOG(LogTemp, Log, TEXT("VPBroadcastSubsystem: UDP receiver stopped"));
}

void UVPBroadcastSubsystem::ReportSchemaToHub()
{
	UE_LOG(LogTemp, Log, TEXT("VPBroadcastSubsystem: Scanning agent properties for schema report..."));

	TArray<FVPPropertyMeta> ScannedProperties = FVPAgentMetadataScanner::ScanAllAgentProperties();
	PropertyMetaArray = ScannedProperties;

	FString SchemaJson = FVPAgentMetadataScanner::GenerateSchemaJson(ScannedProperties, TEXT("ue5"), 1);

	UE_LOG(LogTemp, Log, TEXT("VPBroadcastSubsystem: Generated schema JSON (%d bytes)"), SchemaJson.Len());

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(VPBroadcastConst::SchemaUploadEndpoint);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(SchemaJson);

	HttpRequest->OnProcessRequestComplete().BindLambda(
		[](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
		{
			if (bSuccess && Response.IsValid())
			{
				UE_LOG(LogTemp, Log, TEXT("VPBroadcastSubsystem: Schema uploaded. Code: %d"),
					Response->GetResponseCode());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("VPBroadcastSubsystem: Schema upload failed."));
			}
		}
	);

	HttpRequest->ProcessRequest();
}

void UVPBroadcastSubsystem::ScanAndMapAgentActors()
{
	PropertyMetaArray = FVPAgentMetadataScanner::ScanAllAgentProperties();
	HashToPropertyMap.Reset();
	HashToOwnerMap.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("VPBroadcastSubsystem: No world, skipping actor scan"));
		return;
	}

	int32 MappedActors = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor))
		{
			continue;
		}

		UClass* ActorClass = Actor->GetClass();

		for (TFieldIterator<FProperty> PropIt(ActorClass, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;

			if (!Property->HasMetaData(TEXT("Category")))
			{
				continue;
			}

			FString CatValue = Property->GetMetaData(TEXT("Category"));
			if (!CatValue.Contains(VPBroadcastConst::AgentCategoryTag))
			{
				continue;
			}

			int32 Hash = static_cast<int32>(FNV1aHash32(Property->GetName()));
			HashToPropertyMap.Add(Hash, Property);
			HashToOwnerMap.Add(Hash, Actor);

			UE_LOG(LogTemp, Verbose, TEXT("  Mapped: %s -> Hash 0x%08X -> %s"),
				*Property->GetName(), Hash, *Actor->GetName());
		}

		MappedActors++;
	}

	UE_LOG(LogTemp, Log, TEXT("VPBroadcastSubsystem: Scan complete. Actors: %d | Properties: %d | Mapped: %d"),
		MappedActors, PropertyMetaArray.Num(), HashToPropertyMap.Num());
}

void UVPBroadcastSubsystem::ApplyControlCommand(int32 ParamHash, float Value)
{
	FProperty** PropPtr = HashToPropertyMap.Find(ParamHash);
	if (!PropPtr || !*PropPtr)
	{
		return;
	}

	FProperty* Property = *PropPtr;
	UObject** OwnerPtr = HashToOwnerMap.Find(ParamHash);
	UObject* Target = OwnerPtr ? *OwnerPtr : nullptr;
	if (!Target)
	{
		return;
	}

	if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		FloatProp->SetPropertyValue_InContainer(Target, Value);
	}
	else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
	{
		DoubleProp->SetPropertyValue_InContainer(Target, static_cast<double>(Value));
	}
	else if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		ByteProp->SetPropertyValue_InContainer(Target, static_cast<uint8>(FMath::RoundToInt(Value)));
	}
	else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		IntProp->SetPropertyValue_InContainer(Target, static_cast<int32>(Value));
	}
	else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		BoolProp->SetPropertyValue_InContainer(Target, Value >= 0.5f);
	}
	else
	{
		return;
	}

	FName ParamName(*Property->GetName());

	UE_LOG(LogTemp, Verbose, TEXT("VPBroadcastSubsystem: Set %s = %f on %s"),
		*Property->GetName(), Value, *Target->GetName());

	if (AVPStageEffectActor* EffectActor = Cast<AVPStageEffectActor>(Target))
	{
		EffectActor->OnControlCommandReceived.Broadcast(ParamName, Value);
		EffectActor->ReceiveOnParameterChanged(ParamName, Value);
		EffectActor->ReceiveOnAnyAgentParameterChanged();
	}
	else if (AActor* OwnerActor = Cast<AActor>(Target))
	{
		TArray<UVPAgentComponent*> AgentComponents;
		OwnerActor->GetComponents<UVPAgentComponent>(AgentComponents);
		for (UVPAgentComponent* Agent : AgentComponents)
		{
			Agent->OnControlCommandReceived.Broadcast(ParamName, Value);
			Agent->ReceiveOnParameterChanged(ParamName, Value);
			Agent->ReceiveOnAnyParameterChanged();
		}
	}
}

void UVPBroadcastSubsystem::ProcessCommandQueue()
{
	FVPStageCommand Command;
	while (CommandQueue.Dequeue(Command))
	{
		ApplyControlCommand(Command.ParamHash, Command.Value);
	}
}