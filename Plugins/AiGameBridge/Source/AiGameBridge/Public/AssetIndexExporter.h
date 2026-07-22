// AssetIndexExporter
// 扫描项目内指定类型的资产，导出为 JSON 供 DreamMaker 工具读取
// 输出路径：<Project>/Saved/DreamMaker/asset-index.json

#pragma once

#include "CoreMinimal.h"
#include "AiGameBridge.h"

class AIGAMEBRIDGE_API FAssetIndexExporter
{
public:
	/** 扫所有支持类型的资产并写文件。返回成功导出的资产总数 */
	static int32 ExportAll();

	/** 输出文件绝对路径 */
	static FString GetOutputPath();
};
