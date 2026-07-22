// AssetIndexExporter 实现

#include "AssetIndexExporter.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"

#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/FrameRate.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "ObjectTools.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "ThumbnailHelpers.h"
#include "ImageUtils.h"

#include "Animation/AnimSequence.h"
#include "NiagaraSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogDreamMakerIdx, Log, All);

namespace
{
	/** 我们支持的资产类型白名单（前端 AssetRef.assetClass 与之对齐） */
	static const TArray<FName>& GetSupportedClasses()
	{
		static const TArray<FName> Classes = {
			TEXT("AnimSequence"),
			TEXT("AnimMontage"),
			TEXT("NiagaraSystem"),
			TEXT("SoundCue"),
			TEXT("SoundWave"),
			TEXT("StaticMesh"),
			TEXT("SkeletalMesh"),
			TEXT("Skeleton"),           // 角色外观编辑器：选择 Skeleton 资产
			TEXT("MaterialInterface"),  // 角色外观编辑器：选择材质（Material/MaterialInstance 都派生自此）
		};
		return Classes;
	}

	/** 把 ObjectPath 转成安全的文件名（用于缩略图 PNG） */
	static FString SanitizeForFileName(const FString& In)
	{
		FString Out = In;
		Out.ReplaceInline(TEXT("/"), TEXT("_"));
		Out.ReplaceInline(TEXT("\\"), TEXT("_"));
		Out.ReplaceInline(TEXT("."), TEXT("__"));
		Out.ReplaceInline(TEXT(":"), TEXT("_"));
		Out.ReplaceInline(TEXT("?"), TEXT("_"));
		Out.ReplaceInline(TEXT("*"), TEXT("_"));
		Out.ReplaceInline(TEXT("<"), TEXT("_"));
		Out.ReplaceInline(TEXT(">"), TEXT("_"));
		Out.ReplaceInline(TEXT("|"), TEXT("_"));
		Out.ReplaceInline(TEXT("\""), TEXT("_"));
		// 限长（Windows 文件名 255 字符上限，留余地）
		if (Out.Len() > 200) Out.LeftChopInline(Out.Len() - 200);
		return Out;
	}

	/** 缩略图 PNG 输出目录 */
	static FString GetThumbsDir()
	{
		return FPaths::ConvertRelativePathToFull(
			FPaths::ProjectSavedDir() / TEXT("DreamMaker") / TEXT("thumbs")
		);
	}

	/**
	 * 尝试导出某个资产的缩略图为 PNG
	 * 返回缩略图文件名（仅文件名，不含路径），失败返回空串
	 *
	 * 策略：
	 * 1) 先尝试从 package 缓存的 thumbnail 读（ConditionallyLoadThumbnailsForObjects）
	 * 2) 如果 package 内没缓存，加载对象 + ThumbnailTools::RenderThumbnail 实时渲染
	 */
	static FString ExportThumbnailPNG(const FAssetData& Asset, const FString& ThumbsDir, FString& OutFailReason)
	{
		const FString FullName = Asset.GetFullName();   // "Class /Game/Path/Name.Name"
		const FName FullNameName(*FullName);

		FObjectThumbnail CachedFallback;   // 实时渲染时的输出容器
		FObjectThumbnail* ThumbPtr = nullptr;

		// 1) 试 package 缓存
		FThumbnailMap ThumbMap;
		TArray<FName> ObjectFullNames;
		ObjectFullNames.Add(FullNameName);
		ThumbnailTools::ConditionallyLoadThumbnailsForObjects(ObjectFullNames, ThumbMap);
		ThumbPtr = ThumbMap.Find(FullNameName);

		// 2) 没有 / 像素数据空 → 实时渲染
		// 注意：ConditionallyLoadThumbnailsForObjects 可能返回有尺寸但 0 像素的占位 thumbnail
		// 所以光看 IsEmpty() 不够，必须同时检查像素数组
		const bool bNeedRender =
			!ThumbPtr ||
			ThumbPtr->IsEmpty() ||
			ThumbPtr->AccessImageData().Num() == 0;
		if (bNeedRender)
		{
			UObject* Obj = Asset.GetAsset();
			if (!Obj)
			{
				OutFailReason = TEXT("GetAsset returned null");
				return FString();
			}

			ThumbnailTools::RenderThumbnail(
				Obj, 128, 128,
				ThumbnailTools::EThumbnailTextureFlushMode::AlwaysFlush,
				/*RT*/nullptr, &CachedFallback
			);
			ThumbPtr = &CachedFallback;
		}

		if (!ThumbPtr || ThumbPtr->IsEmpty())
		{
			OutFailReason = bNeedRender ? TEXT("RenderThumbnail produced empty") : TEXT("cached but empty");
			return FString();
		}

		const TArray<uint8>& Pixels = ThumbPtr->AccessImageData();
		const int32 W = ThumbPtr->GetImageWidth();
		const int32 H = ThumbPtr->GetImageHeight();
		if (Pixels.Num() == 0 || W <= 0 || H <= 0)
		{
			OutFailReason = FString::Printf(TEXT("zero pixels (W=%d H=%d N=%d)"), W, H, Pixels.Num());
			return FString();
		}

		const FColor* ColorData = reinterpret_cast<const FColor*>(Pixels.GetData());
		TArray64<uint8> PngData;
		FImageView View(ColorData, W, H, EGammaSpace::sRGB);
		if (!FImageUtils::CompressImage(PngData, TEXT("png"), View))
		{
			OutFailReason = TEXT("CompressImage failed");
			return FString();
		}

		const FString SafeName = SanitizeForFileName(Asset.GetObjectPathString()) + TEXT(".png");
		const FString OutPath = ThumbsDir / SafeName;
		if (!FFileHelper::SaveArrayToFile(MoveTemp(PngData), *OutPath))
		{
			OutFailReason = FString::Printf(TEXT("SaveArrayToFile failed: %s"), *OutPath);
			return FString();
		}

		return SafeName;
	}

