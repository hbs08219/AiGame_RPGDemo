#include "RpgDemoGameMode.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PointLight.h"
#include "Engine/SkyLight.h"
#include "Engine/SpotLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RpgDemoCharacter.h"
#include "RpgDemoHUD.h"
#include "RpgDemoNpc.h"
#include "RpgDemoSaveGame.h"
#include "RpgDemoToolGenSubsystem.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    ARpgDemoHUD* GetDemoHud(const UObject* WorldContext)
    {
        if (APlayerController* Controller = UGameplayStatics::GetPlayerController(WorldContext, 0))
        {
            return Cast<ARpgDemoHUD>(Controller->GetHUD());
        }
        return nullptr;
    }
}

ARpgDemoGameMode::ARpgDemoGameMode()
{
    DefaultPawnClass = ARpgDemoCharacter::StaticClass();
    HUDClass = ARpgDemoHUD::StaticClass();
}

void ARpgDemoGameMode::BeginPlay()
{
    Super::BeginPlay();

    URpgDemoToolGenSubsystem* ToolGen = GetToolGen();
    if (ToolGen && ToolGen->LoadToolGenData())
    {
        if (ARpgDemoCharacter* Player = Cast<ARpgDemoCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
        {
            Player->ApplyToolGenConfig(ToolGen);
        }
        const FRpgToolGenAttributes& Attributes = ToolGen->GetPlayerAttributes();
        PlayerHealth = Attributes.Health;
        PlayerMana = Attributes.Mana;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[RpgDemo][ToolGen] 加载失败：%s"), ToolGen ? *ToolGen->GetLastError() : TEXT("GameInstance 子系统不可用"));
    }

    SpawnDemoWorld();

    // 世界 Adapter：关卡编辑器标记的正式建筑必须提供 Pawn 阻挡；玩法 Kernel 不感知 UE 网格或碰撞实现。
    int32 CollisionBuildingCount = 0;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor || !Actor->ActorHasTag(TEXT("AGMakerFormalBuilding"))) continue;

        TArray<UStaticMeshComponent*> Meshes;
        Actor->GetComponents<UStaticMeshComponent>(Meshes);
        for (UStaticMeshComponent* Mesh : Meshes)
        {
            if (!Mesh || !Mesh->GetStaticMesh()) continue;
            Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
            Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
            ++CollisionBuildingCount;
        }
    }
    UE_LOG(LogTemp, Display, TEXT("[RpgDemo][World Adapter] Enabled Pawn collision on %d formal building meshes."), CollisionBuildingCount);

    ValidateDemoData();
    UpdateObjective();
    SetToast(TEXT("欢迎来到雾港。靠近角色，按 [E] 交互。"));
}

void ARpgDemoGameMode::SpawnProp(const FVector& Location, const FVector& Scale)
{
    AStaticMeshActor* Prop = GetWorld()->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
    if (!Prop) return;
    if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
    {
        Prop->GetStaticMeshComponent()->SetStaticMesh(Cube);
    }
    Prop->SetActorScale3D(Scale);
    Prop->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
}

ARpgDemoNpc* ARpgDemoGameMode::SpawnNpc(const FString& Id, const FString& Name, const FVector& Location, const FLinearColor& Color)
{
    ARpgDemoNpc* Npc = GetWorld()->SpawnActor<ARpgDemoNpc>(Location, FRotator::ZeroRotator);
    if (!Npc) return nullptr;

    const FRpgToolGenNpc* Definition = GetToolGen() ? GetToolGen()->FindNpc(Id) : nullptr;
    Npc->Configure(Id, Definition ? Definition->Name : Name, Color, Definition ? Definition->InteractRadius : 200.f);
    return Npc;
}

