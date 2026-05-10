#include "VPStageEffectActor.h"

#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

AVPStageEffectActor::AVPStageEffectActor()
	: bPreviousFirework(false)
	, bPreviousTransition(false)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	HologramMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HologramMesh"));
	HologramMesh->SetupAttachment(RootComponent);

	KeySpotLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("KeySpotLight"));
	KeySpotLight->SetupAttachment(RootComponent);
	KeySpotLight->Intensity = 5000.0f;
	KeySpotLight->InnerConeAngle = 15.0f;
	KeySpotLight->OuterConeAngle = 30.0f;

	AmbientLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("AmbientLight"));
	AmbientLight->SetupAttachment(RootComponent);
	AmbientLight->Intensity = 2000.0f;
	AmbientLight->AttenuationRadius = 2000.0f;
}

#if WITH_EDITOR
void AVPStageEffectActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ApplyAllParameters();
}
#endif

void AVPStageEffectActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	bool bFireworkNow = Event_Firework != 0;
	bool bTransitionNow = Event_Transition != 0;

	if (bFireworkNow && !bPreviousFirework)
	{
		FName EventName(TEXT("Event_Firework"));
		OnEventTriggered.Broadcast(EventName);
		ReceiveOnEventTriggered(EventName);
	}
	bPreviousFirework = bFireworkNow;

	if (bTransitionNow && !bPreviousTransition)
	{
		FName EventName(TEXT("Event_Transition"));
		OnEventTriggered.Broadcast(EventName);
		ReceiveOnEventTriggered(EventName);
	}
	bPreviousTransition = bTransitionNow;
}

void AVPStageEffectActor::BeginPlay()
{
	Super::BeginPlay();

	if (HologramMesh && HologramMesh->GetMaterial(0))
	{
		HologramMaterial = HologramMesh->CreateDynamicMaterialInstance(0);
	}

	if (MaterialParameterCollection && GetWorld())
	{
		MPCInstance = GetWorld()->GetParameterCollectionInstance(MaterialParameterCollection);
	}

	bPreviousFirework = (Event_Firework != 0);
	bPreviousTransition = (Event_Transition != 0);

	ApplyAllParameters();
}

void AVPStageEffectActor::ApplyAllParameters()
{
	ApplyARHologramParams();
	ApplyAmbientLightParams();
}

void AVPStageEffectActor::ApplyARHologramParams()
{
	if (HologramMaterial)
	{
		HologramMaterial->SetScalarParameterValue(TEXT("Alpha"), AR_Hologram_Alpha);
		HologramMaterial->SetScalarParameterValue(TEXT("EmissivePower"), AR_EmissivePower);
		HologramMaterial->SetVectorParameterValue(TEXT("HologramColor"), AR_Hologram_Color);
	}

	if (MPCInstance)
	{
		MPCInstance->SetScalarParameterValue(TEXT("AR_Hologram_Alpha"), AR_Hologram_Alpha);
		MPCInstance->SetScalarParameterValue(TEXT("AR_EmissivePower"), AR_EmissivePower);
	}

	if (HologramMesh)
	{
		HologramMesh->SetRelativeScale3D(FVector(AR_ModelScale));
	}
}

void AVPStageEffectActor::ApplyAmbientLightParams()
{
	if (KeySpotLight)
	{
		KeySpotLight->SetIntensity(Ambient_Intensity * 10000.0f);
		KeySpotLight->SetInnerConeAngle(FMath::Lerp(5.0f, 20.0f, Spotlight_Angle));
		KeySpotLight->SetOuterConeAngle(FMath::Lerp(15.0f, 45.0f, Spotlight_Angle));
	}

	if (AmbientLight)
	{
		AmbientLight->SetIntensity(Ambient_Intensity * 5000.0f);

		float TempNormalized = FMath::GetMappedRangeValueClamped(
			FVector2D(1000.0f, 10000.0f),
			FVector2D(0.0f, 1.0f),
			Light_Temperature
		);
		FLinearColor WarmColor = FLinearColor(1.0f, 0.6f, 0.3f);
		FLinearColor CoolColor = FLinearColor(0.5f, 0.7f, 1.0f);
		FLinearColor TempColor = FMath::Lerp(WarmColor, CoolColor, TempNormalized);
		AmbientLight->SetLightColor(TempColor, true);
	}

	if (MPCInstance)
	{
		MPCInstance->SetScalarParameterValue(TEXT("Ambient_Intensity"), Ambient_Intensity);
		MPCInstance->SetScalarParameterValue(TEXT("Spotlight_Angle"), Spotlight_Angle);
	}
}

void AVPStageEffectActor::SpawnNiagaraAtActor(UNiagaraSystem* System, FVector Offset)
{
	if (!System || !GetWorld())
	{
		return;
	}

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(),
		System,
		GetActorLocation() + Offset,
		FRotator::ZeroRotator,
		FVector(1.0f),
		true,
		true,
		ENCPoolMethod::AutoRelease
	);
}

void AVPStageEffectActor::SpawnNiagaraAtLocation(UNiagaraSystem* System, FVector Location, FVector Offset)
{
	if (!System || !GetWorld())
	{
		return;
	}

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(),
		System,
		Location + Offset,
		FRotator::ZeroRotator,
		FVector(1.0f),
		true,
		true,
		ENCPoolMethod::AutoRelease
	);
}

void AVPStageEffectActor::TriggerFirework()
{
	SpawnNiagaraAtActor(FireworkEffect, FVector(0.0f, 0.0f, 500.0f));
}

void AVPStageEffectActor::TriggerSceneTransition()
{
	SpawnNiagaraAtActor(TransitionEffect);
}