	TSharedPtr<FJsonObject> AssetToJson(const FAssetData& Asset, const FString& ThumbnailFile)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("objectPath"), Asset.GetObjectPathString());
		const FString ClassName = Asset.AssetClassPath.GetAssetName().ToString();
		Obj->SetStringField(TEXT("assetClass"), ClassName);
		Obj->SetStringField(TEXT("displayName"), Asset.AssetName.ToString());
		Obj->SetStringField(TEXT("packagePath"), Asset.PackagePath.ToString());
		if (!ThumbnailFile.IsEmpty())
		{
			Obj->SetStringField(TEXT("thumbnail"), ThumbnailFile);
		}

		// AnimSequence 额外导出帧率信息（用于事件「使用动画帧数」勾选）
		if (ClassName == TEXT("AnimSequence"))
		{
			if (UAnimSequence* Seq = Cast<UAnimSequence>(Asset.GetAsset()))
			{
				const int32 NumKeys = Seq->GetNumberOfSampledKeys();
				const FFrameRate FrameRate = Seq->GetSamplingFrameRate();
				if (NumKeys > 0)
				{
					Obj->SetNumberField(TEXT("numFrames"), NumKeys);
				}
				if (FrameRate.Numerator > 0 && FrameRate.Denominator > 0)
				{
					const double Fps = FrameRate.AsDecimal();
					Obj->SetNumberField(TEXT("frameRate"), Fps);
				}
			}
		}

		// NiagaraSystem 额外导出 loop 标签（前端用于区分循环/非循环特效）
		if (ClassName == TEXT("NiagaraSystem"))
		{
			if (UNiagaraSystem* NS = Cast<UNiagaraSystem>(Asset.GetAsset()))
			{
				Obj->SetBoolField(TEXT("loop"), NS->IsLooping());
			}
		}

		return Obj;
	}
}

FString FAssetIndexExporter::GetOutputPath()
{
	return FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("DreamMaker") / TEXT("asset-index.json")
	);
}

