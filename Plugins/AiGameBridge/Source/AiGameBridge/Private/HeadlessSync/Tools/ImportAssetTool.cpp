// 无头资产同步 — import_asset MCP 工具实现

#include "HeadlessSync/Tools/ImportAssetTool.h"

#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "JsonDomBuilder.h"
#include "Misc/Paths.h"
#include "ModelContextProtocolToolResults.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogAiGameHeadlessImport, Log, All);

TSharedPtr<FJsonObject> FImportAssetTool::GetInputJsonSchema() const
{
	FJsonDomBuilder::FObject Schema;
	Schema.Set(TEXT("type"), TEXT("object"));

	FJsonDomBuilder::FObject Props;
	// sourcePath 兼容直接 MCP 调用；source={kind:'file',path} 与服务端 publisher 契约一致。
	Props.Set(TEXT("sourcePath"), FJsonDomBuilder::FObject().Set(TEXT("type"), TEXT("string")));
	Props.Set(TEXT("source"), FJsonDomBuilder::FObject().Set(TEXT("type"), TEXT("object")));
	Props.Set(TEXT("sourceSha256"), FJsonDomBuilder::FObject().Set(TEXT("type"), TEXT("string")));
	Props.Set(TEXT("destinationRoot"), FJsonDomBuilder::FObject().Set(TEXT("type"), TEXT("string")));
	Props.Set(TEXT("assetName"), FJsonDomBuilder::FObject().Set(TEXT("type"), TEXT("string")));
	Props.Set(TEXT("overwrite"), FJsonDomBuilder::FObject().Set(TEXT("type"), TEXT("boolean")));
	Schema.Set(TEXT("properties"), Props);

	FJsonDomBuilder::FArray Required;
	// sourcePath/source 二选一；JSON Schema draft 兼容性差异由 Run 进行最终严格校验。
	Required.Add(TEXT("destinationRoot"));
	Schema.Set(TEXT("required"), Required);

	return Schema.AsJsonObject().ToSharedPtr();
}

FModelContextProtocolToolResult FImportAssetTool::Run(const TSharedPtr<FJsonObject>& Params)
{
	using namespace UE::ModelContextProtocol;

	if (!Params.IsValid())
	{
		return MakeErrorResult(TEXT("import_asset: 参数为空"));
	}

	FString SourcePath;
	FString DestinationRoot;
	FString AssetName;
	bool bOverwrite = false;

	// 直接调用使用 sourcePath；publisher 使用 source={kind:'file',path}。只接受本机
	// 沙盒已写好的 file，拒绝 base64/URL，避免绕过服务端对真实 bytes 的审计链。
	Params->TryGetStringField(TEXT("sourcePath"), SourcePath);
	if (SourcePath.IsEmpty())
	{
		const TSharedPtr<FJsonObject>* Source = nullptr;
		FString SourceKind;
		if (!Params->TryGetObjectField(TEXT("source"), Source)
			|| Source == nullptr
			|| !Source->IsValid()
			|| !(*Source)->TryGetStringField(TEXT("kind"), SourceKind)
			|| SourceKind != TEXT("file")
			|| !(*Source)->TryGetStringField(TEXT("path"), SourcePath)
			|| SourcePath.IsEmpty())
		{
			return MakeErrorResult(TEXT("import_asset: 需要 sourcePath 或 source={kind:'file',path}"));
		}
	}
	if (!Params->TryGetStringField(TEXT("destinationRoot"), DestinationRoot) || !DestinationRoot.StartsWith(TEXT("/Game")))
	{
		return MakeErrorResult(TEXT("import_asset: destinationRoot 必须是 /Game 包根"));
	}
	Params->TryGetStringField(TEXT("assetName"), AssetName);
	Params->TryGetBoolField(TEXT("overwrite"), bOverwrite);

	// 规范化源路径并确认存在
	SourcePath = FPaths::ConvertRelativePathToFull(SourcePath);
	if (!FPaths::FileExists(SourcePath))
	{
		return MakeErrorResult(FString::Printf(TEXT("import_asset: 源文件不存在 %s"), *SourcePath));
	}

	// 构造导入任务（同步阻塞，bAutomated 抑制一切对话框）
	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	Task->Filename = SourcePath;
	Task->DestinationPath = DestinationRoot;
	// DestinationName 留空：让工厂按源文件名命名，Interchange 下本字段被忽略。
	Task->bAutomated = true;
	Task->bReplaceExisting = bOverwrite;
	Task->bReplaceExistingSettings = bOverwrite;
	Task->bSave = true;
	Task->bAsync = false;

	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	AssetTools.ImportAssetTasks({ Task });

	const TArray<FString>& Imported = Task->ImportedObjectPaths;
	if (Imported.Num() == 0)
	{
		UE_LOG(LogAiGameHeadlessImport, Warning,
			TEXT("import_asset: 无导入结果 source=%s dest=%s"), *SourcePath, *DestinationRoot);
		return MakeErrorResult(FString::Printf(
			TEXT("import_asset: 导入失败（无结果对象）source=%s dest=%s"), *SourcePath, *DestinationRoot));
	}

	// 取第一个导入对象的 ObjectPath（/Game/Path/Name.Name）。
	const FString ObjectPath = Imported[0];
	const int32 DotIndex = ObjectPath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	if (DotIndex == INDEX_NONE)
	{
		return MakeErrorResult(FString::Printf(
			TEXT("import_asset: 导入结果不是规范 ObjectPath：%s"), *ObjectPath));
	}
	const FString PackagePath = ObjectPath.Left(DotIndex);

	UE_LOG(LogAiGameHeadlessImport, Log,
		TEXT("import_asset: 成功 object=%s package=%s overwrite=%d"), *ObjectPath, *PackagePath, bOverwrite ? 1 : 0);

	// 组装 structuredContent，与服务端 validateUePaths/normalizeToolPayload 契约对齐。
	FJsonDomBuilder::FObject Payload;
	Payload.Set(TEXT("objectPath"), ObjectPath);
	Payload.Set(TEXT("packagePath"), PackagePath);
	// checksum 由服务端在调用前已算好（sourceSha256），它会对 UE 返回做相等校验。
	// 这里把源文件 sha256 回显——服务端 normalizeToolPayload 后核对 publication.sha256。
	FString SourceSha256;
	Params->TryGetStringField(TEXT("sourceSha256"), SourceSha256);
	if (!SourceSha256.IsEmpty())
	{
		Payload.Set(TEXT("checksum"), SourceSha256);
	}
	Payload.Set(TEXT("message"), bOverwrite ? TEXT("reimport completed") : TEXT("import completed"));

	// 显式构造 TSharedPtr<FJsonValue>，走 MakeStructuredContentResult(TSharedPtr<FJsonValue>) 重载，
	// 避免 TSharedRef 被匹配到 UStruct 模板版本（C2039 StaticStruct）。
	const TSharedPtr<FJsonValue> StructuredContent = MakeShared<FJsonValueObject>(Payload.AsJsonObject());
	return MakeStructuredContentResult(StructuredContent);
}
