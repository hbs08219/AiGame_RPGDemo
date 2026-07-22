#include "DataProxyServer.h"
#include "AssetIndexExporter.h"   // 【UE-1】资产索引刷新（含缩略图渲染）
#include "Async/Async.h"          // AsyncTask(GameThread) 排帧执行

#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "HttpResultCallback.h"
#include "HttpPath.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogDataProxy, Log, All);

namespace
{
	// 统一 CORS 头（与 Node dev-control 对齐，前端跨端口 fetch 需要）
	void AddCors(FHttpServerResponse& Resp)
	{
		Resp.Headers.Add(TEXT("Access-Control-Allow-Origin"), { TEXT("*") });
		Resp.Headers.Add(TEXT("Access-Control-Allow-Methods"), { TEXT("GET, POST, PUT, DELETE, OPTIONS, PATCH") });
		Resp.Headers.Add(TEXT("Access-Control-Allow-Headers"), { TEXT("Content-Type") });
	}

	// 用 JSON 字符串建 200 响应（带 CORS）
	TUniquePtr<FHttpServerResponse> JsonOk(const FString& Json)
	{
		TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create(Json, TEXT("application/json"));
		Resp->Code = EHttpServerResponseCodes::Ok;
		AddCors(*Resp);
		return Resp;
	}

	// {ok:false, error:"..."} + 指定状态码
	TUniquePtr<FHttpServerResponse> JsonErr(EHttpServerResponseCodes Code, const FString& Err)
	{
		FString Body = FString::Printf(TEXT("{\"ok\":false,\"error\":\"%s\"}"), *Err.ReplaceCharWithEscapedChar());
		TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create(Body, TEXT("application/json"));
		Resp->Code = Code;
		AddCors(*Resp);
		return Resp;
	}

	// JSON 字符串转义（用于把值塞进手拼 JSON 的字符串位）
	FString JsonEsc(const FString& S)
	{
		FString R = S;
		R.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
		R.ReplaceInline(TEXT("\""), TEXT("\\\""));
		R.ReplaceInline(TEXT("\n"), TEXT("\\n"));
		R.ReplaceInline(TEXT("\r"), TEXT("\\r"));
		R.ReplaceInline(TEXT("\t"), TEXT("\\t"));
		return R;
	}

	// 把任意 JSON 文本 pretty-print 归一化（解析→2空格缩进序列化，与 Node JSON.stringify(x,null,2) 对齐）。
	// 解析失败返回 false（调用方回退原文）。
	bool PrettyNormalizeJson(const FString& In, FString& Out)
	{
		TSharedPtr<FJsonValue> Value;
		TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(In);
		if (!FJsonSerializer::Deserialize(Reader, Value) || !Value.IsValid())
		{
			return false;
		}
		TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);
		bool bOk = FJsonSerializer::Serialize(Value.ToSharedRef(), FString(), Writer);
		Writer->Close();
		return bOk;
	}

	// 从 JSON 文本里取顶层整数字段（用于 notify 的 rev）。取不到返回 -1。
	int32 ReadTopLevelInt(const FString& Json, const FString& Field)
	{
		TSharedPtr<FJsonObject> Obj;
		TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Json);
		if (FJsonSerializer::Deserialize(Reader, Obj) && Obj.IsValid())
		{
			double N;
			if (Obj->TryGetNumberField(Field, N)) return (int32)N;
		}
		return -1;
	}

	FString NowIso()
	{
		return FDateTime::UtcNow().ToIso8601();
	}
}

FString FDataProxyServer::QueryParam(const FHttpServerRequest& Req, const FString& Key, const FString& Def)
{
	const FString* V = Req.QueryParams.Find(Key);
	return V ? *V : Def;
}

FString FDataProxyServer::BodyToString(const FHttpServerRequest& Req)
{
	if (Req.Body.Num() == 0) return FString();
	FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(Req.Body.GetData()), Req.Body.Num());
	return FString(Conv.Length(), Conv.Get());
}

FDataProxyServer& FDataProxyServer::Get()
{
	static FDataProxyServer Instance;
	return Instance;
}

FString FDataProxyServer::ToolGenRoot()
{
	return FPaths::ConvertRelativePathToFull(
		FPaths::ProjectContentDir() / TEXT("Data") / TEXT("ToolGen"));
}

