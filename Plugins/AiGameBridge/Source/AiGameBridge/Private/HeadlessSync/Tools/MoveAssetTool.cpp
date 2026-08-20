// 无头资产同步 — Published/Cache 资产移动 MCP 工具实现

#include "HeadlessSync/Tools/MoveAssetTool.h"

#include "AssetToolsModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "IAssetTools.h"
#include "JsonDomBuilder.h"
#include "Misc/PackageName.h"
#include "ModelContextProtocolToolResults.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogAiGameHeadlessMove, Log, All);

namespace
{
	constexpr TCHAR PublishedRoot[] = TEXT("/Game/AiGame/Published");
	constexpr TCHAR MoveCacheRoot[] = TEXT("/Game/AiGame/Cache");

	bool IsCanonicalObjectPath(const FString& ObjectPath)
	{
		const int32 DotIndex = ObjectPath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (DotIndex <= 0 || DotIndex >= ObjectPath.Len() - 1) return false;
		const FString PackagePath = ObjectPath.Left(DotIndex);
		const FString AssetName = ObjectPath.Mid(DotIndex + 1);
		return PackagePath.StartsWith(TEXT("/Game/")) && !AssetName.Contains(TEXT("/"));
	}

	bool IsManagedRoot(const FString& Path)
	{
		return Path == PublishedRoot || Path.StartsWith(FString(PublishedRoot) + TEXT("/"))
			|| Path == MoveCacheRoot || Path.StartsWith(FString(MoveCacheRoot) + TEXT("/"));
	}

	FModelContextProtocolToolResult MakeMoveResult(const FString& ObjectPath, const FString& PackagePath, const FString& Checksum, const FString& Message)
	{
		using namespace UE::ModelContextProtocol;
		FJsonDomBuilder::FObject Payload;
		Payload.Set(TEXT("objectPath"), ObjectPath);
		Payload.Set(TEXT("packagePath"), PackagePath);
		if (!Checksum.IsEmpty()) Payload.Set(TEXT("checksum"), Checksum);
		Payload.Set(TEXT("message"), Message);
		const TSharedPtr<FJsonValue> StructuredContent = MakeShared<FJsonValueObject>(Payload.AsJsonObject());
		return MakeStructuredContentResult(StructuredContent);
	}
}

TSharedPtr<FJsonObject> FMoveAssetTool::GetInputJsonSchema() const
{
	FJsonDomBuilder::FObject Schema;
	Schema.Set(TEXT("type"), TEXT("object"));
	FJsonDomBuilder::FObject Props;
	Props.Set(TEXT("objectPath"), FJsonDomBuilder::FObject().Set(TEXT("type"), TEXT("string")));
	Props.Set(TEXT("destinationRoot"), FJsonDomBuilder::FObject().Set(TEXT("type"), TEXT("string")));
	Props.Set(TEXT("sourceSha256"), FJsonDomBuilder::FObject().Set(TEXT("type"), TEXT("string")));
	Schema.Set(TEXT("properties"), Props);
	FJsonDomBuilder::FArray Required;
	Required.Add(TEXT("objectPath"));
	Required.Add(TEXT("destinationRoot"));
	Schema.Set(TEXT("required"), Required);
	return Schema.AsJsonObject().ToSharedPtr();
}

FModelContextProtocolToolResult FMoveAssetTool::Run(const TSharedPtr<FJsonObject>& Params)
{
	using namespace UE::ModelContextProtocol;
	if (!Params.IsValid()) return MakeErrorResult(TEXT("move_asset: 参数为空"));

	FString ObjectPath;
	FString DestinationRoot;
	FString SourceSha256;
	if (!Params->TryGetStringField(TEXT("objectPath"), ObjectPath) || !IsCanonicalObjectPath(ObjectPath)) {
		return MakeErrorResult(TEXT("move_asset: objectPath 必须是规范 /Game/.../Asset.Asset"));
	}
	if (!Params->TryGetStringField(TEXT("destinationRoot"), DestinationRoot) || !IsManagedRoot(DestinationRoot)) {
		return MakeErrorResult(TEXT("move_asset: destinationRoot 只允许 /Game/AiGame/Published 或 /Game/AiGame/Cache"));
	}
	Params->TryGetStringField(TEXT("sourceSha256"), SourceSha256);

	const int32 DotIndex = ObjectPath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	const FString CurrentPackagePath = ObjectPath.Left(DotIndex);
	if (!IsManagedRoot(CurrentPackagePath)) {
		return MakeErrorResult(TEXT("move_asset: 只能移动 AiGame Published/Cache 下的资产"));
	}

	UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath);
	if (!Asset) return MakeErrorResult(FString::Printf(TEXT("move_asset: 未找到资产 %s"), *ObjectPath));

	const FString CurrentFolder = FPackageName::GetLongPackagePath(CurrentPackagePath);
	if (CurrentFolder == DestinationRoot) {
		return MakeMoveResult(ObjectPath, CurrentPackagePath, SourceSha256, TEXT("already at destination"));
	}

	FAssetRenameData RenameData(Asset, DestinationRoot, Asset->GetName());
	TArray<FAssetRenameData> RenameBatch;
	RenameBatch.Add(MoveTemp(RenameData));
	if (!FAssetToolsModule::GetModule().Get().RenameAssets(RenameBatch)) {
		return MakeErrorResult(FString::Printf(TEXT("move_asset: UE 重命名失败 %s → %s"), *ObjectPath, *DestinationRoot));
	}

	const FString NewName = Asset->GetName();
	const FString NewPackagePath = FString::Printf(TEXT("%s/%s"), *DestinationRoot, *NewName);
	const FString NewObjectPath = FString::Printf(TEXT("%s.%s"), *NewPackagePath, *NewName);
	UE_LOG(LogAiGameHeadlessMove, Log, TEXT("move_asset: %s → %s"), *ObjectPath, *NewObjectPath);
	return MakeMoveResult(NewObjectPath, NewPackagePath, SourceSha256, TEXT("move completed"));
}
