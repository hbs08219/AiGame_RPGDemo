// 无头资产同步 — AiGameAssetSync commandlet
//
// 用法：
//   UnrealEditor-Cmd.exe <proj.uproject> -run=AiGameAssetSync -MCPport=<N>
//     -unattended -nopause -nullrhi -nosourcecontrol
//
// 行为：
//   1. 加载 Runtime 模块 ModelContextProtocol（无头下可加载，已验证不碰 UI）；
//   2. StartServer(<N>) 起 MCP HTTP 服务（端口独立，避开编辑器 8000）；
//   3. AddTool(import_asset) 显式注册（绕开"Toolset 自动发现是编辑器专属"限制）；
//   4. 挂起主循环等待请求，直到进程被外部终止（优雅让位由服务端控制）。

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "AiGameAssetSyncCommandlet.generated.h"

UCLASS()
class UAiGameAssetSyncCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UAiGameAssetSyncCommandlet();

	/** UCommandlet 入口。返回 0 = 成功（这里常驻直到被终止，正常不会自然返回）。 */
	virtual int32 Main(const FString& Params) override;
};
