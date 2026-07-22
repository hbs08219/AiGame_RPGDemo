// AiGameBridge 模块实现：启动 4100 桥 + 自动资产索引导出

#include "AiGameBridge.h"
#include "DataProxyServer.h"
#include "AssetIndexExporter.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Containers/Ticker.h"

DEFINE_LOG_CATEGORY_STATIC(LogAiGameBridge, Log, All);

void FAiGameBridgeModule::StartupModule()
{
	UE_LOG(LogAiGameBridge, Log, TEXT("[AiGameBridge] 启动：4100 数据桥 + 资产索引导出"));

	// ── 启动 4100 HTTP 桥（UE 引擎专属能力，与 Node data:4000 并存）──
	// Start() 幂等；旧 AiGameEditor 模块不再调，生命周期完全由本插件拥有。
	FDataProxyServer::Get().Start();

	// ── 启动时自动刷新资产索引 ──
	// 时机：必须等 AssetRegistry 全量加载完，再额外延迟 5 秒让 .uasset 真正被流式读取，
	// 否则 RenderThumbnail 时 GetAsset 还拿不到对象，会有大量缩略图渲染失败。
	auto KickOffExport = []()
	{
		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([](float) -> bool
			{
				const int32 N = FAssetIndexExporter::ExportAll();
				UE_LOG(LogAiGameBridge, Log,
					TEXT("[AiGameBridge] 启动自动刷新资产索引完成（%d 个资产）"), N);
				return false;
			}),
			5.0f
		);
	};

	IAssetRegistry& Registry = FAssetRegistryModule::GetRegistry();
	if (Registry.IsLoadingAssets())
	{
		Registry.OnFilesLoaded().AddLambda(KickOffExport);
	}
	else
	{
		KickOffExport();
	}
}

void FAiGameBridgeModule::ShutdownModule()
{
	// 停 4100 桥（释放 Router + 监听器）
	FDataProxyServer::Get().Stop();
	UE_LOG(LogAiGameBridge, Log, TEXT("[AiGameBridge] 已关闭"));
}

IMPLEMENT_MODULE(FAiGameBridgeModule, AiGameBridge)
