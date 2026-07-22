// AiGameBridge — 纯工具桥插件模块入口
//
// 职责：在引擎初始化后启动 4100 数据桥(FDataProxyServer) 与资产索引导出，
// 并拥有其生命周期（Shutdown 时停桥）。不注册任何 UE 内交互 UI。

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * AiGameBridge 模块。导出宏 AIGAMEBRIDGE_API 由 UBT 自动注入，
 * 供桥内类（FDataProxyServer / FAssetIndexExporter）跨模块可见。
 */
class AIGAMEBRIDGE_API FAiGameBridgeModule : public IModuleInterface
{
public:
	/** IModuleInterface 实现 */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
