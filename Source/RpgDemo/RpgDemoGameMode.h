#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RpgDemoToolGenSubsystem.h"
#include "RpgDemoGameMode.generated.h"

class ARpgDemoNpc;

UENUM()
enum class ERpgDemoQuestState : uint8
{
    NotStarted,
    Active,
    BattleWon,
    Completed
};

UCLASS()
class RPGDEMO_API ARpgDemoGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ARpgDemoGameMode();
    virtual void BeginPlay() override;

    void HandleInteract();
    void HandleBattleAttack();
    void HandleBattleSkill();
    void HandleBattlePotion();
    void DebugAdvanceDemo(int32 Step);
    void ToggleSystemMenu();
    void NavigateSystemMenu(int32 Direction);
    void ExecuteSystemMenuAction();
    void DeleteSelectedSave();
    void ResetDemo();

private:
    void SpawnDemoWorld();
    void RefreshSystemMenu();
    void SaveToSlot(int32 SlotIndex);
    bool LoadFromSlot(int32 SlotIndex);
    FString SlotName(int32 SlotIndex) const;
    FString BuildSaveSummary() const;
    TArray<FString> BuildSlotLines(bool bForSave) const;
    void SpawnProp(const FVector& Location, const FVector& Scale);
    ARpgDemoNpc* SpawnNpc(const FString& Id, const FString& Name, const FVector& Location, const FLinearColor& Color);
    ARpgDemoNpc* FindNearestInteractable() const;

    URpgDemoToolGenSubsystem* GetToolGen() const;
    void StartDialogue(const FString& DialogueId);
    void ContinueDialogue();
    void ResolveDialogueNode(const FString& NodeId);
    void RefreshDialogueChoice(const FRpgToolGenDialogueNode& Node);
    bool DoesDialogueConditionPass(const FString& Kind, const FString& Key, int32 Value) const;
    void ExecuteDialogueAction(const FRpgToolGenDialogueNode& Node);
    void StartQuest(const FString& QuestId);
    void AdvanceQuest(const FString& QuestId);
    int32 GetQuestStep(const FString& QuestId) const;
    bool IsQuestActive(const FString& QuestId) const;
    bool IsQuestCompleted(const FString& QuestId) const;
    void AddItem(const FString& ItemId, int32 Count);

    void StartBattle();
    void ResolveBattleTurn(int32 Damage, const FString& ActionLabel);
    void EndBattle();
    void UpdateObjective();
    void SetDialogue(const FString& Speaker, const FString& Text);
    void SetToast(const FString& Text);
    void RefreshBattleHud();
    void ValidateDemoData() const;

    // 旧战斗演示暂存状态；任务与对话不再依赖这些硬编码布尔/枚举。
    ERpgDemoQuestState QuestState = ERpgDemoQuestState::NotStarted;
    bool bHasIncense = false;
    bool bBattleActive = false;
    bool bSystemMenuOpen = false;
    bool bDialogueActive = false;
    int32 SystemMenuTab = 0;
    int32 SystemMenuSlot = 0;
    int32 PlayerHealth = 100;
    int32 PlayerMana = 30;
    int32 EnemyHealth = 120;
    int32 PotionCount = 2;
    int32 Gold = 0;

    FString ActiveQuestId;
    TMap<FString, int32> QuestSteps;
    TMap<FString, int32> Inventory;
    FString ActiveDialogueId;
    FString ActiveDialogueNodeId;
    int32 DialogueChoiceIndex = 0;
};
