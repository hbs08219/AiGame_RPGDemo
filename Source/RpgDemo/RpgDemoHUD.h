#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RpgDemoHUD.generated.h"

UCLASS()
class RPGDEMO_API ARpgDemoHUD : public AHUD
{
    GENERATED_BODY()

public:
    void SetObjective(const FString& InObjective);
    void SetToast(const FString& InToast);
    void ShowDialogue(const FString& Speaker, const FString& Text);
    void ShowBattle(int32 PlayerHealth, int32 PlayerMana, int32 EnemyHealth, int32 PotionCount);
    void SetSystemMenu(bool bOpen, int32 InTab, int32 InSlot, const TArray<FString>& InSlotLines, int32 InGold, const FString& InQuestSummary, const TArray<FString>& InInventoryLines, int32 InPlayerHealth, int32 InPlayerMana);
    void CloseOverlay();
    virtual void DrawHUD() override;

private:
    void DrawSystemMenu(float Width, float Height, UFont* Font);
    FString Objective;
    FString Toast;
    FString DialogueSpeaker;
    FString DialogueText;
    bool bDialogueOpen = false;
    bool bBattleOpen = false;
    bool bSystemMenuOpen = false;
    int32 SystemMenuTab = 0;
    int32 SystemMenuSlot = 0;
    int32 SystemMenuGold = 0;
    FString SystemMenuQuestSummary;
    TArray<FString> SystemMenuInventoryLines;
    int32 SystemMenuPlayerHealth = 100;
    int32 SystemMenuPlayerMana = 30;
    TArray<FString> SystemMenuSlotLines;
    int32 DisplayPlayerHealth = 100;
    int32 DisplayPlayerMana = 30;
    int32 DisplayEnemyHealth = 100;
    int32 DisplayPotionCount = 2;
};
