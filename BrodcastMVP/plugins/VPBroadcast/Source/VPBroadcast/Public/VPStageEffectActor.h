#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VPStageEffectActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialParameterCollection;
class UMaterialParameterCollectionInstance;
class UStaticMeshComponent;
class UNiagaraSystem;
class UNiagaraComponent;
class USpotLightComponent;
class UPointLightComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStageControlCommand, FName, ParameterName, float, Value);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStageEventTriggered, FName, EventName);

UCLASS(Blueprintable, BlueprintType, Category = "VPBroadcast")
class VPBROADCAST_API AVPStageEffectActor : public AActor
{
	GENERATED_BODY()

public:
	AVPStageEffectActor();

	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// ==================== 蓝图可调用的工具函数 ====================

	UFUNCTION(BlueprintCallable, Category = "VPBroadcast|Effect")
	void ApplyAllParameters();

	UFUNCTION(BlueprintCallable, Category = "VPBroadcast|Effect")
	void ApplyARHologramParams();

	UFUNCTION(BlueprintCallable, Category = "VPBroadcast|Effect")
	void ApplyAmbientLightParams();

	UFUNCTION(BlueprintCallable, Category = "VPBroadcast|Effect")
	void TriggerFirework();

	UFUNCTION(BlueprintCallable, Category = "VPBroadcast|Effect")
	void TriggerSceneTransition();

	UFUNCTION(BlueprintCallable, Category = "VPBroadcast|Effect")
	void SpawnNiagaraAtLocation(UNiagaraSystem* System, FVector Location, FVector Offset = FVector::ZeroVector);

	// ==================== 蓝图事件（和 Widget 滑条 OnValueChanged 一样） ====================

	UPROPERTY(BlueprintAssignable, Category = "VPBroadcast|Events", meta = (DisplayName = "On Control Command"))
	FOnStageControlCommand OnControlCommandReceived;

	UPROPERTY(BlueprintAssignable, Category = "VPBroadcast|Events", meta = (DisplayName = "On Event Triggered"))
	FOnStageEventTriggered OnEventTriggered;

	UFUNCTION(BlueprintImplementableEvent, Category = "VPBroadcast|Events", meta = (DisplayName = "On Parameter Changed"))
	void ReceiveOnParameterChanged(const FName& ParameterName, float NewValue);

	UFUNCTION(BlueprintImplementableEvent, Category = "VPBroadcast|Events", meta = (DisplayName = "On Any Parameter Changed"))
	void ReceiveOnAnyAgentParameterChanged();

	UFUNCTION(BlueprintImplementableEvent, Category = "VPBroadcast|Events", meta = (DisplayName = "On Event Triggered"))
	void ReceiveOnEventTriggered(const FName& EventName);

	// ==================== AR 全息投影参数 ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VPAgent|AR 全息",
		meta = (ClampMin = "0.0", ClampMax = "1.0", VPA_Tags = "全息,透明度,AR,Hologram", VPA_Description = "全息投影透明度，0.0 完全透明 / 1.0 完全不透明"))
	float AR_Hologram_Alpha = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VPAgent|AR 全息",
		meta = (ClampMin = "0.0", ClampMax = "5.0", VPA_Tags = "全息,亮度,发光,AR", VPA_Description = "全息投影发光强度倍率"))
	float AR_EmissivePower = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VPAgent|AR 全息",
		meta = (VPA_Tags = "全息,颜色,AR,Hologram", VPA_Description = "全息投影颜色 (Linear Color)，控制全息色彩倾向"))
	FLinearColor AR_Hologram_Color = FLinearColor(0.0f, 0.8f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VPAgent|AR 全息",
		meta = (ClampMin = "0.1", ClampMax = "3.0", VPA_Tags = "全息,缩放,AR", VPA_Description = "全息投影模型缩放比例"))
	float AR_ModelScale = 1.0f;

	// ==================== 环境光参数 ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VPAgent|环境光",
		meta = (ClampMin = "0.0", ClampMax = "1.0", VPA_Tags = "环境光,亮度,氛围", VPA_Description = "环境光整体强度"))
	float Ambient_Intensity = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VPAgent|环境光",
		meta = (ClampMin = "0.0", ClampMax = "1.0", VPA_Tags = "聚光灯,角度,光束", VPA_Description = "聚光灯锥角参数，控制光束扩散范围"))
	float Spotlight_Angle = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VPAgent|环境光",
		meta = (ClampMin = "1000.0", ClampMax = "10000.0", VPA_Tags = "色温,灯光,冷暖", VPA_Description = "灯光色温 (K)，1000 暖色 / 10000 冷色"))
	float Light_Temperature = 5500.0f;

	// ==================== 舞台效果事件 ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VPAgent|舞台事件",
		meta = (VPA_Tags = "烟花,特效,粒子,Firework", VPA_Description = "触发烟花粒子特效"))
	uint8 Event_Firework = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VPAgent|舞台事件",
		meta = (VPA_Tags = "转场,过渡,切换,Transition", VPA_Description = "触发舞台场景转场动画"))
	uint8 Event_Transition = 0;

	// ==================== 场景组件引用 ====================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VPBroadcast|Components")
	TObjectPtr<UStaticMeshComponent> HologramMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VPBroadcast|Components")
	TObjectPtr<USpotLightComponent> KeySpotLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VPBroadcast|Components")
	TObjectPtr<UPointLightComponent> AmbientLight;

	// ==================== 资源引用 ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VPBroadcast|Resources")
	TObjectPtr<UMaterialParameterCollection> MaterialParameterCollection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VPBroadcast|Resources")
	TObjectPtr<UNiagaraSystem> FireworkEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VPBroadcast|Resources")
	TObjectPtr<UNiagaraSystem> TransitionEffect;

protected:
	void SpawnNiagaraAtActor(UNiagaraSystem* System, FVector Offset = FVector::ZeroVector);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HologramMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollectionInstance> MPCInstance;

	bool bPreviousFirework = false;
	bool bPreviousTransition = false;
};