// 共享工作流库根：<ToolchainDir>/data/workflow/。与 Node 端 SHARED_WORKFLOW_ROOT
// 完全一致（不含 Harness/，由 rel 自带 Harness/ 前缀拼上）。读 AIGAME_TOOLCHAIN_DIR
// 环境变量定位工具链根；缺省回落到工程根的同级 AiGameTools 目录。
//   多人协作时本共享库在 compute 进程上，多人连同一份；单机开发下也在本机。
FString FDataProxyServer::SharedWorkflowRoot()
{
	FString ToolchainDir = FPlatformMisc::GetEnvironmentVariable(TEXT("AIGAME_TOOLCHAIN_DIR"));
	if (ToolchainDir.IsEmpty())
	{
		// 兜底：假设工程根与工具链仓库同级（Ai_Game 和 AiGameTools 在 D:/UGit/ 下并列）
		ToolchainDir = FPaths::ConvertRelativePathToFull(
			FPaths::ProjectDir() / TEXT("..") / TEXT("AiGameTools"));
		FPaths::CollapseRelativeDirectories(ToolchainDir);
	}
	FString Result = ToolchainDir / TEXT("data") / TEXT("workflow");
	Result = FPaths::ConvertRelativePathToFull(Result);
	FPaths::CollapseRelativeDirectories(Result);
	UE_LOG(LogDataProxy, Log, TEXT("[DataProxy] SharedWorkflowRoot = %s (ToolchainDir=%s)"),
		*Result, *ToolchainDir);
	return Result;
}

// 判断 relPath 是否属于共享工作流库（Harness/ 前缀）
bool FDataProxyServer::IsSharedHarnessPath(const FString& RelPath)
{
	return RelPath.StartsWith(TEXT("Harness/")) || RelPath.Equals(TEXT("Harness"));
}

FString FDataProxyServer::ResolveSandboxed(const FString& RelPath)
{
	// 拒空 / 绝对路径 / 含 ..（双重校验：先字面，再 collapse 后前缀比对）
	if (RelPath.IsEmpty() || RelPath.Contains(TEXT("..")) || FPaths::IsRelative(RelPath) == false)
	{
		return FString();
	}
	// 按前缀分流：Harness/ → 共享工作流库根（不依赖工程根）；其余 → 工程 ToolGen 根
	const FString Root = IsSharedHarnessPath(RelPath) ? SharedWorkflowRoot() : ToolGenRoot();
	FString Abs = FPaths::ConvertRelativePathToFull(Root / RelPath);
	FPaths::CollapseRelativeDirectories(Abs);
	// 归一化后必须仍在 Root 之下
	FString RootWithSlash = Root;
	if (!RootWithSlash.EndsWith(TEXT("/"))) RootWithSlash += TEXT("/");
	if (Abs != Root && !Abs.StartsWith(RootWithSlash))
	{
		return FString();
	}
	return Abs;
}

FString FDataProxyServer::ComputeNotifyUrl()
{
	FString Env = FPlatformMisc::GetEnvironmentVariable(TEXT("COMPUTE_NOTIFY_URL"));
	if (!Env.IsEmpty()) return Env;
	return TEXT("http://127.0.0.1:4005/coop/notify");
}

