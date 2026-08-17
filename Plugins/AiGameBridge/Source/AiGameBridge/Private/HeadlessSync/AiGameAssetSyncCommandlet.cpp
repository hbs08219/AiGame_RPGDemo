// 无头资产同步 — AiGameAssetSync commandlet 实现

#include "HeadlessSync/AiGameAssetSyncCommandlet.h"
#include "HeadlessSync/Tools/DeleteCachedAssetTool.h"
#include "HeadlessSync/Tools/ImportAssetTool.h"
#include "HeadlessSync/Tools/MoveAssetTool.h"
#include "HeadlessSync/Tools/ShutdownHeadlessSyncTool.h"

#include "IModelContextProtocolModule.h"
#include "HttpServerModule.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/Engine.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Templates/Atomic.h"

DEFINE_LOG_CATEGORY_STATIC(LogAiGameAssetSync, Log, All);

UAiGameAssetSyncCommandlet::UAiGameAssetSyncCommandlet()
{
	// 无头常驻服务：不打日志头、不要求编辑器 UI、失败也不弹窗。
	IsClient = false;
	IsEditor = true;   // 需要 Editor target 的 AssetTools/UnrealEd（导入是编辑器能力）
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = false;
}

int32 UAiGameAssetSyncCommandlet::Main(const FString& Params)
{
	// 解析端口：-MCPport=<N>，默认 8001（避开编辑器常用 8000）。
	uint32 Port = 8001;
	FString PortStr;
	if (FParse::Value(*Params, TEXT("MCPport="), PortStr))
	{
		const int32 Parsed = FCString::Atoi(*PortStr);
		if (Parsed >= 1 && Parsed <= 65535)
		{
			Port = static_cast<uint32>(Parsed);
		}
		else
		{
			UE_LOG(LogAiGameAssetSync, Warning,
				TEXT("[AiGameAssetSync] 非法 MCPport=%s，回退默认 %u"), *PortStr, Port);
		}
	}

	UE_LOG(LogAiGameAssetSync, Log,
		TEXT("[AiGameAssetSync] 启动无头资产同步 MCP 服务，端口 %u"), Port);

	// 加载 Runtime MCP 模块（Type=Runtime，无头可加载）。
	IModelContextProtocolModule& Mcp = IModelContextProtocolModule::GetChecked();

	// 在 tool 内持有共享原子标志；shutdown 请求若排在 import_asset 后面，会等导入完整
	// 返回主线程后才置位，因此不会中断半写入的资产包。
	const TSharedRef<TAtomic<bool>, ESPMode::ThreadSafe> ShutdownRequested = MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(false);

	// 显式注册工具，绕开编辑器 Toolset 自动发现限制。
	if (!Mcp.AddTool(MakeShared<FImportAssetTool>()))
	{
		UE_LOG(LogAiGameAssetSync, Error,
			TEXT("[AiGameAssetSync] import_asset 注册失败（同名工具已存在？）"));
		return 1;
	}
	if (!Mcp.AddTool(MakeShared<FMoveAssetTool>()))
	{
		UE_LOG(LogAiGameAssetSync, Error,
			TEXT("[AiGameAssetSync] move_asset 注册失败（同名工具已存在？）"));
		return 1;
	}
	if (!Mcp.AddTool(MakeShared<FDeleteCachedAssetTool>()))
	{
		UE_LOG(LogAiGameAssetSync, Error,
			TEXT("[AiGameAssetSync] delete_cached_asset 注册失败（同名工具已存在？）"));
		return 1;
	}
	if (!Mcp.AddTool(MakeShared<FShutdownHeadlessSyncTool>([ShutdownRequested]()
		{
			ShutdownRequested->Store(true);
		})))
	{
		UE_LOG(LogAiGameAssetSync, Error,
			TEXT("[AiGameAssetSync] shutdown_headless_sync 注册失败（同名工具已存在？）"));
		return 1;
	}

	// 起 MCP HTTP 服务（监听 /mcp，与编辑器模式同约定）。
	Mcp.StartServer(Port);

	UE_LOG(LogAiGameAssetSync, Log,
		TEXT("[AiGameAssetSync] MCP 服务就绪：http://localhost:%u/mcp（import_asset 已注册）"), Port);

	// 挂起主循环：commandlet 主线程空转等待，直到外部（优雅让位/服务端）终止进程。
	// GIsRunning 由引擎主循环维护；commandlet 里手动驱动 tick 以处理 HTTP 请求。
	GIsRunning = true;
	double LastTime = FPlatformTime::Seconds();
	while (GIsRunning && !IsEngineExitRequested() && !ShutdownRequested->Load())
	{
		// 驱动引擎 tick，让 MCP HTTP 服务有机会处理到来的请求。
		const double Now = FPlatformTime::Seconds();
		const float DeltaTime = static_cast<float>(Now - LastTime);
		LastTime = Now;
		GEngine->Tick(DeltaTime, false);
		// 显式驱动 HTTP 服务器模块：commandlet 手动 GEngine->Tick 不会触发
		// FHttpServerModule 的 FTSTickerObjectBase ticker（它挂在真主循环上），
		// 必须在这里逐帧泵，否则 HTTP listener 接受连接但永不处理请求。
		FHttpServerModule::Get().Tick(DeltaTime);
		FPlatformProcess::Sleep(0.01f);
	}

	UE_LOG(LogAiGameAssetSync, Log, TEXT("[AiGameAssetSync] 收到引擎退出或优雅让位请求，停止 MCP 服务"));
	Mcp.StopServer();
	return 0;
}
