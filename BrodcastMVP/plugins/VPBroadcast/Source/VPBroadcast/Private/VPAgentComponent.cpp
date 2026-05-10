#include "VPAgentComponent.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

UVPAgentComponent::UVPAgentComponent()
	: bPreviousTrigger(false)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UVPAgentComponent::BeginPlay()
{
	Super::BeginPlay();

	bPreviousTrigger = false;

	if (MaterialParameterCollection && GetWorld())
	{
		MPCInstance = GetWorld()->GetParameterCollectionInstance(MaterialParameterCollection);
	}

	ApplyAllParameters();
}

void UVPAgentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CachedMIDs.Reset();
	Super::EndPlay(EndPlayReason);
}

void UVPAgentComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	DetectEventEdge();
}

void UVPAgentComponent::DetectEventEdge()
{
	bool bTriggerNow = Event_Trigger != 0;
	if (bTriggerNow && !bPreviousTrigger)
	{
		FName EventName(TEXT("Event_Trigger"));
		OnEventTriggered.Broadcast(EventName);
		ReceiveOnEventTriggered(EventName);
		OnEffectTriggered(TEXT("Event_Trigger"));
	}
	bPreviousTrigger = bTriggerNow;
}

void UVPAgentComponent::ApplyAllParameters()
{
	SyncToActorComponents();
	SyncAllToMPC();
}

void UVPAgentComponent::SyncToActorComponents()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<UStaticMeshComponent*> MeshComponents;
	Owner->GetComponents<UStaticMeshComponent>(MeshComponents);
	for (UStaticMeshComponent* Mesh : MeshComponents)
	{
		TArray<UMaterialInstanceDynamic*>& MIDs = CachedMIDs.FindOrAdd(Mesh);
		const int32 NumMaterials = Mesh->GetNumMaterials();

		if (MIDs.Num() != NumMaterials)
		{
			MIDs.SetNum(NumMaterials);
		}

		for (int32 i = 0; i < NumMaterials; ++i)
		{
			if (!IsValid(MIDs[i]))
			{
				MIDs[i] = Mesh->CreateDynamicMaterialInstance(i);
			}

			if (MIDs[i])
			{
				MIDs[i]->SetScalarParameterValue(TEXT("Opacity"), Agent_Opacity);
				MIDs[i]->SetScalarParameterValue(TEXT("Emissive"), Agent_Emissive);
				MIDs[i]->SetVectorParameterValue(TEXT("ColorTint"), Agent_Color);
			}
		}
	}

	Owner->SetActorScale3D(FVector(Agent_Scale));

	TArray<UPointLightComponent*> PointLights;
	Owner->GetComponents<UPointLightComponent>(PointLights);
	for (UPointLightComponent* Light : PointLights)
	{
		Light->SetIntensity(Agent_Emissive * 2000.0f);
		Light->SetLightColor(Agent_Color, true);
	}
}

void UVPAgentComponent::SyncToMPC(FName ParamName, float Value)
{
	if (!MPCInstance)
	{
		return;
	}
	MPCInstance->SetScalarParameterValue(ParamName, Value);
}

void UVPAgentComponent::SyncAllToMPC()
{
	if (!MPCInstance)
	{
		return;
	}
	MPCInstance->SetScalarParameterValue(TEXT("Agent_Opacity"), Agent_Opacity);
	MPCInstance->SetScalarParameterValue(TEXT("Agent_Emissive"), Agent_Emissive);
	MPCInstance->SetScalarParameterValue(TEXT("Agent_Scale"), Agent_Scale);
	MPCInstance->SetVectorParameterValue(TEXT("Agent_Color"), Agent_Color);
}

void UVPAgentComponent::SpawnDefaultEffect()
{
	if (!DefaultEffectAsset)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(),
		DefaultEffectAsset,
		Owner->GetActorLocation(),
		FRotator::ZeroRotator,
		FVector(1.0f),
		true, true,
		ENCPoolMethod::AutoRelease
	);
}

void UVPAgentComponent::TriggerEffectByName(FName EffectName)
{
	OnEffectTriggered(EffectName.ToString());
	OnEventTriggered.Broadcast(EffectName);
	ReceiveOnEventTriggered(EffectName);
	SpawnDefaultEffect();
}