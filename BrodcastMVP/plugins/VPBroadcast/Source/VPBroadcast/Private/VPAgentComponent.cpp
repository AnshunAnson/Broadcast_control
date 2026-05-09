#include "VPAgentComponent.h"

#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

UVPAgentComponent::UVPAgentComponent()
	: PreviousColor(FLinearColor::White)
	, bPreviousTrigger(false)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UVPAgentComponent::BeginPlay()
{
	Super::BeginPlay();

	PreviousValues.Add(TEXT("Agent_Opacity"), Agent_Opacity);
	PreviousValues.Add(TEXT("Agent_Emissive"), Agent_Emissive);
	PreviousValues.Add(TEXT("Agent_Scale"), Agent_Scale);
	PreviousColor = Agent_Color;
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

	DetectAndApplyChanges();
}

void UVPAgentComponent::DetectAndApplyChanges()
{
	bool bAnyChanged = false;
	TMap<FName, float> ChangedParams;

	auto CheckAndUpdate = [&](FName ParamName, float CurrentValue)
	{
		float* Prev = PreviousValues.Find(ParamName);
		if (!Prev || !FMath::IsNearlyEqual(*Prev, CurrentValue))
		{
			ChangedParams.Add(ParamName, CurrentValue - (Prev ? *Prev : 0.0f));
			PreviousValues.Add(ParamName, CurrentValue);
			bAnyChanged = true;
		}
	};

	CheckAndUpdate(TEXT("Agent_Opacity"), Agent_Opacity);
	CheckAndUpdate(TEXT("Agent_Emissive"), Agent_Emissive);
	CheckAndUpdate(TEXT("Agent_Scale"), Agent_Scale);

	if (!PreviousColor.Equals(Agent_Color, 0.001f))
	{
		PreviousColor = Agent_Color;
		bAnyChanged = true;
	}

	bool bTriggerNow = Event_Trigger != 0;
	if (bTriggerNow && !bPreviousTrigger)
	{
		OnEffectTriggered(TEXT("Event_Trigger"));
		SpawnDefaultEffect();
	}
	bPreviousTrigger = bTriggerNow;

	if (bAnyChanged)
	{
		ApplyAllParameters();
		OnParametersChanged(ChangedParams);
	}
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

void UVPAgentComponent::TriggerEffectByName(FName EffectName)
{
	OnEffectTriggered(EffectName.ToString());
	SpawnDefaultEffect();
}