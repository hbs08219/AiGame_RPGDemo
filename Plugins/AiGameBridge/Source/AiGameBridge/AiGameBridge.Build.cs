// AiGameBridge module — 纯工具桥（独立插件）
//
// 只承载 UE 引擎专属能力：
//   - FDataProxyServer  : 4100 HTTP 桥（UE 就绪探测 + 资产索引刷新等 UE 专属路由）
//   - FAssetIndexExporter: 扫描资产 + 渲染缩略图 → Saved/DreamMaker/
//
// 不含：Puerts 运行时宿主 / gameplay 框架（TS）/ 任何 UE 内交互 UI。
// 这些是"项目模板"与独立 Puerts 插件的职责，工作流由工作台负责分发。

using UnrealBuildTool;

public class AiGameBridge : ModuleRules
{
	public AiGameBridge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// --- 4100 HTTP 桥 (FDataProxyServer) ---
			"HTTPServer",      // FHttpServerModule 内置 HTTP 服务（4100 监听）
			"HTTP",            // FHttpModule 客户端（写完发 notify 到 compute 4005）
			"Json",            // FJsonObject / 序列化
			"JsonUtilities",
			// --- 资产索引导出 (FAssetIndexExporter) ---
			"AssetRegistry",   // IAssetRegistry 扫描资产
			"ImageWrapper",    // FImageUtils 缩略图 → PNG 编码
		"AssetTools",      // ThumbnailTools / ObjectTools
		"UnrealEd",        // UE5.8 已并入：ThumbnailManager / ThumbnailHelpers / ObjectTools
			"AnimGraphRuntime", // UAnimSequence（帧率/帧数导出）
			"Niagara",         // UNiagaraSystem（loop 导出）
		});

		// 缩略图渲染需 GameThread 资产加载 + 渲染管线，仅在 Editor 下可用
		if (Target.Type != TargetType.Editor)
		{
			// 理论不会走到（.uplugin 已声明模块 Type=Editor），保险断言
			System.Console.WriteLine("AiGameBridge 仅支持 Editor 构建");
		}
	}
}
