// 无头资产同步 — 手动清理 Cache 资产 MCP 工具实现

#include "HeadlessSync/Tools/DeleteCachedAssetTool.h"

#include "AssetRegistry/AssetData.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "JsonDomBuilder.h"
#include "ModelContextProtocolToolResults.h"
#include "ObjectTools.h"
#include "UObject/SoftObjectPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogAiGameHeadlessCacheCleanup, Log, All);

namespace
{
	constexpr TCHAR CacheRoot[] = TEXT("/Game/AiGame/Cache/");

	bool IsCacheObjectPath(const FString& ObjectPath)
	{
		const int32 DotIndex = ObjectPath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		return DotIndex > 0 && DotIndex < ObjectPath.Len() - 1 && ObjectPath.StartsWith(CacheRoot);
	}
}

TSharedPtr<FJsonObject> FDeleteCachedAssetTool::GetInputJsonSchema() const
{
	FJsonDomBuilder::FObject Schema;
	Schema.Set(TEXT("type"), TEXT("object"));
	FJsonDomBuilder::FObject Props;
	Props.Set(TEXT("objectPath"), FJsonDomBuilder::FObject().Set(TEXT("type"), TEXT("string")));
	Props.Set(TEXT("sourceSha256"), FJsonDomBuilder::FObject().Set(TEXT("type"), TEXT("string")));
	Schema.Set(TEXT("properties"), Props);
	FJsonDomBuilder::FArray Required;
	Required.Add(TEXT("objectPath"));
	Schema.Set(TEXT("required"), Required);
	return Schema.AsJsonObject().ToSharedPtr();
}

FModelContextProtocolToolResult FDeleteCachedAssetTool::Run(const TSharedPtr<FJsonObject>& Params)
{
	using namespace UE::ModelContextProtocol;
	if (!Params.IsValid()) return MakeErrorResult(TEXT("delete_cached_asset: 参数为空"));

	FString ObjectPath;
	FString SourceSha256;
	if (!Params->TryGetStringField(TEXT("objectPath"), ObjectPath) || !IsCacheObjectPath(ObjectPath)) {
		return MakeErrorResult(TEXT("delete_cached_asset: objectPath 必须位于 /Game/AiGame/Cache"));
	}
	Params->TryGetStringField(TEXT("sourceSha256"), SourceSha256);

	UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath);
	if (!Asset) return MakeErrorResult(FString::Printf(TEXT("delete_cached_asset: 未找到缓存资产 %s"), *ObjectPath));

	TArray<FAssetData> Assets;
	Assets.Emplace(Asset);
	const int32 DeletedCount = ObjectTools::DeleteAssets(Assets, false);
	if (DeletedCount != 1) {
		return MakeErrorResult(FString::Printf(TEXT("delete_cached_asset: 删除失败 %s"), *ObjectPath));
	}

	FJsonDomBuilder::FObject Payload;
	Payload.Set(TEXT("deleted"), true);
	Payload.Set(TEXT("objectPath"), ObjectPath);
	if (!SourceSha256.IsEmpty()) Payload.Set(TEXT("checksum"), SourceSha256);
	Payload.Set(TEXT("message"), TEXT("cached asset deleted"));
	const TSharedPtr<FJsonValue> StructuredContent = MakeShared<FJsonValueObject>(Payload.AsJsonObject());
	UE_LOG(LogAiGameHeadlessCacheCleanup, Log, TEXT("delete_cached_asset: %s"), *ObjectPath);
	return MakeStructuredContentResult(StructuredContent);
}