void ARpgDemoGameMode::SpawnDemoWorld()
{
    // 雾港小镇（LV_MistportTown_08）场景优化：
    // 1) 补光照与氛围 —— 静态地图原本没有动态光源，这里统一补上晨光/天光/海雾；
    // 2) 重整地面布局 —— 用 BasicShapes 搭出中心广场、十字道路、市集、药棚、
    //    铁匠铺、矿区入口与东侧外海码头，让小镇有清晰的功能分区与动线。
    // NPC id 与交互逻辑保持不变，只调整摆放位置以对齐各功能区。

    // ── 光照与氛围 ───────────────────────────────────────────────────────
    // 太阳、天光与雾全部是关卡资产，由关卡编辑器 / Unreal 场景直接配置并保存。
    // Gameplay 运行时不得创建、查找或覆盖这些环境 Actor，避免 PIE 与场景资产互相覆盖。
    // 灯笼点光（暖色，界定建筑与路口）
    const auto SpawnLantern = [this](const FVector& Location, const FLinearColor& Color, float Intensity)
    {
        if (APointLight* Lantern = GetWorld()->SpawnActor<APointLight>(Location, FRotator::ZeroRotator))
        {
            Lantern->GetLightComponent()->SetMobility(EComponentMobility::Movable);
            Lantern->GetLightComponent()->SetLightColor(Color);
            Lantern->GetLightComponent()->SetIntensity(Intensity);
        }
    };

    // 材质色板（Cube 默认边长 100cm，Scale 即实际尺寸/100）
    const FLinearColor StoneDark(.20f, .21f, .23f);   // 深石板主路
    const FLinearColor StoneLight(.30f, .31f, .33f);  // 浅石板广场
    const FLinearColor Wood(.34f, .24f, .17f);        // 木色栈道/围栏
    const FLinearColor Water(.12f, .24f, .42f);       // 海水
    const FLinearColor Sand(.40f, .36f, .26f);        // 沙地
    const FLinearColor Rock(.26f, .27f, .29f);        // 矿岩

    // ── 地面与布局重整（X=东西，西负东正；Y=南北，南负北正；Z=0 地表）────
    // 正式地面与道路已固化在关卡的 AGMaker 模块化 HISM 批次中；运行时不再叠加白模底板与道路。
    const bool bUseAuthoredEnvironment = true;
    if (!bUseAuthoredEnvironment)
    {
        // 中心广场（港长所在，玩家出生点附近）
        SpawnProp(FVector(0.f, 0.f, -2.f), FVector(11.f, 11.f, 0.16f));
        SpawnProp(FVector(0.f, 0.f, 2.f), FVector(0.7f, 0.7f, 0.9f));
        SpawnProp(FVector(0.f, 0.f, 70.f), FVector(0.52f, 0.52f, 0.14f));

        // 十字主路（连接四区 + 通向码头）
        SpawnProp(FVector(0.f, 0.f, -1.f), FVector(16.f, 1.5f, 0.08f));
        SpawnProp(FVector(0.f, 0.f, -1.f), FVector(1.5f, 16.f, 0.08f));

        // 北侧市集区（篝火 + 摊位）
        SpawnProp(FVector(0.f, 900.f, -2.f), FVector(8.f, 6.5f, 0.12f));
        SpawnProp(FVector(0.f, 900.f, 0.f), FVector(2.0f, 1.4f, 0.4f));

        // 南侧矿区入口（岩壁 + 拱形洞口 + 两块水晶）
        SpawnProp(FVector(0.f, -900.f, -2.f), FVector(8.f, 6.5f, 0.12f));
        SpawnProp(FVector(0.f, -1250.f, -10.f), FVector(5.6f, 2.8f, 2.8f));
        SpawnProp(FVector(0.f, -1380.f, 120.f), FVector(3.2f, 0.44f, 2.0f));
        SpawnProp(FVector(-440.f, -1250.f, 12.f), FVector(0.66f, 0.66f, 1.2f));
        SpawnProp(FVector(440.f, -1250.f, 12.f), FVector(0.66f, 0.66f, 1.2f));

        // 西侧药棚区（草药摊 + 围栏）
        SpawnProp(FVector(-900.f, 0.f, -2.f), FVector(6.5f, 8.f, 0.12f));
        SpawnProp(FVector(-900.f, 0.f, 0.f), FVector(2.2f, 1.2f, 0.44f));
    for (int32 i = 0; i < 6; ++i)                                            // 围栏桩
    {
        const float T = (float)i / 5.f - 0.5f;
        SpawnProp(FVector(-900.f + T * 600.f, -420.f, 2.f), FVector(0.3f, 0.3f, 0.4f));
        SpawnProp(FVector(-900.f + T * 600.f, 420.f, 2.f), FVector(0.3f, 0.3f, 0.4f));
    }

    // 东侧铁匠铺（砧台 + 后墙）
    SpawnProp(FVector(900.f, 0.f, -2.f), FVector(6.5f, 8.f, 0.12f));
    SpawnProp(FVector(900.f, 0.f, 0.f), FVector(2.4f, 1.4f, 0.48f));         // 砧台
    SpawnProp(FVector(900.f, 420.f, 0.f), FVector(5.2f, 0.3f, 1.4f));        // 铺子后墙

    // 东侧外海水域（水面 + 沙滩岸线 + 木栈桥）
    SpawnProp(FVector(1950.f, 0.f, -4.f), FVector(9.f, 14.f, 0.10f));        // 海面
    SpawnProp(FVector(1500.f, 0.f, -2.f), FVector(3.5f, 14.f, 0.12f));       // 沙滩
    for (int32 i = 0; i < 7; ++i)                                            // 伸入水中的木栈道
    {
        SpawnProp(FVector(1550.f + i * 90.f, 0.f, 0.f), FVector(0.7f, 2.6f, 0.08f));
    }
    SpawnProp(FVector(1850.f, 0.f, 0.f), FVector(0.4f, 0.4f, 2.2f));         // 码头灯柱
    }

    // 四角灯笼（界定小镇范围）
    SpawnLantern(FVector(-1250.f, -1250.f, 260.f), FLinearColor(1.f, .55f, .20f), 10000.f);
    SpawnLantern(FVector(1250.f, -1250.f, 260.f), FLinearColor(1.f, .55f, .20f), 10000.f);
    SpawnLantern(FVector(-1250.f, 1250.f, 260.f), FLinearColor(1.f, .55f, .20f), 10000.f);
    SpawnLantern(FVector(1250.f, 1250.f, 260.f), FLinearColor(1.f, .55f, .20f), 10000.f);
    // 重点区域灯笼
    SpawnLantern(FVector(0.f, 900.f, 240.f), FLinearColor(1.f, .62f, .24f), 9000.f);         // 市集
    SpawnLantern(FVector(0.f, -1250.f, 240.f), FLinearColor(.9f, .2f, .85f), 11000.f);       // 矿洞入口
    SpawnLantern(FVector(1850.f, 0.f, 240.f), FLinearColor(.35f, .75f, 1.f), 9000.f);        // 码头

    // NPC 的名称、交互距离、对话 ID 均来自 ToolGen/NPC；当前关卡仅保留位置锚点。
    SpawnNpc(TEXT("NPC_01"), TEXT("村长"), FVector(0.f, 0.f, 0.f), FLinearColor(.25f, .65f, 1.f));
    SpawnNpc(TEXT("NPC_apothecary"), TEXT("药师"), FVector(-900.f, 0.f, 0.f), FLinearColor(.35f, 1.f, .45f));
    SpawnNpc(TEXT("NPC_blacksmith"), TEXT("铁匠"), FVector(900.f, 0.f, 0.f), FLinearColor(1.f, .45f, .16f));
    SpawnNpc(TEXT("NPC_02"), TEXT("商人"), FVector(0.f, 900.f, 0.f), FLinearColor(1.f, .78f, .22f));

    // 玩家出生与移动平面由 TypeScript 从 WorldSceneDefinition 读取并通过 3C Adapter 执行。
}

