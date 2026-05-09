#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VPAgentComponent.generated.h"

class UMaterialParameterCollection;
class UMaterialParameterCollectionInstance;
class UNiagaraSystem;

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

	UFUNCTION(BlueprintCallable, Category = "VPBroadcast|Agent")
	void ApplyAllParameters();

	UFUNCTION(BlueprintCallable, Category = "VPBroadcast|Agent")
	void TriggerEffectByName(FName EffectName);

	// ==================== 预设效果参数（可为空，美术可在蓝图子类中自由增删） ====================

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

	// ==================== 资源配置（美术在编辑器中拖入资产即可） ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VPBroadcast|Resources")
	TObjectPtr<UMaterialParameterCollection> MaterialParameterCollection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VPBroadcast|Resources")
	TObjectPtr<UNiagaraSystem> DefaultEffectAsset;

	// ==================== 蓝图可覆写的事件 ====================

	UFUNCTION(BlueprintImplementableEvent, Category = "VPBroadcast|Agent")
	void OnEffectTriggered(const FString& EffectName);

	UFUNCTION(BlueprintImplementableEvent, Category = "VPBroadcast|Agent")
	void OnParametersChanged(const TMap<FName, float>& ChangedParams);

protected:
	void DetectAndApplyChanges();
	void SpawnDefaultEffect();

	void SyncToMPC(FName ParamName, float Value);
	void SyncAllToMPC();
	void SyncToActorComponents();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollectionInstance> MPCInstance;

	TMap<UStaticMeshComponent*, TArray<UMaterialInstanceDynamic*>> CachedMIDs;

	TMap<FName, float> PreviousValues;
	FLinearColor PreviousColor;

	bool bPreviousTrigger;
};