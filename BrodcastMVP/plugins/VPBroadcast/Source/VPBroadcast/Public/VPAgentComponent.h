#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VPAgentComponent.generated.h"

class UMaterialParameterCollection;
class UMaterialParameterCollectionInstance;
class UNiagaraSystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAgentControlCommand, FName, ParameterName, float, Value);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAgentEventTriggered, FName, EventName);

UCLASS(ClassGroup = VPBroadcast, Blueprintable, BlueprintType,
	meta = (BlueprintSpawnableComponent, DisplayName = "VP Stage Agent"))
class VPBROADCAST_API UVPAgentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVPAgentComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// ==================== 蓝图可调用的工具函数 ====================

	UFUNCTION(BlueprintCallable, Category = "VPBroadcast|Agent")
	void ApplyAllParameters();

	UFUNCTION(BlueprintCallable, Category = "VPBroadcast|Agent")
	void SyncToActorComponents();

	UFUNCTION(BlueprintCallable, Category = "VPBroadcast|Agent")
	void SyncAllToMPC();

	UFUNCTION(BlueprintCallable, Category = "VPBroadcast|Agent")
	void TriggerEffectByName(FName EffectName);

	UFUNCTION(BlueprintCallable, Category = "VPBroadcast|Agent")
	void SpawnDefaultEffect();

	// ==================== 蓝图事件（和 Widget 滑条 OnValueChanged 一样） ====================

	UPROPERTY(BlueprintAssignable, Category = "VPBroadcast|Events", meta = (DisplayName = "On Control Command"))
	FOnAgentControlCommand OnControlCommandReceived;

	UPROPERTY(BlueprintAssignable, Category = "VPBroadcast|Events", meta = (DisplayName = "On Event Triggered"))
	FOnAgentEventTriggered OnEventTriggered;

	UFUNCTION(BlueprintImplementableEvent, Category = "VPBroadcast|Events", meta = (DisplayName = "On Parameter Changed"))
	void ReceiveOnParameterChanged(const FName& ParameterName, float NewValue);

	UFUNCTION(BlueprintImplementableEvent, Category = "VPBroadcast|Events", meta = (DisplayName = "On Any Parameter Changed"))
	void ReceiveOnAnyParameterChanged();

	UFUNCTION(BlueprintImplementableEvent, Category = "VPBroadcast|Events", meta = (DisplayName = "On Event Triggered"))
	void ReceiveOnEventTriggered(const FName& EventName);

	UFUNCTION(BlueprintImplementableEvent, Category = "VPBroadcast|Agent")
	void OnEffectTriggered(const FString& EffectName);

	UFUNCTION(BlueprintImplementableEvent, Category = "VPBroadcast|Agent")
	void OnParametersChanged(const TMap<FName, float>& ChangedParams);

	// ==================== 预设效果参数 ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VPAgent|透明度",
		meta = (ClampMin = "0.0", ClampMax = "1.0", VPA_Tags = "透明度,Alpha,淡入淡出,Opacity", VPA_Description = "控制所属 Actor 的整体透明度"))
	float Agent_Opacity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VPAgent|发光",
		meta = (ClampMin = "0.0", ClampMax = "10.0", VPA_Tags = "发光,亮度,Emissive,Glow", VPA_Description = "控制材质发光强度"))
	float Agent_Emissive = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VPAgent|缩放",
		meta = (ClampMin = "0.1", ClampMax = "5.0", VPA_Tags = "缩放,Scale,大小,尺寸", VPA_Description = "控制所属 Actor 的缩放比例"))
	float Agent_Scale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VPAgent|颜色",
		meta = (VPA_Tags = "颜色,Color,色彩,Tint", VPA_Description = "控制所属 Actor 的染色"))
	FLinearColor Agent_Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VPAgent|事件",
		meta = (VPA_Tags = "触发,Trigger,Effect,特效", VPA_Description = "触发特效事件（在蓝图中覆写 OnEffectTriggered 响应）"))
	uint8 Event_Trigger = 0;

	// ==================== 资源配置 ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VPBroadcast|Resources")
	TObjectPtr<UMaterialParameterCollection> MaterialParameterCollection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VPBroadcast|Resources")
	TObjectPtr<UNiagaraSystem> DefaultEffectAsset;

protected:
	void DetectEventEdge();

	void SyncToMPC(FName ParamName, float Value);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollectionInstance> MPCInstance;

	TMap<UStaticMeshComponent*, TArray<UMaterialInstanceDynamic*>> CachedMIDs;

	bool bPreviousTrigger = false;
};