int32 FAssetIndexExporter::ExportAll()
{
	IAssetRegistry& Registry = FAssetRegistryModule::GetRegistry();

	if (Registry.IsLoadingAssets())
	{
		UE_LOG(LogDreamMakerIdx, Warning,
			TEXT("[AssetIndex] AssetRegistry 仍在加载，本次跳过"));
		return 0;
	}

	// 确保缩略图目录存在（不再每次清空，改为增量更新）
	const FString ThumbsDir = GetThumbsDir();
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	PF.CreateDirectoryTree(*ThumbsDir);

	TMap<FName, TArray<TSharedPtr<FJsonValue>>> ByClass;
	int32 Total = 0;
	int32 ThumbnailCount = 0;

	for (const FName& ClassName : GetSupportedClasses())
	{
		FARFilter Filter;
		Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), ClassName));
		Filter.bRecursiveClasses = true;

		TArray<FAssetData> Found;
		Registry.GetAssets(Filter, Found);

		// 兜底：有些类不在 Engine 包里
		if (Found.Num() == 0)
		{
			static const TArray<FString> ExtraModules = {
				TEXT("/Script/Niagara"),
				TEXT("/Script/Engine"),
				TEXT("/Script/AnimGraphRuntime"),
			};
			for (const FString& ModulePath : ExtraModules)
			{
				FARFilter F2;
				F2.ClassPaths.Add(FTopLevelAssetPath(*ModulePath, ClassName));
				F2.bRecursiveClasses = true;
				TArray<FAssetData> More;
				Registry.GetAssets(F2, More);
				Found.Append(More);
			}
		}

		TArray<TSharedPtr<FJsonValue>>& Bucket = ByClass.FindOrAdd(ClassName);
		for (const FAssetData& A : Found)
		{
			const FString PkgPath = A.PackagePath.ToString();
			if (PkgPath.StartsWith(TEXT("/Engine/")) ||
				PkgPath.StartsWith(TEXT("/Temp/")) ||
				PkgPath.StartsWith(TEXT("/Memory/")))
			{
				continue;
			}

			// 增量更新：如果缩略图 PNG 已存在就跳过，避免重复触发着色器编译
			const FString ExpectedName = SanitizeForFileName(A.GetObjectPathString()) + TEXT(".png");
			const FString ExistingPath = ThumbsDir / ExpectedName;
			FString ThumbFile;
			FString FailReason;
			if (PF.FileExists(*ExistingPath))
			{
				ThumbFile = ExpectedName;
			}
			else
			{
				ThumbFile = ExportThumbnailPNG(A, ThumbsDir, FailReason);
			}
			if (!ThumbFile.IsEmpty())
			{
				++ThumbnailCount;
			}
			else if (!FailReason.IsEmpty())
			{
				// 只对前若干次失败打 Warning，避免刷爆日志
				static int32 FailLogQuota = 5;
				if (FailLogQuota > 0)
				{
					UE_LOG(LogDreamMakerIdx, Warning,
						TEXT("[AssetIndex] thumb fail: %s (%s) → %s"),
						*A.AssetName.ToString(), *ClassName.ToString(), *FailReason);
					--FailLogQuota;
				}
			}

			Bucket.Add(MakeShared<FJsonValueObject>(AssetToJson(A, ThumbFile)));
			++Total;
		}
	}

	// 组装最终 JSON
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 4);   // bumped: NiagaraSystem 加 loop
	Root->SetStringField(TEXT("generatedAt"), FDateTime::UtcNow().ToIso8601());
	Root->SetNumberField(TEXT("totalCount"), Total);
	Root->SetNumberField(TEXT("thumbnailCount"), ThumbnailCount);

	TSharedPtr<FJsonObject> AssetsByClass = MakeShared<FJsonObject>();
	for (const auto& Kv : ByClass)
	{
		AssetsByClass->SetArrayField(Kv.Key.ToString(), Kv.Value);
	}
	Root->SetObjectField(TEXT("assetsByClass"), AssetsByClass);

	const FString OutputPath = GetOutputPath();
	const FString OutputDir = FPaths::GetPath(OutputPath);
	if (!PF.DirectoryExists(*OutputDir))
	{
		PF.CreateDirectoryTree(*OutputDir);
	}

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

	if (!FFileHelper::SaveStringToFile(JsonString, *OutputPath))
	{
		UE_LOG(LogDreamMakerIdx, Error,
			TEXT("[AssetIndex] 写文件失败：%s"), *OutputPath);
		return -1;
	}

	UE_LOG(LogDreamMakerIdx, Log,
		TEXT("[AssetIndex] 已导出 %d 个资产 / %d 个缩略图 → %s"),
		Total, ThumbnailCount, *OutputPath);
	return Total;
}
