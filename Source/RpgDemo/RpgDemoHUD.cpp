#include "RpgDemoHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"

void ARpgDemoHUD::SetObjective(const FString& InObjective)
{
    Objective = InObjective;
}

void ARpgDemoHUD::SetToast(const FString& InToast)
{
    Toast = InToast;
}

void ARpgDemoHUD::ShowDialogue(const FString& Speaker, const FString& Text)
{
    DialogueSpeaker = Speaker;
    DialogueText = Text;
    bDialogueOpen = true;
    bBattleOpen = false;
}

void ARpgDemoHUD::ShowBattle(int32 PlayerHealth, int32 PlayerMana, int32 EnemyHealth, int32 PotionCount)
{
    DisplayPlayerHealth = PlayerHealth;
    DisplayPlayerMana = PlayerMana;
    DisplayEnemyHealth = EnemyHealth;
    DisplayPotionCount = PotionCount;
    bBattleOpen = true;
    bDialogueOpen = false;
}

void ARpgDemoHUD::SetSystemMenu(bool bOpen, int32 InTab, int32 InSlot, const TArray<FString>& InSlotLines, int32 InGold, const FString& InQuestSummary, const TArray<FString>& InInventoryLines, int32 InPlayerHealth, int32 InPlayerMana)
{
    bSystemMenuOpen = bOpen;
    SystemMenuTab = InTab;
    SystemMenuSlot = InSlot;
    SystemMenuSlotLines = InSlotLines;
    SystemMenuGold = InGold;
    SystemMenuQuestSummary = InQuestSummary;
    SystemMenuInventoryLines = InInventoryLines;
    SystemMenuPlayerHealth = InPlayerHealth;
    SystemMenuPlayerMana = InPlayerMana;
}

void ARpgDemoHUD::CloseOverlay()
{
    bDialogueOpen = false;
    bBattleOpen = false;
}

void ARpgDemoHUD::DrawSystemMenu(float Width, float Height, UFont* Font)
{
    const float PanelX = Width * .12f;
    const float PanelY = Height * .10f;
    const float PanelW = Width * .76f;
    const float PanelH = Height * .80f;
    const TArray<FString> Tabs = { TEXT("状态"), TEXT("背包"), TEXT("任务"), TEXT("存档"), TEXT("读档") };
    DrawRect(FLinearColor(.015f, .02f, .05f, .97f), PanelX, PanelY, PanelW, PanelH);
    DrawText(TEXT("系统菜单"), FLinearColor(1.f, .80f, .28f), PanelX + 34.f, PanelY + 26.f, Font, 1.45f, false);
    DrawText(TEXT("Esc 关闭   ←→ 切换页签   ↑↓ 选择存档位   Enter 执行"), FLinearColor(.55f, .65f, .85f), PanelX + 34.f, PanelY + 58.f, Font, .82f, false);

    float TabX = PanelX + 34.f;
    for (int32 Index = 0; Index < Tabs.Num(); ++Index)
    {
        const bool bSelected = Index == SystemMenuTab;
        DrawRect(bSelected ? FLinearColor(.18f, .34f, .74f, 1.f) : FLinearColor(.08f, .11f, .20f, 1.f), TabX, PanelY + 92.f, 126.f, 38.f);
        DrawText(Tabs[Index], bSelected ? FLinearColor::White : FLinearColor(.66f, .70f, .82f), TabX + 34.f, PanelY + 102.f, Font, 1.f, false);
        TabX += 134.f;
    }

    const float ContentX = PanelX + 48.f;
    float ContentY = PanelY + 160.f;
    const FLinearColor LabelColor(.64f, .70f, .82f);
    const FLinearColor ValueColor(.95f, .94f, .86f);
    if (SystemMenuTab == 0)
    {
        DrawText(TEXT("雾港旅人"), FLinearColor(1.f, .80f, .28f), ContentX, ContentY, Font, 1.35f, false); ContentY += 42.f;
        DrawText(FString::Printf(TEXT("HP  %d / 100"), SystemMenuPlayerHealth), ValueColor, ContentX, ContentY, Font, 1.1f, false); ContentY += 32.f;
        DrawText(FString::Printf(TEXT("MP  %d / 30"), SystemMenuPlayerMana), ValueColor, ContentX, ContentY, Font, 1.1f, false); ContentY += 32.f;
        DrawText(FString::Printf(TEXT("金币  %d G"), SystemMenuGold), ValueColor, ContentX, ContentY, Font, 1.1f, false); ContentY += 50.f;
        DrawText(TEXT("Demo 复刻：任务 / 对话 / 回合制战斗 / 多槽存档"), LabelColor, ContentX, ContentY, Font, .98f, false);
    }
    else if (SystemMenuTab == 1)
    {
        DrawText(TEXT("背包"), FLinearColor(1.f, .80f, .28f), ContentX, ContentY, Font, 1.35f, false); ContentY += 46.f;
        if (SystemMenuInventoryLines.IsEmpty())
        {
            DrawText(TEXT("背包为空"), LabelColor, ContentX, ContentY, Font, 1.02f, false);
        }
        else
        {
            for (const FString& Line : SystemMenuInventoryLines)
            {
                DrawText(Line, ValueColor, ContentX, ContentY, Font, 1.05f, false);
                ContentY += 32.f;
            }
        }
    }
    else if (SystemMenuTab == 2)
    {
        DrawText(TEXT("任务"), FLinearColor(1.f, .80f, .28f), ContentX, ContentY, Font, 1.35f, false); ContentY += 46.f;
        DrawText(SystemMenuQuestSummary.IsEmpty() ? TEXT("当前没有进行中的任务") : SystemMenuQuestSummary, ValueColor, ContentX, ContentY, Font, 1.02f, false);
    }
    else
    {
        const bool bSave = SystemMenuTab == 3;
        DrawText(bSave ? TEXT("存档 — 手动存档位") : TEXT("读档 — 自动与手动存档位"), FLinearColor(1.f, .80f, .28f), ContentX, ContentY, Font, 1.35f, false); ContentY += 44.f;
        for (int32 Index = 0; Index < SystemMenuSlotLines.Num(); ++Index)
        {
            const bool bSelected = Index == SystemMenuSlot;
            DrawText(FString::Printf(TEXT("%s%s"), bSelected ? TEXT("▶ ") : TEXT("   "), *SystemMenuSlotLines[Index]), bSelected ? FLinearColor(1.f, .82f, .30f) : ValueColor, ContentX, ContentY, Font, bSelected ? 1.08f : 1.f, false);
            ContentY += 34.f;
        }
        DrawText(bSave ? TEXT("Enter 保存   Delete 删除   Esc 返回") : TEXT("Enter 读取   Delete 删除   Esc 返回"), LabelColor, ContentX, ContentY + 22.f, Font, .92f, false);
    }
}

void ARpgDemoHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas || !GEngine) return;

    UFont* Font = GEngine->GetSmallFont();
    const float Width = Canvas->SizeX;
    const float Height = Canvas->SizeY;
    DrawRect(FLinearColor(0.025f, 0.035f, 0.08f, .86f), 20.f, 18.f, 460.f, 94.f);
    DrawText(TEXT("RPG DEMO · 雾港余烬"), FLinearColor(1.f, .82f, .28f), 38.f, 32.f, Font, 1.35f, false);
    DrawText(TEXT("主线任务"), FLinearColor(.45f, .75f, 1.f), 38.f, 60.f, Font, .95f, false);
    DrawText(Objective, FLinearColor::White, 38.f, 80.f, Font, .92f, false);

    if (!Toast.IsEmpty())
    {
        DrawRect(FLinearColor(0.f, 0.f, 0.f, .70f), Width * .5f - 220.f, 130.f, 440.f, 42.f);
        DrawText(Toast, FLinearColor(1.f, .92f, .55f), Width * .5f - 190.f, 143.f, Font, 1.0f, false);
    }

    if (bDialogueOpen)
    {
        DrawRect(FLinearColor(.025f, .025f, .055f, .94f), 48.f, Height - 210.f, Width - 96.f, 152.f);
        DrawText(DialogueSpeaker, FLinearColor(1.f, .78f, .27f), 78.f, Height - 188.f, Font, 1.15f, false);
        DrawText(DialogueText, FLinearColor::White, 78.f, Height - 150.f, Font, 1.05f, false);
        DrawText(TEXT("按 [E] 继续"), FLinearColor(.55f, .65f, .82f), Width - 190.f, Height - 84.f, Font, .85f, false);
    }

    if (bBattleOpen)
    {
        DrawRect(FLinearColor(.04f, .01f, .06f, .94f), 70.f, Height * .5f - 150.f, Width - 140.f, 300.f);
        DrawText(TEXT("⚔ 晶核史莱姆战斗"), FLinearColor(1.f, .58f, .42f), Width * .5f - 115.f, Height * .5f - 122.f, Font, 1.4f, false);
        DrawText(FString::Printf(TEXT("勇者  HP %d / 100    MP %d / 30"), DisplayPlayerHealth, DisplayPlayerMana), FLinearColor(.55f, .85f, 1.f), 120.f, Height * .5f - 65.f, Font, 1.1f, false);
        DrawText(FString::Printf(TEXT("晶核史莱姆  HP %d / 120"), DisplayEnemyHealth), FLinearColor(1.f, .48f, .48f), Width - 355.f, Height * .5f - 65.f, Font, 1.1f, false);
        DrawText(FString::Printf(TEXT("[1] 普攻   [2] 星火斩（10 MP）   [3] 治疗药 ×%d"), DisplayPotionCount), FLinearColor(1.f, .9f, .42f), Width * .5f - 195.f, Height * .5f + 15.f, Font, 1.12f, false);
        DrawText(TEXT("回合制演示：选择行动后敌人会反击"), FLinearColor(.72f, .72f, .86f), Width * .5f - 155.f, Height * .5f + 60.f, Font, .88f, false);
    }

    if (bSystemMenuOpen) DrawSystemMenu(Width, Height, Font);
}
