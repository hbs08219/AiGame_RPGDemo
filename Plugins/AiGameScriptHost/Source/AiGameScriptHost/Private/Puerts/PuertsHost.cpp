// UPuertsHost 实现

#include "Puerts/PuertsHost.h"
#include "JsEnv.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY(LogAiGameScriptHost);

void UPuertsHost::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	StartScript();

	// GameInstanceSubsystem 初始化早于 World/LocalPlayer → 轮询到就绪再广播 OnReady
	ReadyTickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateWeakLambda(this, [this](float) -> bool
		{
			return !TryFireReady();   // 就绪后 TryFireReady 返回 true → 这里返回 false 停止轮询
		}), 0.1f);

	FrameTickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateWeakLambda(this, [this](float DeltaSeconds) -> bool
		{
			if (bReadyFired) OnFrame.Broadcast(DeltaSeconds);
			return true;
		}));

	UE_LOG(LogAiGameScriptHost, Log, TEXT("[Puerts] Host initialized"));
}

void UPuertsHost::Deinitialize()
{
	if (ReadyTickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ReadyTickHandle);
		ReadyTickHandle.Reset();
	}
	if (FrameTickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(FrameTickHandle);
		FrameTickHandle.Reset();
	}

	JsEnv.Reset();   // 释放 JS 虚拟机
	bScriptStarted = false;
	bReadyFired = false;

	Super::Deinitialize();
}

void UPuertsHost::StartScript()
{
	if (bScriptStarted) return;

	// ScriptRoot 默认 "JavaScript" → 读 Content/JavaScript/Main.js
	JsEnv = MakeShared<puerts::FJsEnv>();
	if (!JsEnv.IsValid())
	{
		UE_LOG(LogAiGameScriptHost, Error, TEXT("[Puerts] FJsEnv 创建失败"));
		return;
	}

	TArray<TPair<FString, UObject*>> Args;
	Args.Add(TPair<FString, UObject*>(TEXT("Host"), this));
	JsEnv->Start(TEXT("Main"), Args);

	bScriptStarted = true;
	UE_LOG(LogAiGameScriptHost, Log, TEXT("[Puerts] TS 入口 Main 已启动"));
}

bool UPuertsHost::TryFireReady()
{
	if (bReadyFired) return true;

	UWorld* World = GetWorld();
	if (!World) return false;

	UGameInstance* GI = GetGameInstance();
	if (!GI || !GI->GetFirstGamePlayer()) return false;

	bReadyFired = true;
	OnReady.Broadcast();
	UE_LOG(LogAiGameScriptHost, Log, TEXT("[Puerts] OnReady 广播(世界+本地玩家就绪)"));
	return true;
}

UGameInstanceSubsystem* UPuertsHost::GetGameSubsystem(TSubclassOf<UGameInstanceSubsystem> SubsystemClass) const
{
	if (!SubsystemClass) return nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		return GI->GetSubsystemBase(SubsystemClass);
	}
	return nullptr;
}

ULocalPlayerSubsystem* UPuertsHost::GetLocalPlayerSubsystem(TSubclassOf<ULocalPlayerSubsystem> SubsystemClass) const
{
	if (!SubsystemClass) return nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (ULocalPlayer* LP = GI->GetFirstGamePlayer())
		{
			return LP->GetSubsystemBase(SubsystemClass);
		}
	}
	return nullptr;
}

UUserWidget* UPuertsHost::SpawnWidget(const FString& WidgetClassPath, int32 ZOrder)
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	UClass* WClass = LoadClass<UUserWidget>(nullptr, *WidgetClassPath);
	if (!WClass)
	{
		UE_LOG(LogAiGameScriptHost, Warning, TEXT("[Puerts] SpawnWidget: WBP 类未找到 %s"), *WidgetClassPath);
		return nullptr;
	}

	UUserWidget* Widget = CreateWidget<UUserWidget>(World, WClass);
	if (Widget)
	{
		Widget->AddToViewport(ZOrder);
	}
	return Widget;
}

void UPuertsHost::RemoveWidget(UUserWidget* Widget)
{
	if (Widget)
	{
		Widget->RemoveFromParent();
	}
}

FString UPuertsHost::LoadToolGenJson(const FString& RelPathUnderToolGen) const
{
	const FString FullPath = FPaths::ProjectContentDir() / TEXT("Data/ToolGen") / RelPathUnderToolGen;
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *FullPath))
	{
		UE_LOG(LogAiGameScriptHost, Warning, TEXT("[Puerts] LoadToolGenJson 未找到: %s"), *FullPath);
		return FString();
	}
	return Json;
}
