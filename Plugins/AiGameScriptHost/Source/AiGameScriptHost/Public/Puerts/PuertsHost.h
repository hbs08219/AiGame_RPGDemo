// UPuertsHost — PuerTS 脚本运行时宿主(GameInstanceSubsystem)
//
// gameplay 逻辑迁移到 TypeScript 的运行时入口。C++ 这层只做"宿主 + 桥":
//   - 管理 FJsEnv 生命周期,启动 TS 入口模块 "Main"(把 this 作为 "Host" 传给脚本)。
//   - 世界与本地玩家就绪后广播 OnReady,作为 TS 侧 gameplay 模块的统一启动点
//     (GameInstanceSubsystem 初始化早于 LocalPlayer/World,直接访问会拿不到)。
//   - 向 TS 暴露访问 C++ 能力的桥接 API:取任意 Subsystem(Inventory/Quest/Battle/输入…)、创建 UMG。
// gameplay 业务逻辑全部在 TS 侧(TypeScript/),经这些桥访问引擎与底层系统。

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/SubclassOf.h"
#include "Containers/Ticker.h"
#include "PuertsHost.generated.h"

// 本插件日志分类（独立于游戏模块，避免依赖外部 LogAiGame）
AIGAMESCRIPTHOST_API DECLARE_LOG_CATEGORY_EXTERN(LogAiGameScriptHost, Log, All);

// 世界+本地玩家就绪后广播一次(无参)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPuertsHostReady);
// 每帧广播给 TS Gameplay Kernel；仅传递时间事实，不携带任何玩法规则。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPuertsHostFrame, float, DeltaSeconds);

namespace puerts { class FJsEnv; }

class UUserWidget;
class UGameInstanceSubsystem;
class ULocalPlayerSubsystem;

UCLASS()
class AIGAMESCRIPTHOST_API UPuertsHost : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 世界与本地玩家就绪后广播一次。TS 在此启动需要 World/Player 的 gameplay 模块。 */
	UPROPERTY(BlueprintAssignable, Category = "Puerts")
	FPuertsHostReady OnReady;

	/** 引擎帧时钟。TS Kernel 在此处理已归一化输入并产出 EngineCommand。 */
	UPROPERTY(BlueprintAssignable, Category = "Puerts")
	FPuertsHostFrame OnFrame;

	// —— Subsystem 访问层:让 TS 拿到 C++ 子系统(并订阅其 delegate) ——

	/** 取 GameInstanceSubsystem。TS: host.GetGameSubsystem(UE.InventorySubsystem.StaticClass()) as UE.InventorySubsystem */
	UFUNCTION(BlueprintCallable, Category = "Puerts")
	UGameInstanceSubsystem* GetGameSubsystem(TSubclassOf<UGameInstanceSubsystem> SubsystemClass) const;

	/** 取 LocalPlayerSubsystem(如 CustomInputSubsystem,用于订阅输入事件)。无本地玩家时返回 null */
	UFUNCTION(BlueprintCallable, Category = "Puerts")
	ULocalPlayerSubsystem* GetLocalPlayerSubsystem(TSubclassOf<ULocalPlayerSubsystem> SubsystemClass) const;

	// —— UI 便利:创建/移除 UMG(输入模式等流程由 TS 控制) ——

	/** 由 WBP 类路径创建 UUserWidget 并加入视口,返回实例(TS 自行填充/绑事件) */
	UFUNCTION(BlueprintCallable, Category = "Puerts")
	UUserWidget* SpawnWidget(const FString& WidgetClassPath, int32 ZOrder = 0);

	/** 从父级移除 widget */
	UFUNCTION(BlueprintCallable, Category = "Puerts")
	void RemoveWidget(UUserWidget* Widget);

	// —— 数据便利:给任意 TS 模块读 ToolGen JSON(不必依赖某个业务 Bridge) ——

	/** TS 调:读 Content/Data/ToolGen/<RelPath> 文件原文(TS 自行 JSON.parse)。失败返空串。 */
	UFUNCTION(BlueprintCallable, Category = "Puerts")
	FString LoadToolGenJson(const FString& RelPathUnderToolGen) const;

private:
	void StartScript();
	/** 轮询:World + 本地玩家就绪 → 广播 OnReady(一次)→ 停止轮询 */
	bool TryFireReady();

	TSharedPtr<puerts::FJsEnv> JsEnv;
	FTSTicker::FDelegateHandle ReadyTickHandle;
	FTSTicker::FDelegateHandle FrameTickHandle;
	bool bScriptStarted = false;
	bool bReadyFired = false;
};