ARpgDemoNpc* ARpgDemoGameMode::FindNearestInteractable() const
{
    APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!Player) return nullptr;

    ARpgDemoNpc* Nearest = nullptr;
    float BestDistanceSquared = TNumericLimits<float>::Max();
    for (TActorIterator<ARpgDemoNpc> It(GetWorld()); It; ++It)
    {
        const float DistanceSquared = FVector::DistSquared(Player->GetActorLocation(), It->GetActorLocation());
        if (DistanceSquared <= FMath::Square(It->InteractRadius) && DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            Nearest = *It;
        }
    }
    return Nearest;
}

URpgDemoToolGenSubsystem* ARpgDemoGameMode::GetToolGen() const
{
    return GetGameInstance() ? GetGameInstance()->GetSubsystem<URpgDemoToolGenSubsystem>() : nullptr;
}

int32 ARpgDemoGameMode::GetQuestStep(const FString& QuestId) const
{
    if (const int32* Step = QuestSteps.Find(QuestId)) return *Step;
    return 0;
}

bool ARpgDemoGameMode::IsQuestActive(const FString& QuestId) const
{
    return ActiveQuestId == QuestId && GetQuestStep(QuestId) > 0 && !IsQuestCompleted(QuestId);
}

bool ARpgDemoGameMode::IsQuestCompleted(const FString& QuestId) const
{
    const FRpgToolGenQuest* Quest = GetToolGen() ? GetToolGen()->FindQuest(QuestId) : nullptr;
    return Quest && GetQuestStep(QuestId) > Quest->Steps.Num();
}

