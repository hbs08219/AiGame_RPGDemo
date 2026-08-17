// 无头资产同步 — 手动清理 Cache 资产 MCP 工具
//
// 这是唯一删除 UE 资产的工具。仅接受 /Game/AiGame/Cache 下路径，调用方仍须在
// 服务端实施 owner/admin 人工确认；自动协调器不得调用它。

#pragma once

#include "CoreMinimal.h"
#include "IModelContextProtocolTool.h"

class FDeleteCachedAssetTool final : public IModelContextProtocolTool
{
public:
	virtual FString GetName() const override { return TEXT("delete_cached_asset"); }
	virtual FString GetDescription() const override
	{
		return TEXT("删除 /Game/AiGame/Cache 下的无引用缓存资产。仅供人工确认后的手动清理。 ");
	}
	virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override;
	virtual FModelContextProtocolToolResult Run(const TSharedPtr<FJsonObject>& Params) override;
};
