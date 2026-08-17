// 无头资产同步 — Published/Cache 资产移动 MCP 工具
//
// 只允许 /Game/AiGame/Published 与 /Game/AiGame/Cache 间移动；不接受任意 /Game 路径，
// 避免同步服务成为通用工程资产重命名接口。

#pragma once

#include "CoreMinimal.h"
#include "IModelContextProtocolTool.h"

class FMoveAssetTool final : public IModelContextProtocolTool
{
public:
	virtual FString GetName() const override { return TEXT("move_asset"); }
	virtual FString GetDescription() const override
	{
		return TEXT("在 /Game/AiGame/Published 与 /Game/AiGame/Cache 之间移动已导入资产，保留 UE redirector。");
	}
	virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override;
	virtual FModelContextProtocolToolResult Run(const TSharedPtr<FJsonObject>& Params) override;
};