void ARpgDemoGameMode::AddItem(const FString& ItemId, int32 Count)
{
    if (ItemId.IsEmpty() || Count <= 0) return;
    Inventory.FindOrAdd(ItemId) += Count;
    const FString Name = GetToolGen() ? GetToolGen()->GetItemName(ItemId) : ItemId;
    SetToast(FString::Printf(TEXT("获得：%s ×%d"), *Name, Count));
}

void ARpgDemoGameMode::StartQuest(const FString& QuestId)
{
    const FRpgToolGenQuest* Quest = GetToolGen() ? GetToolGen()->FindQuest(QuestId) : nullptr;
    if (!Quest) { SetToast(FString::Printf(TEXT("任务配置不存在：%s"), *QuestId)); return; }
    if (IsQuestActive(QuestId) || IsQuestCompleted(QuestId)) return;

    ActiveQuestId = QuestId;
    QuestSteps.Add(QuestId, 1);
    QuestState = ERpgDemoQuestState::Active;
    SetToast(FString::Printf(TEXT("已接受主线任务：%s"), *Quest->Name));
    UpdateObjective();
    SaveToSlot(0);
}

void ARpgDemoGameMode::AdvanceQuest(const FString& QuestId)
{
    const FRpgToolGenQuest* Quest = GetToolGen() ? GetToolGen()->FindQuest(QuestId) : nullptr;
    if (!Quest || !IsQuestActive(QuestId)) return;

    const int32 NextStep = GetQuestStep(QuestId) + 1;
    QuestSteps.Add(QuestId, NextStep);
    if (NextStep > Quest->Steps.Num())
    {
        Gold += Quest->RewardGold;
        for (const TPair<FString, int32>& Reward : Quest->RewardItems) AddItem(Reward.Key, Reward.Value);
        QuestState = ERpgDemoQuestState::Completed;
        SetToast(FString::Printf(TEXT("任务完成：%s   获得 %d 金币 / %d 经验"), *Quest->Name, Quest->RewardGold, Quest->RewardExperience));
    }
    else
    {
        SetToast(FString::Printf(TEXT("任务推进：%s"), *Quest->Steps[NextStep - 1].Description));
    }
    UpdateObjective();
    SaveToSlot(0);
}

bool ARpgDemoGameMode::DoesDialogueConditionPass(const FString& Kind, const FString& Key, int32 Value) const
{
    if (Kind == TEXT("questStep")) return GetQuestStep(Key) == Value;
    if (Kind == TEXT("questActive")) return IsQuestActive(Key);
    if (Kind == TEXT("questCompleted")) return IsQuestCompleted(Key);
    return false;
}

void ARpgDemoGameMode::ExecuteDialogueAction(const FRpgToolGenDialogueNode& Node)
{
    if (Node.Action == TEXT("startQuest")) StartQuest(Node.QuestId);
    else if (Node.Action == TEXT("advanceQuest")) AdvanceQuest(Node.QuestId);
    else if (Node.Action == TEXT("giveItem")) AddItem(Node.ItemId, Node.Count);
}

void ARpgDemoGameMode::StartDialogue(const FString& DialogueId)
{
    const FRpgToolGenDialogue* Dialogue = GetToolGen() ? GetToolGen()->FindDialogue(DialogueId) : nullptr;
    if (!Dialogue) { SetToast(FString::Printf(TEXT("对话配置不存在：%s"), *DialogueId)); return; }
    ActiveDialogueId = DialogueId;
    ActiveDialogueNodeId.Empty();
    DialogueChoiceIndex = 0;
    bDialogueActive = true;
    ResolveDialogueNode(Dialogue->StartNodeId);
}

void ARpgDemoGameMode::ResolveDialogueNode(const FString& NodeId)
{
    const FRpgToolGenDialogue* Dialogue = GetToolGen() ? GetToolGen()->FindDialogue(ActiveDialogueId) : nullptr;
    const FRpgToolGenDialogueNode* Node = Dialogue ? Dialogue->Nodes.Find(NodeId) : nullptr;
    if (!Node)
    {
        bDialogueActive = false;
        ActiveDialogueId.Empty();
        ActiveDialogueNodeId.Empty();
        if (ARpgDemoHUD* Hud = GetDemoHud(this)) Hud->CloseOverlay();
        return;
    }

    if (Node->Type == TEXT("condition"))
    {
        for (const FRpgToolGenDialogueBranch& Branch : Node->Branches)
        {
            if (DoesDialogueConditionPass(Branch.Kind, Branch.Key, Branch.Value)) { ResolveDialogueNode(Branch.Next); return; }
        }
        ResolveDialogueNode(Node->ElseNext);
        return;
    }

    if (Node->Type == TEXT("action"))
    {
        ExecuteDialogueAction(*Node);
        ResolveDialogueNode(Node->Next);
        return;
    }

    ActiveDialogueNodeId = Node->Id;
    if (Node->Type == TEXT("choice"))
    {
        DialogueChoiceIndex = 0;
        RefreshDialogueChoice(*Node);
        return;
    }
    SetDialogue(Node->Speaker, Node->Text);
}

