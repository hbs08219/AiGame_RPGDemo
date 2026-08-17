// 无头资产同步 — 优雅停机 MCP 工具实现

#include "HeadlessSync/Tools/ShutdownHeadlessSyncTool.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "JsonDomBuilder.h"
#include "ModelContextProtocolToolResults.h"

TSharedPtr<FJsonObject> FShutdownHeadlessSyncTool::GetInputJsonSchema() const
{
	FJsonDomBuilder::FObject Schema;
	Schema.Set(TEXT("type"), TEXT("object"));
	Schema.Set(TEXT("properties"), FJsonDomBuilder::FObject());
	return Schema.AsJsonObject().ToSharedPtr();
}

FModelContextProtocolToolResult FShutdownHeadlessSyncTool::Run(const TSharedPtr<FJsonObject>& Params)
{
	using namespace UE::ModelContextProtocol;
	if (!RequestShutdown)
	{
		return MakeErrorResult(TEXT("shutdown_headless_sync: 停机回调未配置"));
	}

	// 该函数由 MCP 在 commandlet 主线程执行。若 import_asset 正在运行，当前请求会
	// 留在 HTTP 队列中，直到导入返回后才执行；因此不会中断正在写入的 UE 资产。
	RequestShutdown();

	FJsonDomBuilder::FObject Payload;
	Payload.Set(TEXT("accepted"), true);
	Payload.Set(TEXT("message"), TEXT("headless sync shutdown accepted after current batch"));
	const TSharedPtr<FJsonValue> StructuredContent = MakeShared<FJsonValueObject>(Payload.AsJsonObject());
	return MakeStructuredContentResult(StructuredContent);
}