void FDataProxyServer::MaybeNotifyCompute(const FString& RelPath, int32 Rev, const FString& By)
{
	// 匹配拆分 Story 写路径 → 发协同通知（fire-and-forget，失败不阻塞，一致性靠 git）
	FString Kind, StoryId, NodeId;
	// Harness/Stories/<id>/nodes/<id>.json
	FString P = RelPath.Replace(TEXT("\\"), TEXT("/"));
	if (P.StartsWith(TEXT("Harness/Stories/")))
	{
		FString Rest = P.RightChop(FString(TEXT("Harness/Stories/")).Len());
		TArray<FString> Segs; Rest.ParseIntoArray(Segs, TEXT("/"), true);
		if (Segs.Num() == 3 && Segs[1] == TEXT("nodes") && Segs[2].EndsWith(TEXT(".json")))
		{
			Kind = TEXT("node");
			StoryId = Segs[0];
			NodeId = Segs[2].LeftChop(5); // 去 .json
		}
		else if (Segs.Num() == 2 && Segs[1] == TEXT("_index.json"))
		{
			Kind = TEXT("structure");
			StoryId = Segs[0];
		}
	}
	if (Kind.IsEmpty()) return; // 非 Story 写，不通知

	FString Body;
	if (Kind == TEXT("node"))
	{
		Body = FString::Printf(TEXT("{\"kind\":\"node\",\"storyId\":\"%s\",\"nodeId\":\"%s\",\"rev\":%d,\"by\":\"%s\"}"),
			*StoryId, *NodeId, Rev, *By);
	}
	else
	{
		Body = FString::Printf(TEXT("{\"kind\":\"structure\",\"storyId\":\"%s\",\"by\":\"%s\"}"),
			*StoryId, *By);
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(ComputeNotifyUrl());
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Req->SetContentAsString(Body);
	Req->OnProcessRequestComplete().BindLambda(
		[](FHttpRequestPtr, FHttpResponsePtr, bool bOk)
		{
			// fire-and-forget：不关心结果，失败仅记日志
			if (!bOk) UE_LOG(LogDataProxy, Verbose, TEXT("[notify] compute 通知发送失败(忽略)"));
		});
	Req->ProcessRequest();
}

void FDataProxyServer::Start()
{
	if (bStarted) return;

	// 【B1 架构：UE 桥接】UE C++ 不再顶替 Node data 占 4000，而是恒定占 4100 提供
	//   "UE 引擎专属能力"（资产索引刷新、缩略图渲染、PIE 等），与 Node data(4000) 并存。
	//   工程 JSON 读写（/toolgen/*）全部归 Node data(4000)，UE 不再提供（已删除相关路由）。
	//   端口 4100 固定；env AIGAME_DATAPROXY_PORT 可显式覆盖（调试用）。
	Port = 4100;
	const FString PortEnv = FPlatformMisc::GetEnvironmentVariable(TEXT("AIGAME_DATAPROXY_PORT"));
	if (!PortEnv.IsEmpty())
	{
		const int32 P = FCString::Atoi(*PortEnv);
		if (P > 0 && P < 65536) Port = (uint32)P;
	}

	FHttpServerModule& HttpModule = FHttpServerModule::Get();
	Router = HttpModule.GetHttpRouter(Port);
	if (!Router.IsValid())
	{
		UE_LOG(LogDataProxy, Warning,
			TEXT("[DataProxy] 端口 %u 获取 Router 失败（可能被占用）。UE 桥接未启动。"), Port);
		return;
	}

	BindRoutes();
	HttpModule.StartAllListeners();
	bStarted = true;

	UE_LOG(LogDataProxy, Log, TEXT("[DataProxy] UE 桥接已启动 http://127.0.0.1:%u（B1：UE 专属能力，与 Node data:4000 并存）"),
		Port);
}

void FDataProxyServer::Stop()
{
	if (!bStarted) return;
	// 路由随 Router 释放；监听器统一停。
	FHttpServerModule::Get().StopAllListeners();
	Router.Reset();
	bStarted = false;
	UE_LOG(LogDataProxy, Log, TEXT("[DataProxy] 已停止"));
}

void FDataProxyServer::BindRoutes()
{
	// ── CORS 预检 OPTIONS：所有 data 路径统一返回带 CORS 头的 200 ──
	// 浏览器跨端口 fetch（前端 :2999 → 数据代理 :4000）会先发 OPTIONS 预检，
	// UE HttpServer 对未绑的 verb 返回 404 → 浏览器拒绝实际请求 → 前端 STORIES(0)。
	// 给每个 data 路径绑 OPTIONS，回 200 + CORS 头（body 空即可）。
	auto BindOptions = [this](const TCHAR* Path)
	{
		Router->BindRoute(FHttpPath(Path), EHttpServerRequestVerbs::VERB_OPTIONS,
			FHttpRequestHandler::CreateLambda(
				[](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool
				{
					TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create(FString(), TEXT("application/json"));
					Resp->Code = EHttpServerResponseCodes::Ok;
					AddCors(*Resp); // Access-Control-Allow-Origin/Methods/Headers
					OnComplete(MoveTemp(Resp));
					return true;
				}));
	};
	BindOptions(TEXT("/health"));
	BindOptions(TEXT("/info"));
	BindOptions(TEXT("/assets/refresh"));

	// ── GET /health ──
	Router->BindRoute(FHttpPath(TEXT("/health")), EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateLambda(
			[](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool
			{
				OnComplete(JsonOk(TEXT("{\"ok\":true,\"name\":\"ue-bridge\"}")));
				return true;
			}));

	// ── GET /info ──
	// 【B1 架构】UE 桥接只提供"UE 就绪探测 + UE 专属能力"，不再做 JSON 读写。
	//   前端探测本端点（4100）可达 = UE 已开着 → 点亮资产刷新/渲染类功能。
	//   role=ue-bridge 标明这是 UE 桥接（区别于 Node data 的 role=data）。
	Router->BindRoute(FHttpPath(TEXT("/info")), EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateLambda(
			[](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool
			{
				FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
				FString Json = FString::Printf(
					TEXT("{\"ok\":true,\"role\":\"ue-bridge\",\"impl\":\"ue-cpp\",\"projectRoot\":\"%s\",\"toolGenRoot\":\"%s\"}"),
					*ProjectRoot.ReplaceCharWithEscapedChar(),
					*ToolGenRoot().ReplaceCharWithEscapedChar());
				OnComplete(JsonOk(Json));
				return true;
			}));

	// 【B1 架构】/toolgen/file|list|batch 读写路由已删除 —— 工程 JSON 读写全部归 Node data(4000)。

	// ── POST /assets/refresh ── 【UE-1】刷新资产索引（含缩略图渲染）──
	//   UE 引擎专属能力：扫描 AssetRegistry + 渲染缩略图 PNG，写到
	//   <Project>/Saved/DreamMaker/{asset-index.json, thumbs/}（Node data:4000 的 /ue/assets、
	//   /ue/thumbs 直接读这两处，路径三方天然对齐，无需 Node 改动）。
	//   之前只能在 UE 内嵌 WebView2 里点菜单触发（postMessage）；现暴露成 HTTP，
	//   任意工作台（独立浏览器）都能触发。
	//   线程：FAssetIndexExporter::ExportAll() 会加载 UObject + 走渲染管线，必须在 GameThread。
	//     HttpServer handler 本就在 GameThread，但全量扫描+首次渲缩略图可能耗时数秒~数十秒，
	//     同步跑会卡死当前 tick 帧。故用 AsyncTask(GameThread) 排到后续帧执行，
	//     跑完再用 OnComplete 延迟回结果（真异步 HTTP，不阻塞其他请求）。
	Router->BindRoute(FHttpPath(TEXT("/assets/refresh")), EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateLambda(
			[](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool
			{
				UE_LOG(LogDataProxy, Log, TEXT("[DataProxy] /assets/refresh 收到请求，排队执行 ExportAll"));
				AsyncTask(ENamedThreads::GameThread, [OnComplete]()
				{
					const double T0 = FPlatformTime::Seconds();
					const int32 N = FAssetIndexExporter::ExportAll();
					const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
					const FString Out = FAssetIndexExporter::GetOutputPath();
					if (N < 0)
					{
						UE_LOG(LogDataProxy, Warning, TEXT("[DataProxy] /assets/refresh 失败（ExportAll 返回 %d）"), N);
						OnComplete(JsonErr(EHttpServerResponseCodes::ServerError, TEXT("资产索引写入失败（见 UE 日志）")));
						return;
					}
					UE_LOG(LogDataProxy, Log, TEXT("[DataProxy] /assets/refresh 完成：%d 个资产，耗时 %.0fms"), N, Ms);
					FString Json = FString::Printf(
						TEXT("{\"ok\":true,\"total\":%d,\"elapsedMs\":%.0f,\"outputPath\":\"%s\"}"),
						N, Ms, *Out.ReplaceCharWithEscapedChar());
					OnComplete(JsonOk(Json));
				});
				return true; // 已接管，OnComplete 会在 GameThread 任务完成后异步调用
			}));

	UE_LOG(LogDataProxy, Log, TEXT("[DataProxy] 已绑定路由: /health /info /assets/refresh（B1：UE 桥接，JSON 读写归 Node data:4000）"));
}