void ARpgDemoGameMode::RefreshDialogueChoice(const FRpgToolGenDialogueNode& Node)
{
    FString ChoiceText = Node.Text;
    for (int32 Index = 0; Index < Node.Options.Num(); ++Index)
    {
        ChoiceText += FString::Printf(TEXT("\n%s [%d] %s"), Index == DialogueChoiceIndex ? TEXT("▶") : TEXT(" "), Index + 1, *Node.Options[Index].Label);
    }
    ChoiceText += TEXT("\n←→ 选择   [E] 确认");
    SetDialogue(TEXT("选择"), ChoiceText);
}

void ARpgDemoGameMode::ContinueDialogue()
{
    if (!bDialogueActive) return;
    const FRpgToolGenDialogue* Dialogue = GetToolGen() ? GetToolGen()->FindDialogue(ActiveDialogueId) : nullptr;
    const FRpgToolGenDialogueNode* Node = Dialogue ? Dialogue->Nodes.Find(ActiveDialogueNodeId) : nullptr;
    if (!Node) { ResolveDialogueNode(FString()); return; }
    if (Node->Type == TEXT("choice"))
    {
        if (Node->Options.Num() == 0) { ResolveDialogueNode(FString()); return; }
        ResolveDialogueNode(Node->Options[FMath::Clamp(DialogueChoiceIndex, 0, Node->Options.Num() - 1)].Next);
        return;
    }
    ResolveDialogueNode(Node->Next);
}

void ARpgDemoGameMode::HandleInteract()
{
    if (bBattleActive)
    {
        SetToast(TEXT("战斗中请选择 [1] 普攻、[2] 技能或 [3] 道具。"));
        return;
    }
    if (bDialogueActive)
    {
        ContinueDialogue();
        return;
    }

    ARpgDemoNpc* Npc = FindNearestInteractable();
    if (!Npc)
    {
        SetToast(TEXT("附近没有可交互对象。"));
        return;
    }

    const FRpgToolGenNpc* Definition = GetToolGen() ? GetToolGen()->FindNpc(Npc->Id) : nullptr;
    if (!Definition || Definition->DialogueId.IsEmpty())
    {
        SetToast(TEXT("该 NPC 尚未配置对话。"));
        return;
    }
    StartDialogue(Definition->DialogueId);
}

void ARpgDemoGameMode::StartBattle()
{
    bBattleActive = true;
    EnemyHealth = 120;
    PlayerHealth = 100;
    PlayerMana = 30;
    PotionCount = 2;
    if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0)) Controller->SetIgnoreMoveInput(true);
    SetToast(TEXT("遭遇战触发：晶核史莱姆"));
    RefreshBattleHud();
}

void ARpgDemoGameMode::ResolveBattleTurn(int32 Damage, const FString& ActionLabel)
{
    if (!bBattleActive) return;
    EnemyHealth = FMath::Max(0, EnemyHealth - Damage);
    if (EnemyHealth == 0)
    {
        EndBattle();
        return;
    }
    PlayerHealth = FMath::Max(0, PlayerHealth - 12);
    SetToast(FString::Printf(TEXT("%s造成 %d 点伤害；晶核史莱姆反击 12 点。"), *ActionLabel, Damage));
    if (PlayerHealth == 0)
    {
        PlayerHealth = 100;
        EnemyHealth = 120;
        SetToast(TEXT("演示保护：你被击退，战斗自动重置。"));
    }
    RefreshBattleHud();
}

void ARpgDemoGameMode::DebugAdvanceDemo(int32 Step)
{
    if (bBattleActive || bSystemMenuOpen) return;
    const FString QuestId = TEXT("QST_mine_trouble");
    if (Step == 1)
    {
        StartQuest(QuestId);
    }
    else if (Step == 2)
    {
        if (!IsQuestActive(QuestId)) StartQuest(QuestId);
        while (GetQuestStep(QuestId) < 3) AdvanceQuest(QuestId);
    }
    else if (Step == 3)
    {
        if (!IsQuestActive(QuestId)) StartQuest(QuestId);
        StartBattle();
    }
    UpdateObjective();
}

