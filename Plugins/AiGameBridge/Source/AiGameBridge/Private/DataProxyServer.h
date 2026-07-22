// DataProxyServer — UE 桥接（B1 架构）
//
// 【B1】UE C++ 不再顶替 Node data 占 4000，而是恒定占 127.0.0.1:4100，提供
// "UE 引擎专属能力"（资产索引刷新、缩略图渲染、PIE、Niagara 等），与 Node data(4000) 并存。
//
// 工程 JSON 读写（/toolgen/*）全部归 Node data(4000)，UE 不再提供（相关路由已删除）。
// 计算/协同服务（协同 WS 4004 / Agent 4003 / 文生图）由远端 Node compute(4005) 承担。
//
// 当前路由：/health /info（UE 就绪探测）。后续在此新增 UE 引擎专属路由。

#pragma once

#include "CoreMinimal.h"
#include "AiGameBridge.h"

class IHttpRouter;
struct FHttpServerRequest;
typedef TFunction<void(TUniquePtr<struct FHttpServerResponse>&&)> FHttpResultCallback;

/**
 * UE 桥接 HTTP 服务单例（B1）。
 * Start() 在 4100 起监听；前端探测 4100 可达 = UE 已开着，点亮 UE 专属能力。
 */
class AIGAMEBRIDGE_API FDataProxyServer
{
public:
	static FDataProxyServer& Get();

	/** 启动 HTTP 服务并绑定路由。幂等（重复调用只启动一次）。
	 *  端口：env AIGAME_DATAPROXY_PORT > 默认 4100（UE 桥接固定端口，与 Node data:4000 并存）。 */
	void Start();

	/** 停止 HTTP 服务（模块关闭时调）。 */
	void Stop();

	/** 是否已启动。 */
	bool IsRunning() const { return bStarted; }

private:
	FDataProxyServer() = default;

	/** 绑定所有路由到 Router。 */
	void BindRoutes();

	/** 读请求 query 参数（无则返回默认）。 */
	static FString QueryParam(const FHttpServerRequest& Req, const FString& Key, const FString& Def = FString());

	/** 请求 body（UTF-8 字节）转 FString。 */
	static FString BodyToString(const FHttpServerRequest& Req);

	/** ToolGen 沙盒根：<Project>/Content/Data/ToolGen。 */
	static FString ToolGenRoot();

	/** 共享工作流库根：<ToolchainDir>/data/workflow/Harness（Harness/ 前缀数据落这里）。
	 *  ToolchainDir 读 AIGAME_TOOLCHAIN_DIR env，缺省回落到 <ProjectDir>/../AiGameTools。 */
	static FString SharedWorkflowRoot();

	/** relPath 是否属于共享工作流库（Harness/ 前缀）。 */
	static bool IsSharedHarnessPath(const FString& RelPath);

	/** 把相对路径解析为沙盒下的绝对路径，越界返回空串。
	 *  Harness/ 前缀 → 共享工作流库根；其余 → 工程 ToolGen 根。 */
	static FString ResolveSandboxed(const FString& RelPath);

	/** compute 通知地址（写 Story 节点后 POST /coop/notify）。env COMPUTE_NOTIFY_URL 覆盖。 */
	static FString ComputeNotifyUrl();

	/** 写 Story 拆分节点/索引后，异步 POST 通知 compute 广播（fire-and-forget）。 */
	static void MaybeNotifyCompute(const FString& RelPath, int32 Rev, const FString& By);

	bool bStarted = false;
	uint32 Port = 4100;
	TSharedPtr<IHttpRouter> Router;
};
