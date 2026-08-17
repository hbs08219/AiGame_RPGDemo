// 无头资产同步 — import_asset MCP 工具
//
// 把磁盘上的源资产文件（PNG/FBX/WAV 等）导入为 Content 下的 .uasset，
// 与编辑器模式下 ue-bridge 暴露的 AssetTools.import_asset 同一调用契约，
// 供服务端 project-asset-ue-publisher.cjs 经 MCP 调用。
//
// 底层走 UAssetImportTask + IAssetTools::ImportAssetTasks（bAutomated=true 无交互）。

#pragma once

#include "CoreMinimal.h"
#include "IModelContextProtocolTool.h"

/**
 * import_asset 工具。
 *
 * 输入（JSON object）：
 *   sourcePath      string  源文件磁盘绝对路径（服务端沙盒临时文件）
 *   destinationRoot string  /Game 包根，如 /Game/AiGame/Published
 *   assetName       string  资产名（用于校验与回执；实际命名以源文件/工厂为准）
 *   overwrite       bool    reimport 时 true → bReplaceExisting
 *
 * 输出（structuredContent，与服务端 normalizeToolPayload/validateUePaths 兼容）：
 *   objectPath   /Game/.../Name.Name
 *   packagePath  /Game/.../Name
 *   checksum     服务端固定 publication sha256（由服务端核对，这里回显 sourceSha256）
 *   message      结果描述
 */
class FImportAssetTool : public IModelContextProtocolTool
{
public:
	virtual FString GetName() const override { return TEXT("import_asset"); }
	virtual FString GetDescription() const override
	{
		return TEXT("把磁盘源资产文件导入为 UE Content 下的 .uasset（无头/编辑器通用，自动无交互）。");
	}
	virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override;
	virtual FModelContextProtocolToolResult Run(const TSharedPtr<FJsonObject>& Params) override;
};