void ARpgDemoGameMode::HandleBattleAttack()
{
    if (bBattleActive) ResolveBattleTurn(26, TEXT("普攻"));
}

void ARpgDemoGameMode::HandleBattleSkill()
{
    if (!bBattleActive) return;
    if (PlayerMana < 10)
    {
        SetToast(TEXT("MP 不足，无法施放星火斩。"));
        return;
    }
    PlayerMana -= 10;
    ResolveBattleTurn(48, TEXT("星火斩"));
}

void ARpgDemoGameMode::HandleBattlePotion()
{
    if (!bBattleActive) return;
    if (PotionCount <= 0)
    {
        SetToast(TEXT("治疗药已用完。"));
        return;
    }
    --PotionCount;
    PlayerHealth = FMath::Min(100, PlayerHealth + 36);
    SetToast(TEXT("使用治疗药，恢复 36 HP。"));
    RefreshBattleHud();
}

void ARpgDemoGameMode::EndBattle()
{
    bBattleActive = false;
    QuestState = ERpgDemoQuestState::BattleWon;
    if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0)) Controller->SetIgnoreMoveInput(false);
    if (ARpgDemoHUD* Hud = GetDemoHud(this)) Hud->CloseOverlay();
    SetToast(TEXT("战斗胜利！净化晶核后，返回港长艾琳处交付任务。"));
    SaveToSlot(0);
    UpdateObjective();
}

void ARpgDemoGameMode::UpdateObjective()
{
    ARpgDemoHUD* Hud = GetDemoHud(this);
    if (!Hud) return;

    const FRpgToolGenQuest* Quest = !ActiveQuestId.IsEmpty() && GetToolGen() ? GetToolGen()->FindQuest(ActiveQuestId) : nullptr;
    if (!Quest)
    {
        Hud->SetObjective(TEXT("与村长交谈，了解雾港的近况"));
        return;
    }

    const int32 Step = GetQuestStep(Quest->Id);
    if (Step > 0 && Step <= Quest->Steps.Num())
    {
        Hud->SetObjective(FString::Printf(TEXT("[%s] %s"), *Quest->Name, *Quest->Steps[Step - 1].Description));
    }
    else if (IsQuestCompleted(Quest->Id))
    {
        Hud->SetObjective(FString::Printf(TEXT("已完成：%s"), *Quest->Name));
    }
    else
    {
        Hud->SetObjective(FString::Printf(TEXT("任务状态异常：%s"), *Quest->Name));
    }
}

void ARpgDemoGameMode::SetDialogue(const FString& Speaker, const FString& Text)
{
    if (ARpgDemoHUD* Hud = GetDemoHud(this)) Hud->ShowDialogue(Speaker, Text);
}

void ARpgDemoGameMode::SetToast(const FString& Text)
{
    if (ARpgDemoHUD* Hud = GetDemoHud(this)) Hud->SetToast(Text);
}

void ARpgDemoGameMode::RefreshBattleHud()
{
    if (ARpgDemoHUD* Hud = GetDemoHud(this)) Hud->ShowBattle(PlayerHealth, PlayerMana, EnemyHealth, PotionCount);
}

FString ARpgDemoGameMode::SlotName(int32 SlotIndex) const
{
    return SlotIndex <= 0 ? TEXT("RpgDemoAuto") : FString::Printf(TEXT("RpgDemoSlot_%d"), SlotIndex);
}

FString ARpgDemoGameMode::BuildSaveSummary() const
{
    const FRpgToolGenQuest* Quest = !ActiveQuestId.IsEmpty() && GetToolGen() ? GetToolGen()->FindQuest(ActiveQuestId) : nullptr;
    if (!Quest) return TEXT("自由探索");
    if (IsQuestCompleted(Quest->Id)) return FString::Printf(TEXT("%s · 已完成"), *Quest->Name);
    const int32 Step = GetQuestStep(Quest->Id);
    const FString StepText = Quest->Steps.IsValidIndex(Step - 1) ? Quest->Steps[Step - 1].Description : TEXT("任务状态异常");
    return FString::Printf(TEXT("%s · %s"), *Quest->Name, *StepText);
}

