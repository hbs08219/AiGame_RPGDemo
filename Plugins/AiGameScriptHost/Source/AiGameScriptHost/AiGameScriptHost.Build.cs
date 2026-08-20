// AiGameScriptHost — PuerTS 脚本运行时宿主插件
// 仅承载 UPuertsHost（UGameInstanceSubsystem），管理 FJsEnv 生命周期并启动 TS 入口。
// 不含具体 gameplay 逻辑（那些在 TypeScript/）。

using UnrealBuildTool;

public class AiGameScriptHost : ModuleRules
{
	public AiGameScriptHost(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UMG",                   // SpawnWidget: CreateWidget / LoadClass<UUserWidget>
			"JsEnv",                 // puerts::FJsEnv 脚本宿主(PuerTS)
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
