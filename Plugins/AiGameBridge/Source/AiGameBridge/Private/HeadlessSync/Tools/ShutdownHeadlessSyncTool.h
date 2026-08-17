// 无头资产同步 — 优雅停机 MCP 工具
//
// 由服务端在用户启动 UE 编辑器前调用。请求仅在当前 import_asset 完成后才能被
// commandlet 主线程处理，因此天然满足“当前批次完成后再让位”的 B1 语义。

#pragma once

#include "CoreMinimal.h"
#include "IModelContextProtocolTool.h"
#include "Templates/Function.h"

class FShutdownHeadlessSyncTool final : public IModelContextProtocolTool
{
public:
	explicit FShutdownHeadlessSyncTool(TFunction<void()> InRequestShutdown)
		: RequestShutdown(MoveTemp(InRequestShutdown))
	{
	}

	virtual FString GetName() const override { return TEXT("shutdown_headless_sync"); }
	virtual FString GetDescription() const override
	{
		return TEXT("请求无头资产同步服务在完成当前导入批次后优雅退出，为 UE 编辑器让位。");
	}
	virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override;
	virtual FModelContextProtocolToolResult Run(const TSharedPtr<FJsonObject>& Params) override;

private:
	TFunction<void()> RequestShutdown;
};