TArray<FString> ARpgDemoGameMode::BuildSlotLines(bool bForSave) const
{
    TArray<FString> Lines;
    const int32 FirstSlot = bForSave ? 1 : 0;
    for (int32 Slot = FirstSlot; Slot <= 3; ++Slot)
    {
        URpgDemoSaveGame* Save = Cast<URpgDemoSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName(Slot), 0));
        const FString Prefix = Slot == 0 ? TEXT("自动存档") : FString::Printf(TEXT("存档 %d"), Slot);
        Lines.Add(Save ? FString::Printf(TEXT("%s  %s  %dG  %s"), *Prefix, *Save->Summary, Save->Gold, *Save->SavedAtText) : FString::Printf(TEXT("%s  — 空 —"), *Prefix));
    }
    return Lines;
}

void ARpgDemoGameMode::RefreshSystemMenu()
{
    if (ARpgDemoHUD* Hud = GetDemoHud(this))
    {
        const bool bSavePage = SystemMenuTab == 3;
        const int32 SlotCount = bSavePage ? 3 : 4;
        SystemMenuSlot = FMath::Clamp(SystemMenuSlot, 0, SlotCount - 1);

        TArray<FString> InventoryLines;
        for (const TPair<FString, int32>& Entry : Inventory)
        {
            const FString Name = GetToolGen() ? GetToolGen()->GetItemName(Entry.Key) : Entry.Key;
            InventoryLines.Add(FString::Printf(TEXT("%s × %d"), *Name, Entry.Value));
        }
        InventoryLines.Sort();
        Hud->SetSystemMenu(bSystemMenuOpen, SystemMenuTab, SystemMenuSlot, (SystemMenuTab >= 3) ? BuildSlotLines(bSavePage) : TArray<FString>(), Gold, BuildSaveSummary(), InventoryLines, PlayerHealth, PlayerMana);
    }
}

void ARpgDemoGameMode::ToggleSystemMenu()
{
    if (bBattleActive) { SetToast(TEXT("战斗中不能打开系统菜单。")); return; }
    bSystemMenuOpen = !bSystemMenuOpen;
    if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0)) Controller->SetIgnoreMoveInput(bSystemMenuOpen);
    RefreshSystemMenu();
}

void ARpgDemoGameMode::NavigateSystemMenu(int32 Direction)
{
    if (bDialogueActive)
    {
        const FRpgToolGenDialogue* Dialogue = GetToolGen() ? GetToolGen()->FindDialogue(ActiveDialogueId) : nullptr;
        const FRpgToolGenDialogueNode* Node = Dialogue ? Dialogue->Nodes.Find(ActiveDialogueNodeId) : nullptr;
        if (Node && Node->Type == TEXT("choice") && Node->Options.Num() > 0 && (Direction == -1 || Direction == 1))
        {
            DialogueChoiceIndex = (DialogueChoiceIndex + Direction + Node->Options.Num()) % Node->Options.Num();
            RefreshDialogueChoice(*Node);
        }
        return;
    }

    if (!bSystemMenuOpen) return;
    if (Direction == -1 || Direction == 1)
    {
        SystemMenuTab = (SystemMenuTab + Direction + 5) % 5;
        SystemMenuSlot = 0;
    }
    else if (SystemMenuTab >= 3)
    {
        const int32 Count = SystemMenuTab == 3 ? 3 : 4;
        SystemMenuSlot = (SystemMenuSlot + (Direction > 0 ? 1 : -1) + Count) % Count;
    }
    RefreshSystemMenu();
}

void ARpgDemoGameMode::SaveToSlot(int32 SlotIndex)
{
    URpgDemoSaveGame* Save = Cast<URpgDemoSaveGame>(UGameplayStatics::CreateSaveGameObject(URpgDemoSaveGame::StaticClass()));
    if (!Save) return;
    Save->SavedAtText = FDateTime::Now().ToString(TEXT("MM-dd HH:mm"));
    Save->Summary = BuildSaveSummary();
    Save->ActiveQuestId = ActiveQuestId;
    Save->QuestSteps = QuestSteps;
    Save->Inventory = Inventory;
    Save->QuestState = static_cast<int32>(QuestState);
    Save->bHasIncense = bHasIncense;
    Save->PlayerHealth = PlayerHealth;
    Save->PlayerMana = PlayerMana;
    Save->PotionCount = PotionCount;
    Save->Gold = Gold;
    if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
    {
        Save->PlayerLocation = Pawn->GetActorLocation();
        Save->PlayerRotation = Pawn->GetActorRotation();
    }
    const bool bSaved = UGameplayStatics::SaveGameToSlot(Save, SlotName(SlotIndex), 0);
    SetToast(bSaved ? FString::Printf(TEXT("已保存到 %s。"), *SlotName(SlotIndex)) : TEXT("存档失败。"));
    UE_LOG(LogTemp, Display, TEXT("[RpgDemo][Save] slot=%d result=%s summary=%s"), SlotIndex, bSaved ? TEXT("OK") : TEXT("FAILED"), *Save->Summary);
}

bool ARpgDemoGameMode::LoadFromSlot(int32 SlotIndex)
{
    URpgDemoSaveGame* Save = Cast<URpgDemoSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName(SlotIndex), 0));
    if (!Save) { SetToast(TEXT("该存档位为空。")); return false; }
    QuestState = static_cast<ERpgDemoQuestState>(FMath::Clamp(Save->QuestState, 0, 3));
    ActiveQuestId = Save->ActiveQuestId;
    QuestSteps = Save->QuestSteps;
    Inventory = Save->Inventory;
    bHasIncense = Save->bHasIncense;
    PlayerHealth = Save->PlayerHealth;
    PlayerMana = Save->PlayerMana;
    PotionCount = Save->PotionCount;
    Gold = Save->Gold;
    bBattleActive = false;
    if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0)) Pawn->SetActorLocationAndRotation(Save->PlayerLocation, Save->PlayerRotation, false, nullptr, ETeleportType::TeleportPhysics);
    if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0)) Controller->SetIgnoreMoveInput(false);
    bSystemMenuOpen = false;
    if (ARpgDemoHUD* Hud = GetDemoHud(this)) Hud->CloseOverlay();
    UpdateObjective();
    SetToast(FString::Printf(TEXT("已读取 %s：%s"), *SlotName(SlotIndex), *Save->Summary));
    UE_LOG(LogTemp, Display, TEXT("[RpgDemo][Save] load slot=%d result=OK summary=%s"), SlotIndex, *Save->Summary);
    return true;
}

void ARpgDemoGameMode::ExecuteSystemMenuAction()
{
    if (!bSystemMenuOpen) return;
    if (SystemMenuTab == 3) SaveToSlot(SystemMenuSlot + 1);
    else if (SystemMenuTab == 4) LoadFromSlot(SystemMenuSlot);
    else { SetToast(TEXT("使用 ← → 切换到存档或读档页。")); }
    RefreshSystemMenu();
}

void ARpgDemoGameMode::DeleteSelectedSave()
{
    if (!bSystemMenuOpen || SystemMenuTab < 3) return;
    const int32 Slot = SystemMenuTab == 3 ? SystemMenuSlot + 1 : SystemMenuSlot;
    const bool bDeleted = UGameplayStatics::DeleteGameInSlot(SlotName(Slot), 0);
    SetToast(bDeleted ? FString::Printf(TEXT("已删除 %s。"), *SlotName(Slot)) : TEXT("该存档位为空，无法删除。"));
    RefreshSystemMenu();
}

void ARpgDemoGameMode::ResetDemo()
{
    QuestState = ERpgDemoQuestState::NotStarted;
    ActiveQuestId.Empty();
    QuestSteps.Empty();
    Inventory.Empty();
    bDialogueActive = false;
    ActiveDialogueId.Empty();
    ActiveDialogueNodeId.Empty();
    bHasIncense = false;
    bBattleActive = false;
    PlayerHealth = 100;
    PlayerMana = 30;
    EnemyHealth = 120;
    PotionCount = 2;
    Gold = 0;
    if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
    {
        Controller->SetIgnoreMoveInput(false);
        if (APawn* Pawn = Controller->GetPawn()) Pawn->SetActorLocation(FVector::ZeroVector);
    }
    if (ARpgDemoHUD* Hud = GetDemoHud(this)) Hud->CloseOverlay();
    UpdateObjective();
    SetToast(TEXT("Demo 已重置。前往港长艾琳重新开始。"));
}

void ARpgDemoGameMode::ValidateDemoData() const
{
    const TArray<FString> RequiredFiles =
    {
        FPaths::ProjectContentDir() / TEXT("Data/ToolGen/Quest/QST_mine_trouble.json"),
        FPaths::ProjectContentDir() / TEXT("Data/ToolGen/Dialogue/DLG_blacksmith.json"),
        FPaths::ProjectContentDir() / TEXT("Data/ToolGen/Battle/Encounter/BE_dfa1a10adcac4421.json")
    };
    for (const FString& File : RequiredFiles)
    {
        UE_LOG(LogTemp, Display, TEXT("[RpgDemo][AGMaker] %s: %s"), *File, FPaths::FileExists(File) ? TEXT("OK") : TEXT("MISSING"));
    }
}
